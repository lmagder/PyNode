#include "pywrapper.hpp"
#include <napi.h>
#include <iostream>

Napi::Object PyNodeWrappedPythonObject::Init(Napi::Env env, Napi::Object exports) {
    // This method is used to hook the accessor and method callbacks
    Napi::Function func = DefineClass(env, "PyNodeWrappedPythonObject", {
        InstanceMethod("call", &PyNodeWrappedPythonObject::Call),
        InstanceMethod("get", &PyNodeWrappedPythonObject::GetAttr),
        InstanceMethod("set", &PyNodeWrappedPythonObject::SetAttr),
        InstanceMethod("repr", &PyNodeWrappedPythonObject::Repr)
    });

    // Create a peristent reference to the class constructor. This will allow
    // a function called on a class prototype and a function
    // called on instance of a class to be distinguished from each other.
    constructor = Napi::Persistent(func);
    // Call the SuppressDestruct() method on the static data prevent the calling
    // to this destructor to reset the reference when the environment is no longer
    // available.
    constructor.SuppressDestruct();
    exports.Set("PyNodeWrappedPythonObject", func);
    return exports;
}

PyNodeWrappedPythonObject::PyNodeWrappedPythonObject(const Napi::CallbackInfo &info) : Napi::ObjectWrap<PyNodeWrappedPythonObject>(info) {
    _value = ConvertBorrowedObjectToOwned(info[0].As<Napi::External<PyObject>>().Data());
}

Napi::FunctionReference PyNodeWrappedPythonObject::constructor;

Napi::Value PyNodeWrappedPythonObject::GetAttr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    std::string attrname = info[0].As<Napi::String>();
    py_object_owned attr(PyObject_GetAttrString(_value.get(), attrname.c_str()));
    if (attr == NULL) {
        std::string error("Attribute " + attrname + " not found.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Value returnval = ConvertFromPython(env, attr.get());
    return returnval;
}

Napi::Value PyNodeWrappedPythonObject::SetAttr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    if (info.Length() != 2) {
        std::string error("Missing method name and value");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    py_object_owned pValue = ConvertToPython(info[1]);
    std::string attrname = info[0].ToString();
    if (PyObject_SetAttrString(_value.get(), attrname.c_str(), pValue.get()) != 0) {
        std::string error("Attribute " + attrname + " not found.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    return env.Null();;
}

Napi::Value PyNodeWrappedPythonObject::Call(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    int callable = PyCallable_Check(_value.get());
    if (! callable) {
        std::string error("This Python object is not callable.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    py_object_owned pArgs = BuildPyArgs(info, 0, info.Length());
    py_object_owned pReturnValue(PyObject_Call(_value.get(), pArgs.get(), NULL));
    PyObject *error_occurred = PyErr_Occurred();
    if (error_occurred != NULL) {
        // TODO - get the traceback string into Javascript
        std::string error("A Python error occurred.");
        PyErr_Print();
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Value returnval = ConvertFromPython(env, pReturnValue.get());
    return returnval;
}

Napi::Value PyNodeWrappedPythonObject::Repr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    std::string attrname = info[0].As<Napi::String>();
    py_object_owned repr(PyObject_Repr(_value.get()));
    if (repr == NULL) {
        std::string error("repr() failed.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Null();
    }
    const char * repr_c = PyUnicode_AsUTF8(repr.get());
    Napi::Value result = Napi::String::New(env, repr_c); // (Napi takes ownership of repr_c)
    return result;
}

