#include "helpers.hpp"
#include "jswrapper.h"
#include <iostream>

bool isNapiValueInt(Napi::Env &env, Napi::Value &num) {
  return env.Global()
      .Get("Number")
      .ToObject()
      .Get("isInteger")
      .As<Napi::Function>()
      .Call({num})
      .ToBoolean()
      .Value();
}

/* Returns true if the value given is (roughly) an object literal,
 * ie more appropriate as a Python dict than a WrappedJSObject.
 *
 * Based on https://stackoverflow.com/questions/5876332/how-can-i-differentiate-between-an-object-literal-other-javascript-objects
 */
bool isNapiValuePlainObject(Napi::Env &env, Napi::Value &obj) {
    napi_value result;
    napi_value plainobj_result;
    napi_status status;

    status = napi_get_prototype(env, obj, &result);
    if (status != napi_ok) {
        return false;
    }

    Napi::Object plainobj = Napi::Object::New(env);
    status = napi_get_prototype(env, plainobj, &plainobj_result);
    if (status != napi_ok) {
        return false;
    }

    bool equal;
    status = napi_strict_equals(env, result, plainobj_result, &equal);
    return equal;
}

bool isNapiValueWrappedPython(Napi::Env &env, Napi::Object obj) {
    return obj.InstanceOf(PyNodeWrappedPythonObject::constructor.Value());
}

int Py_GetNumArguments(PyObject *pFunc) {
  py_object_owned fc(PyObject_GetAttrString(pFunc, "__code__"));
  if (fc) {
    py_object_owned ac(PyObject_GetAttrString(fc.get(), "co_argcount"));
    if (ac) {
      long count = PyLong_AsLong(ac.get());
      return count;
    }
  }
  return 0;
}

py_object_owned BuildPyArray(Napi::Env env, Napi::Value arg) {
  auto arr = arg.As<Napi::Array>();
  py_object_owned list(PyList_New(arr.Length()));

  for (size_t i = 0; i < arr.Length(); i++) {
    auto element = arr.Get(i);
    py_object_owned pyval = ConvertToPython(element);
    if (pyval != NULL) {
      PyList_SetItem(list.get(), i, pyval.get());
    }
  }

  return list;
}

py_object_owned BuildPyDict(Napi::Env env, Napi::Value arg) {
  auto obj = arg.As<Napi::Object>();
  auto keys = obj.GetPropertyNames();
  py_object_owned dict(PyDict_New());
  for (size_t i = 0; i < keys.Length(); i++) {
    auto key = keys.Get(i);
    Napi::Value val = obj.Get(key);
    py_object_owned pykey(PyUnicode_FromString(key.ToString().Utf8Value().c_str()));
    py_object_owned pyval = ConvertToPython(val);
    if (pyval != NULL) {
      PyDict_SetItem(dict.get(), pykey.get(), pyval.get());
    }
  }

  return dict;
}

py_object_owned BuildWrappedJSObject(Napi::Env env, Napi::Value arg) {
  py_object_owned pyobj(WrappedJSObject_New(env, arg));
  return pyobj;
}

py_object_owned BuildPyArgs(const Napi::CallbackInfo &args, size_t start_index, size_t count) {
  py_object_owned pArgs(PyTuple_New(count));
  for (size_t i = start_index; i < start_index + count; i++) {
    auto arg = args[i];
    py_object_owned pyobj = ConvertToPython(arg);
    if (pyobj != NULL) {
      PyTuple_SetItem(pArgs.get(), i - start_index, pyobj.get());
    }
  }

  return pArgs;
}

py_object_owned ConvertToPython(Napi::Value arg) {
    Napi::Env env = arg.Env();
    if (arg.IsNumber()) {
      double num = arg.As<Napi::Number>().ToNumber();
      if (isNapiValueInt(env, arg)) {
        return py_object_owned(PyLong_FromLong(num));
      } else {
        return py_object_owned(PyFloat_FromDouble(num));
      }
    } else if (arg.IsString()) {
      std::string str = arg.As<Napi::String>();
      return py_object_owned(PyUnicode_FromString(str.c_str()));
    } else if (arg.IsBoolean()) {
      long b = arg.As<Napi::Boolean>().ToBoolean();
      return py_object_owned(PyBool_FromLong(b));
      // } else if (arg.IsDate()) {
      //   printf("Dates dont work yet");
      //   // Nan::ThrowError("Dates dont work yet");
      //   throw Napi::Error::New(args.Env(), "Dates dont work  yet");
    } else if (arg.IsArray()) {
      return BuildPyArray(env, arg);
    } else if (arg.IsObject()) {
      if (isNapiValueWrappedPython(env, arg.ToObject())) {
        Napi::Object o = arg.ToObject();
        PyNodeWrappedPythonObject *wrapper = Napi::ObjectWrap<PyNodeWrappedPythonObject>::Unwrap(o);
        return ConvertBorrowedObjectToOwned(wrapper->getValue());
      } else if (isNapiValuePlainObject(env, arg)) {
        return BuildPyDict(env, arg);
      } else {
        return BuildWrappedJSObject(env, arg);
      }
    } else if (arg.IsNull() || arg.IsUndefined()) {
      return ConvertBorrowedObjectToOwned(Py_None);
    } else {
      Napi::String string = arg.ToString();
      std::cout << "Unknown arg type" << string.Utf8Value() << std::endl;
      throw Napi::Error::New(arg.Env(), "Unknown arg type");
    }
}


py_object_owned convert_napi_value_to_python(napi_env env, napi_value value) {
      Napi::Value cpp_value = Napi::Value(env, value);
      return ConvertToPython(cpp_value);
}

Napi::Array BuildV8Array(Napi::Env env, PyObject *obj) {
  const bool isList = PyList_Check(obj);
  Py_ssize_t len = isList ? PyList_Size(obj) : PyTuple_Size(obj);

  auto arr = Napi::Array::New(env);

  for (Py_ssize_t i = 0; i < len; i++) {
    PyObject *localObj;
    if (isList) {
      localObj = PyList_GetItem(obj, i);
    } else {
      localObj = PyTuple_GetItem(obj, i);
    }

    Napi::Value result = env.Null();
    if (localObj) 
      result = ConvertFromPython(env, localObj);
    
    arr.Set(i, result);
    
  }
  return arr;
}


Napi::Object BuildV8Dict(Napi::Env env, PyObject *obj) {
  auto keys = PyDict_Keys(obj);
  auto size = PyList_GET_SIZE(keys);
  auto jsObj = Napi::Object::New(env);

  for (Py_ssize_t i = 0; i < size; i++) {
    auto key = PyList_GetItem(keys, i);
    auto keyString = PyObject_Str(key);
    auto val = PyDict_GetItem(obj, key);
    auto jsKey = Napi::String::New(env, PyUnicode_AsUTF8(keyString));
    jsObj.Set(jsKey, ConvertFromPython(env, val));
    Py_DECREF(keyString);
  }

  Py_DECREF(keys);

  return jsObj;
}

Napi::Value ConvertFromPython(Napi::Env env, PyObject * pValue) {
    Napi::Value result = env.Null();
    if (pValue == Py_None) {
      // leave as null
    } else if (PyBool_Check(pValue) == 0) {
      bool b = PyObject_IsTrue(pValue);
      result = Napi::Boolean::New(env, b);
    } else if (PyLong_Check(pValue)) {
      double d = PyLong_AsDouble(pValue);
      result = Napi::Number::New(env, d);
    } else if (PyFloat_Check(pValue)) {
      double d = PyFloat_AsDouble(pValue);
      result = Napi::Number::New(env, d);
    } else if (PyBytes_Check(pValue)) {
      auto str = Napi::String::New(env, PyBytes_AsString(pValue));
      result = str;
    } else if (PyUnicode_Check(pValue)) {
      auto str = Napi::String::New(env, PyUnicode_AsUTF8(pValue));
      result = str;
    } else if (PyList_Check(pValue) || PyTuple_Check(pValue)) {
      auto arr = BuildV8Array(env, pValue);
      result = arr;
    } else if (PyDict_Check(pValue)) {
      auto obj = BuildV8Dict(env, pValue);
      result = obj;
    } else if (Py_IS_TYPE(pValue,&WrappedJSType)) {
      auto obj = Napi::Value(env, WrappedJSObject_get_napi_value(pValue));
      result = obj;
    } else {
      auto exp = Napi::External<PyObject>::New(env, pValue);
      auto obj = PyNodeWrappedPythonObject::constructor.New({exp});
      result = obj;
    }
    return result;
}

napi_value convert_python_to_napi_value(napi_env env, PyObject * obj) {
    return ConvertFromPython(env, obj);
}


