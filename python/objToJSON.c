/*
Developed by ESN, an Electronic Arts Inc. studio.
Copyright (c) 2014, Electronic Arts Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ESN, Electronic Arts Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS INC. BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Portions of code from MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007  Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

Numeric decoder derived from from TCL library
http://www.opensource.apple.com/source/tcl/tcl-14/tcl/license.terms
* Copyright (c) 1988-1993 The Regents of the University of California.
* Copyright (c) 1994 Sun Microsystems, Inc.
*/

#include "py_defines.h"
#include <stdio.h>
#include <ultrajson.h>

#define EPOCH_ORD 719163
static PyObject* type_decimal = NULL;

typedef void *(*PFN_PyTypeToJSON)(JSOBJ obj, JSONTypeContext *ti, void *outValue, size_t *_outLen);

#if (PY_VERSION_HEX < 0x02050000)
typedef ssize_t Py_ssize_t;
#endif

typedef struct __TypeContext
{
  JSPFN_ITEREND iterEnd;
  JSPFN_ITERNEXT iterNext;
  JSPFN_ITERGETNAME iterGetName;
  JSPFN_ITERGETVALUE iterGetValue;
  PFN_PyTypeToJSON PyTypeToJSON;
  PyObject *newObj;
  PyObject *dictObj;
  Py_ssize_t index;
  Py_ssize_t size;
  PyObject *itemValue;
  PyObject *itemName;
  PyObject *attrList;
  PyObject *iterator;

  union
  {
    PyObject *rawJSONValue;
    JSINT64 longValue;
    JSUINT64 unsignedLongValue;
  };
} TypeContext;

#define GET_TC(__ptrtc) ((TypeContext *)((__ptrtc)->prv))

struct PyDictIterState
{
  PyObject *keys;
  size_t i;
  size_t sz;
};

//#define PRINTMARK() fprintf(stderr, "%s: MARK(%d)\n", __FILE__, __LINE__)
#define PRINTMARK()

void initObjToJSON(void)
{
  PyObject* mod_decimal = PyImport_ImportModule("decimal");
  if (mod_decimal)
  {
    type_decimal = PyObject_GetAttrString(mod_decimal, "Decimal");
    Py_INCREF(type_decimal);
    Py_DECREF(mod_decimal);
  }
  else
    PyErr_Clear();
}

#ifdef _LP64
static void *PyIntToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((JSINT64 *) outValue) = PyLong_AsLong(obj);
  return NULL;
}
#else
static void *PyIntToINT32(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((JSINT32 *) outValue) = PyLong_AsLong(obj);
  return NULL;
}
#endif

static void *PyLongToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSINT64 *) outValue) = GET_TC(tc)->longValue;
  return NULL;
}

static void *PyLongToUINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSUINT64 *) outValue) = GET_TC(tc)->unsignedLongValue;
  return NULL;
}

static void *PyFloatToDOUBLE(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((double *) outValue) = PyFloat_AsDouble (obj);
  return NULL;
}

static void *PyStringToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *_outLen = PyBytes_GET_SIZE(obj);
  return PyBytes_AS_STRING(obj);
}

static void *PyUnicodeToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  PyObject *newObj;
#if (PY_VERSION_HEX >= 0x03030000)
  if (PyUnicode_IS_COMPACT_ASCII(obj))
  {
    Py_ssize_t len;
    const char *data = PyUnicode_AsUTF8AndSize(obj, &len);
    *_outLen = len;
    return (void*)data;
  }
#endif
  newObj = PyUnicode_AsUTF8String(obj);
  if(!newObj)
  {
    return NULL;
  }

  GET_TC(tc)->newObj = newObj;

  *_outLen = PyBytes_GET_SIZE(newObj);
  return PyBytes_AS_STRING(newObj);
}

static void *PyRawJSONToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = GET_TC(tc)->rawJSONValue;
  if (PyUnicode_Check(obj))
  {
    return PyUnicodeToUTF8(obj, tc, outValue, _outLen);
  }
  else
  {
    return PyStringToUTF8(obj, tc, outValue, _outLen);
  }
}

static int Tuple_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  PyObject *item;

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    return 0;
  }

  item = PyTuple_GET_ITEM (obj, GET_TC(tc)->index);

  GET_TC(tc)->itemValue = item;
  GET_TC(tc)->index ++;
  return 1;
}

static void Tuple_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ Tuple_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *Tuple_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

static int List_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  GET_TC(tc)->itemValue = PyList_GET_ITEM (obj, GET_TC(tc)->index);
  GET_TC(tc)->index ++;
  return 1;
}

static void List_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ List_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *List_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

//=============================================================================
// Dict iteration functions
// itemName might converted to string (Python_Str). Do refCounting
// itemValue is borrowed from object (which is dict). No refCounting
//=============================================================================

static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
#if PY_MAJOR_VERSION >= 3
  PyObject* itemNameTmp;
#endif

  if (GET_TC(tc)->itemName)
  {
    Py_DECREF(GET_TC(tc)->itemName);
    GET_TC(tc)->itemName = NULL;
  }

  if (!(GET_TC(tc)->itemName = PyIter_Next(GET_TC(tc)->iterator)))
  {
    PRINTMARK();
    return 0;
  }

  if (!(GET_TC(tc)->itemValue = PyObject_GetItem(GET_TC(tc)->dictObj, GET_TC(tc)->itemName)))
  {
    PRINTMARK();
    return 0;
  }

  if (PyUnicode_Check(GET_TC(tc)->itemName))
  {
    GET_TC(tc)->itemName = PyUnicode_AsUTF8String (GET_TC(tc)->itemName);
  }
  else
  if (!PyBytes_Check(GET_TC(tc)->itemName))
  {
    if (UNLIKELY(GET_TC(tc)->itemName == Py_None))
    {
      GET_TC(tc)->itemName = PyUnicode_FromString("null");
      return 1;
    }

    GET_TC(tc)->itemName = PyObject_Str(GET_TC(tc)->itemName);
#if PY_MAJOR_VERSION >= 3
    itemNameTmp = GET_TC(tc)->itemName;
    GET_TC(tc)->itemName = PyUnicode_AsUTF8String (GET_TC(tc)->itemName);
    Py_DECREF(itemNameTmp);
#endif
  }
  else
  {
    Py_INCREF(GET_TC(tc)->itemName);
  }
  PRINTMARK();
  return 1;
}

static void Dict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  if (GET_TC(tc)->itemName)
  {
    Py_DECREF(GET_TC(tc)->itemName);
    GET_TC(tc)->itemName = NULL;
  }
  Py_CLEAR(GET_TC(tc)->iterator);
  Py_DECREF(GET_TC(tc)->dictObj);
  PRINTMARK();
}

static JSOBJ Dict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *Dict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  *outLen = PyBytes_GET_SIZE(GET_TC(tc)->itemName);
  return PyBytes_AS_STRING(GET_TC(tc)->itemName);
}

static int SortedDict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  PyObject *items = NULL, *item = NULL, *key = NULL, *value = NULL;
  Py_ssize_t i, nitems;
#if PY_MAJOR_VERSION >= 3
  PyObject* keyTmp;
#endif

  // Upon first call, obtain a list of the keys and sort them. This follows the same logic as the
  // stanard library's _json.c sort_keys handler.
  if (GET_TC(tc)->newObj == NULL)
  {
    // Obtain the list of keys from the dictionary.
    items = PyMapping_Keys(GET_TC(tc)->dictObj);
    if (items == NULL)
    {
      goto error;
    }
    else if (!PyList_Check(items))
    {
      PyErr_SetString(PyExc_ValueError, "keys must return list");
      goto error;
    }

    // Sort the list.
    if (PyList_Sort(items) < 0)
    {
      PyErr_SetString(PyExc_ValueError, "unorderable keys");
      goto error;
    }

    // Obtain the value for each key, and pack a list of (key, value) 2-tuples.
    nitems = PyList_GET_SIZE(items);
    for (i = 0; i < nitems; i++)
    {
      key = PyList_GET_ITEM(items, i);
      value = PyDict_GetItem(GET_TC(tc)->dictObj, key);

      // Subject the key to the same type restrictions and conversions as in Dict_iterGetValue.
      if (PyUnicode_Check(key))
      {
        key = PyUnicode_AsUTF8String(key);
      }
      else if (!PyBytes_Check(key))
      {
        key = PyObject_Str(key);
#if PY_MAJOR_VERSION >= 3
        keyTmp = key;
        key = PyUnicode_AsUTF8String(key);
        Py_DECREF(keyTmp);
#endif
      }
      else
      {
        Py_INCREF(key);
      }

      item = PyTuple_Pack(2, key, value);
      if (item == NULL)
      {
        goto error;
      }
      if (PyList_SetItem(items, i, item))
      {
        goto error;
      }
      Py_DECREF(key);
    }

    // Store the sorted list of tuples in the newObj slot.
    GET_TC(tc)->newObj = items;
    GET_TC(tc)->size = nitems;
  }

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  item = PyList_GET_ITEM(GET_TC(tc)->newObj, GET_TC(tc)->index);
  GET_TC(tc)->itemName = PyTuple_GET_ITEM(item, 0);
  GET_TC(tc)->itemValue = PyTuple_GET_ITEM(item, 1);
  GET_TC(tc)->index++;
  return 1;

error:
  Py_XDECREF(item);
  Py_XDECREF(key);
  Py_XDECREF(value);
  Py_XDECREF(items);
  return -1;
}

static void SortedDict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  GET_TC(tc)->itemName = NULL;
  GET_TC(tc)->itemValue = NULL;
  Py_DECREF(GET_TC(tc)->dictObj);
  PRINTMARK();
}

static JSOBJ SortedDict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *SortedDict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  *outLen = PyBytes_GET_SIZE(GET_TC(tc)->itemName);
  return PyBytes_AS_STRING(GET_TC(tc)->itemName);
}

static void SetupDictIter(PyObject *dictObj, TypeContext *pc, JSONObjectEncoder *enc)
{
  pc->dictObj = dictObj;
  if (enc->sortKeys)
  {
    pc->iterEnd = SortedDict_iterEnd;
    pc->iterNext = SortedDict_iterNext;
    pc->iterGetValue = SortedDict_iterGetValue;
    pc->iterGetName = SortedDict_iterGetName;
    pc->index = 0;
  }
  else
  {
    pc->iterEnd = Dict_iterEnd;
    pc->iterNext = Dict_iterNext;
    pc->iterGetValue = Dict_iterGetValue;
    pc->iterGetName = Dict_iterGetName;
    pc->iterator = PyObject_GetIter(dictObj);
  }
}

static void Object_beginTypeContext (JSOBJ _obj, JSONTypeContext *tc, JSONObjectEncoder *enc)
{
  PyObject *obj, *objRepr, *exc;
  TypeContext *pc;
  PRINTMARK();
  if (!_obj)
  {
    tc->type = JT_INVALID;
    return;
  }

  obj = (PyObject*) _obj;

  tc->prv = PyObject_Malloc(sizeof(TypeContext));
  pc = (TypeContext *) tc->prv;
  if (!pc)
  {
    tc->type = JT_INVALID;
    PyErr_NoMemory();
    return;
  }
  pc->newObj = NULL;
  pc->dictObj = NULL;
  pc->itemValue = NULL;
  pc->itemName = NULL;
  pc->iterator = NULL;
  pc->attrList = NULL;
  pc->index = 0;
  pc->size = 0;
  pc->longValue = 0;
  pc->rawJSONValue = NULL;

  if (PyIter_Check(obj))
  {
    PRINTMARK();
    goto ISITERABLE;
  }

  if (PyBool_Check(obj))
  {
    PRINTMARK();
    tc->type = (obj == Py_True) ? JT_TRUE : JT_FALSE;
    return;
  }
  else
  if (PyLong_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyLongToINT64;
    tc->type = JT_LONG;
    GET_TC(tc)->longValue = PyLong_AsLongLong(obj);

    exc = PyErr_Occurred();
    if (!exc)
    {
        return;
    }

    if (exc && PyErr_ExceptionMatches(PyExc_OverflowError))
    {
      PyErr_Clear();
      pc->PyTypeToJSON = PyLongToUINT64;
      tc->type = JT_ULONG;
      GET_TC(tc)->unsignedLongValue = PyLong_AsUnsignedLongLong(obj);

      exc = PyErr_Occurred();
      if (exc && PyErr_ExceptionMatches(PyExc_OverflowError))
      {
        PRINTMARK();
        goto INVALID;
      }
    }

    return;
  }
  else
  if (PyLong_Check(obj))
  {
    PRINTMARK();
#ifdef _LP64
    pc->PyTypeToJSON = PyIntToINT64; tc->type = JT_LONG;
#else
    pc->PyTypeToJSON = PyIntToINT32; tc->type = JT_INT;
#endif
    return;
  }
  else
  if (PyBytes_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyStringToUTF8; tc->type = JT_UTF8;
    return;
  }
  else
  if (PyUnicode_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyUnicodeToUTF8; tc->type = JT_UTF8;
    return;
  }
  else
  if (PyFloat_Check(obj) || (type_decimal && PyObject_IsInstance(obj, type_decimal)))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyFloatToDOUBLE; tc->type = JT_DOUBLE;
    return;
  }
  else
  if (obj == Py_None)
  {
    PRINTMARK();
    tc->type = JT_NULL;
    return;
  }

ISITERABLE:
  if (PyDict_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(obj, pc, enc);
    Py_INCREF(obj);
    return;
  }
  else
  if (PyList_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_ARRAY;
    pc->iterEnd = List_iterEnd;
    pc->iterNext = List_iterNext;
    pc->iterGetValue = List_iterGetValue;
    pc->iterGetName = List_iterGetName;
    GET_TC(tc)->index =  0;
    GET_TC(tc)->size = PyList_GET_SIZE( (PyObject *) obj);
    return;
  }
  else
  if (PyTuple_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_ARRAY;
    pc->iterEnd = Tuple_iterEnd;
    pc->iterNext = Tuple_iterNext;
    pc->iterGetValue = Tuple_iterGetValue;
    pc->iterGetName = Tuple_iterGetName;
    GET_TC(tc)->index = 0;
    GET_TC(tc)->size = PyTuple_GET_SIZE( (PyObject *) obj);
    GET_TC(tc)->itemValue = NULL;

    return;
  }

  if (UNLIKELY(PyObject_HasAttrString(obj, "toDict")))
  {
    PyObject* toDictFunc = PyObject_GetAttrString(obj, "toDict");
    PyObject* tuple = PyTuple_New(0);
    PyObject* toDictResult = PyObject_Call(toDictFunc, tuple, NULL);
    Py_DECREF(tuple);
    Py_DECREF(toDictFunc);

    if (toDictResult == NULL)
    {
      goto INVALID;
    }

    if (!PyDict_Check(toDictResult))
    {
      Py_DECREF(toDictResult);
      tc->type = JT_NULL;
      return;
    }

    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(toDictResult, pc, enc);
    return;
  }
  else
  if (UNLIKELY(PyObject_HasAttrString(obj, "__json__")))
  {
    PyObject* toJSONFunc = PyObject_GetAttrString(obj, "__json__");
    PyObject* tuple = PyTuple_New(0);
    PyObject* toJSONResult = PyObject_Call(toJSONFunc, tuple, NULL);
    Py_DECREF(tuple);
    Py_DECREF(toJSONFunc);

    if (toJSONResult == NULL)
    {
      goto INVALID;
    }

    if (PyErr_Occurred())
    {
      Py_DECREF(toJSONResult);
      goto INVALID;
    }

    if (!PyBytes_Check(toJSONResult) && !PyUnicode_Check(toJSONResult))
    {
      Py_DECREF(toJSONResult);
      PyErr_Format (PyExc_TypeError, "expected string");
      goto INVALID;
    }

    PRINTMARK();
    pc->PyTypeToJSON = PyRawJSONToUTF8;
    tc->type = JT_RAW;
    GET_TC(tc)->rawJSONValue = toJSONResult;
    return;
  }

  PRINTMARK();
  PyErr_Clear();

  objRepr = PyObject_Repr(obj);
  PyErr_Format (PyExc_TypeError, "%s is not JSON serializable", PyBytes_AS_STRING(objRepr));
  Py_DECREF(objRepr);

INVALID:
  PRINTMARK();
  tc->type = JT_INVALID;
  PyObject_Free(tc->prv);
  tc->prv = NULL;
  return;
}

static void Object_endTypeContext(JSOBJ obj, JSONTypeContext *tc)
{
  Py_XDECREF(GET_TC(tc)->newObj);

  PyObject_Free(tc->prv);
  tc->prv = NULL;
}

static const char *Object_getStringValue(JSOBJ obj, JSONTypeContext *tc, size_t *_outLen)
{
  return GET_TC(tc)->PyTypeToJSON (obj, tc, NULL, _outLen);
}

static JSINT64 Object_getLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSINT64 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static JSUINT64 Object_getUnsignedLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSUINT64 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static JSINT32 Object_getIntValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSINT32 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static double Object_getDoubleValue(JSOBJ obj, JSONTypeContext *tc)
{
  double ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static void Object_releaseObject(JSOBJ _obj)
{
  Py_DECREF( (PyObject *) _obj);
}

static int Object_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->iterNext(obj, tc);
}

static void Object_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  GET_TC(tc)->iterEnd(obj, tc);
}

static JSOBJ Object_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->iterGetValue(obj, tc);
}

static char *Object_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return GET_TC(tc)->iterGetName(obj, tc, outLen);
}


#define ENCODER_HELP_TEXT \
    "Use ensure_ascii=false to output UTF-8. Pass in double_precision to" \
    " alter the maximum digit precision of doubles. Set" \
    " encode_html_chars=True to encode < > & as unicode escape sequences." \
    " Set escape_forward_slashes=False to prevent escaping / characters."

HPyDef_METH(objToJSON, "dumps", objToJSON_impl, HPyFunc_KEYWORDS,
            .doc="Converts arbitrary object recursively into JSON. " \
                 ENCODER_HELP_TEXT);

// ujson.c does something a bit weird: it defines two Python-level methods
// ("encode" and "dumps") for the same C-level function
// ("objToJSON_impl"). HPyDef_METH does not support this use case, but we can
// define our HPyDef by hand: HPyDef_METH is just a convenience macro and the
// structure and fields of HPyDef is part of the publich API
HPyDef objToJSON_encode = {
   .kind = HPyDef_Kind_Meth,
   .meth = {
       .name = "encode",
       .impl = objToJSON_impl,
       .cpy_trampoline = objToJSON_trampoline,
       .signature = HPyFunc_KEYWORDS,
       .doc = ("Converts arbitrary object recursively into JSON. "
               ENCODER_HELP_TEXT),
   }
};

static HPy
objToJSON_impl(HPyContext *ctx, HPy self, HPy *args, HPy_ssize_t nargs, HPy kw)
{
  static const char *kwlist[] = { "obj", "ensure_ascii", "encode_html_chars", "escape_forward_slashes", "sort_keys", "indent", NULL };

  char buffer[65536];
  char *ret;
  HPy h_ret;
  HPy oinput = HPy_NULL;
  HPy oensureAscii = HPy_NULL;
  HPy oencodeHTMLChars = HPy_NULL;
  HPy oescapeForwardSlashes = HPy_NULL;
  HPy osortKeys = HPy_NULL;
  HPyTracker ht;
  PyObject *oinput_o;

  JSONObjectEncoder encoder =
  {
    Object_beginTypeContext,
    Object_endTypeContext,
    Object_getStringValue,
    Object_getLongValue,
    Object_getUnsignedLongValue,
    Object_getIntValue,
    Object_getDoubleValue,
    Object_iterNext,
    Object_iterEnd,
    Object_iterGetValue,
    Object_iterGetName,
    Object_releaseObject,
    PyObject_Malloc,
    PyObject_Realloc,
    PyObject_Free,
    -1, //recursionMax
    1, //forceAscii
    0, //encodeHTMLChars
    1, //escapeForwardSlashes
    0, //sortKeys
    0, //indent
    NULL, //prv
  };

  // use encoder.prv to pass around the ctx
  encoder.prv = ctx;

  PRINTMARK();

  if (!HPyArg_ParseKeywords(ctx, &ht, args, nargs, kw, "O|OOOOi", kwlist, &oinput, &oensureAscii, &oencodeHTMLChars, &oescapeForwardSlashes, &osortKeys, &encoder.indent))
  {
     return HPy_NULL;
  }

  oinput_o = HPy_AsPyObject(ctx, oinput);

  if (!HPy_IsNull(oensureAscii) && !HPy_IsTrue(ctx, oensureAscii))
  {
    encoder.forceASCII = 0;
  }

  if (!HPy_IsNull(oencodeHTMLChars) && HPy_IsTrue(ctx, oencodeHTMLChars))
  {
    encoder.encodeHTMLChars = 1;
  }

  if (!HPy_IsNull(oescapeForwardSlashes) && !HPy_IsTrue(ctx, oescapeForwardSlashes))
  {
    encoder.escapeForwardSlashes = 0;
  }

  if (!HPy_IsNull(osortKeys) && HPy_IsTrue(ctx, osortKeys))
  {
    encoder.sortKeys = 1;
  }

  dconv_d2s_init(DCONV_D2S_EMIT_TRAILING_DECIMAL_POINT | DCONV_D2S_EMIT_TRAILING_ZERO_AFTER_POINT,
                 NULL, NULL, 'e', DCONV_DECIMAL_IN_SHORTEST_LOW, DCONV_DECIMAL_IN_SHORTEST_HIGH, 0, 0);

  PRINTMARK();
  ret = JSON_EncodeObject (oinput_o, &encoder, buffer, sizeof (buffer));
  PRINTMARK();

  dconv_d2s_free();

  if (PyErr_Occurred())
  {
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  // XXX: FIXME set error with HPy error
  if (encoder.errorMsg)
  {
    if (ret != buffer)
    {
      encoder.free (ret);
    }

    PyErr_Format (PyExc_OverflowError, "%s", encoder.errorMsg);
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  h_ret = HPyUnicode_FromString(ctx, ret);

  if (ret != buffer)
  {
    encoder.free (ret);
  }

  PRINTMARK();

  HPyTracker_Close(ctx, ht);
  return h_ret;
}


HPyDef_METH(objToJSONFile, "dump", objToJSONFile_impl, HPyFunc_KEYWORDS,
            .doc="Converts arbitrary object recursively into JSON file. " \
                 ENCODER_HELP_TEXT);
static HPy
objToJSONFile_impl(HPyContext *ctx, HPy self, HPy *args, HPy_ssize_t nargs, HPy kw)
{
  HPy data;
  HPy file;
  HPy string;
  HPy write;
  HPy argtuple;
  HPy result;
  HPyTracker ht;

  PRINTMARK();

  if (!HPyArg_Parse(ctx, &ht, args, nargs, "OO", &data, &file))
  {
    return HPy_NULL;
  }

  if (!HPy_HasAttr_s(ctx, file, "write"))
  {
    HPyTracker_Close(ctx, ht);
    HPyErr_SetString(ctx, ctx->h_TypeError, "expected file");
    return HPy_NULL;
  }

  write = HPy_GetAttr_s(ctx, file, "write");
  if (HPy_IsNull(write)) {
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  if (!HPyCallable_Check(ctx, write))
  {
    HPy_Close(ctx, write);
    HPyTracker_Close(ctx, ht);
    HPyErr_SetString(ctx, ctx->h_TypeError, "expected file");
    return HPy_NULL;
  }

  HPy objtojson_args[] = { data };
  string = objToJSON_impl(ctx, self, objtojson_args, 1, kw);
  if (HPy_IsNull(string)) {
    HPy_Close(ctx, write);
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  argtuple = HPyTuple_Pack(ctx, 1, string);
  HPy_Close(ctx, string);
  if (HPy_IsNull(argtuple))
  {
    HPy_Close(ctx, write);
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  result = HPy_CallTupleDict(ctx, write, argtuple, HPy_NULL);
  if (HPy_IsNull(result))
  {
    HPy_Close(ctx, write);
    HPy_Close(ctx, argtuple);
    HPyTracker_Close(ctx, ht);
    return HPy_NULL;
  }

  HPy_Close(ctx, write);
  HPy_Close(ctx, argtuple);
  HPy_Close(ctx, result);
  HPyTracker_Close(ctx, ht);

  PRINTMARK();

  return HPy_Dup(ctx, ctx->h_None);
}
