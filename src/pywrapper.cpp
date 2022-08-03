#include "pywrapper.hpp"
#include "pynode.hpp"
#include "worker.hpp"
#include <napi.h>
#include <iostream>


Napi::Object PyNodeWrappedPythonObject::Init(Napi::Env env, Napi::Object exports) {
    // This method is used to hook the accessor and method callbacks
    Napi::Function func = DefineClass(env, "PyNodeWrappedPythonObject", {
        InstanceMethod("__call__", &PyNodeWrappedPythonObject::Call),
        InstanceMethod("__callasync__", &PyNodeWrappedPythonObject::CallAsync),
        InstanceMethod("__callasync_promise__", &PyNodeWrappedPythonObject::CallAsyncPromise),
        InstanceMethod("__getattr__", &PyNodeWrappedPythonObject::GetAttr),
        InstanceMethod("__setattr__", &PyNodeWrappedPythonObject::SetAttr),
        InstanceMethod("__repr__", &PyNodeWrappedPythonObject::Repr),
        InstanceAccessor<&PyNodeWrappedPythonObject::GetPyType>("__pytype__"),
    });

    auto instData = env.GetInstanceData<PyNodeEnvData>();
    instData->PyNodeWrappedPythonObjectConstructor = Napi::Persistent(func);
    exports.Set("PyNodeWrappedPythonObject", func);
    return exports;
}

PyNodeWrappedPythonObject::PyNodeWrappedPythonObject(const Napi::CallbackInfo &info) : Napi::ObjectWrap<PyNodeWrappedPythonObject>(info) {
    _value = ConvertBorrowedObjectToOwned(info[0].As<Napi::External<PyObject>>().Data());
}

PyNodeWrappedPythonObject::~PyNodeWrappedPythonObject()
{
    py_ensure_gil ctx;
    _value = nullptr;
}

Napi::Value PyNodeWrappedPythonObject::GetAttr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    std::string attrname = info[0].As<Napi::String>();
    py_object_owned attr(PyObject_GetAttrString(_value.get(), attrname.c_str()));
    if (attr == NULL) {
        std::string error("Attribute " + attrname + " not found.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return ConvertFromPython(env, attr.get());
}

Napi::Value PyNodeWrappedPythonObject::SetAttr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    if (info.Length() != 2) {
        std::string error("Missing method name and value");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    py_object_owned pValue = ConvertToPython(info[1]);
    std::string attrname = info[0].ToString();
    if (PyObject_SetAttrString(_value.get(), attrname.c_str(), pValue.get()) != 0) {
        std::string error("Attribute " + attrname + " not found.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return env.Undefined();
}

Napi::Value PyNodeWrappedPythonObject::Call(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    int callable = PyCallable_Check(_value.get());
    if (! callable) {
        std::string error("This Python object is not callable.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    py_object_owned pArgs = BuildPyArgs(info, 0, info.Length());
    py_object_owned pReturnValue(PyObject_CallObject(_value.get(), pArgs.get()));
    PyObject *error_occurred = PyErr_Occurred();
    if (error_occurred != NULL) {
        // TODO - get the traceback string into Javascript
        std::string error("A Python error occurred.");
        PyErr_Print();
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return ConvertFromPython(env, pReturnValue.get());
}

Napi::Value PyNodeWrappedPythonObject::CallAsync(const Napi::CallbackInfo& info) {
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    int callable = PyCallable_Check(_value.get());
    if (!callable) {
        std::string error("This Python object is not callable.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() == 0 || !info[info.Length() - 1].IsFunction()) {
        std::cerr << "Last argument to 'call' must be a function" << std::endl;
        Napi::Error::New(env, "Last argument to 'call' must be a function")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto pArgs = BuildPyArgs(info, 0, info.Length() - 1);

    Napi::Function cb = info[info.Length() - 1].As<Napi::Function>();
    PyNodeWorker* pnw = new PyNodeWorker(cb, std::move(pArgs),  ConvertBorrowedObjectToOwned(_value.get()));
    pnw->Queue();
    return env.Undefined();
}

Napi::Value PyNodeWrappedPythonObject::CallAsyncPromise(const Napi::CallbackInfo& info) {
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    int callable = PyCallable_Check(_value.get());
    if (!callable) {
        std::string error("This Python object is not callable.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto pArgs = BuildPyArgs(info, 0, info.Length());

    auto ret = Napi::Promise::Deferred(env);
    PyNodeWorker* pnw = new PyNodeWorker(ret, std::move(pArgs), ConvertBorrowedObjectToOwned(_value.get()));
    pnw->Queue();
    return ret.Promise();
}

Napi::Value PyNodeWrappedPythonObject::Repr(const Napi::CallbackInfo &info){
    py_ensure_gil ctx;
    Napi::Env env = info.Env();
    std::string attrname = info[0].As<Napi::String>();
    py_object_owned repr(PyObject_Repr(_value.get()));
    if (repr == NULL) {
        std::string error("repr() failed.");
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    const char * repr_c = PyUnicode_AsUTF8(repr.get());
    Napi::Value result = Napi::String::New(env, repr_c); // (Napi takes ownership of repr_c)
    return result;
}

Napi::Value PyNodeWrappedPythonObject::GetPyType(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
#if PY_VERSION_HEX >= 0x030B0000
    py_object_owned nameStr(PyType_GetQualName(Py_TYPE(_value.get())));
    return Napi::String::New(env, PyUnicode_AsUTF8(nameStr.get()));
#else
    auto type = Py_TYPE(_value.get());
    if (type->tp_flags & Py_TPFLAGS_HEAPTYPE) {
        PyHeapTypeObject* et = (PyHeapTypeObject*)type;
        return Napi::String::New(env, PyUnicode_AsUTF8(et->ht_qualname));
    }
    else {
        return Napi::String::New(env, type->tp_name);
    }
#endif
}

