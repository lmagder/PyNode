#ifndef PYNODE_PYWRAPPER_HPP
#define PYNODE_PYWRAPPER_HPP

#include "Python.h"
#include "helpers.hpp"
#include "napi.h"

class PyNodeWrappedPythonObject : public Napi::ObjectWrap<PyNodeWrappedPythonObject> {
  public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    PyNodeWrappedPythonObject(const Napi::CallbackInfo &info);
    ~PyNodeWrappedPythonObject();
    Napi::Value Call(const Napi::CallbackInfo &info);
    Napi::Value GetAttr(const Napi::CallbackInfo &info);
    Napi::Value SetAttr(const Napi::CallbackInfo &info);
    Napi::Value Repr(const Napi::CallbackInfo &info);
    Napi::Value GetPyType(const Napi::CallbackInfo& info);
    PyObject * getValue() { return _value.get(); };

  private:
    py_object_owned _value;
};

#endif

