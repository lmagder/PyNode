#define PY_SSIZE_T_CLEAN
#include "jswrapper.hpp"
#include "helpers.hpp"
#include <structmember.h>
#include <optional>
#include "napi.h"

struct WrappedJSObject {
    PyObject_HEAD
        /* Type-specific fields go here. */
    struct CPPData
    {
        Napi::ObjectReference object_reference;
    };
    CPPData cpp;
};

static void
WrappedJSObject_dealloc(PyObject* obj)
{
    WrappedJSObject *self = (WrappedJSObject *)obj;
    self->cpp.~CPPData();
    Py_TYPE(self)->tp_free((PyObject *) self);
}



static PyObject *
WrappedJSObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    WrappedJSObject *self;
    self = (WrappedJSObject *) type->tp_alloc(type, 0);
    if (self != NULL) {
        new (&self->cpp) WrappedJSObject::CPPData();
    }
    return (PyObject *) self;
}

static int
WrappedJSObject_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    if (!PyArg_ParseTuple(args, ""))
        return -1;
    return 0;
}

static PyMemberDef WrappedJSObject_members[] = {
    {NULL}  /* Sentinel */
};

static PyMethodDef WrappedJSObject_methods[] = {
    {NULL}, /* Sentinel */
};

static PyObject *
WrappedJSObject_getattro(PyObject *_self, PyObject *attr)
{
    WrappedJSObject *self = (WrappedJSObject*)_self;
    auto env = self->cpp.object_reference.Env();
    auto wrapped = self->cpp.object_reference.Value();
    napi_valuetype type;
    bool hasattr;
    const char * utf8name = PyUnicode_AsUTF8(attr);
    if (wrapped.Has(utf8name)) {
        auto result = wrapped.Get(utf8name);
        py_object_owned pyval = ConvertToPython(result);
        if (pyval) {
            return pyval.release();
        }
    }
    PyErr_SetObject(PyExc_AttributeError, attr);
    Py_RETURN_NONE;
}

static PyObject *
WrappedJSObject_call(PyObject *_self, PyObject *args, PyObject *kwargs)
{
    WrappedJSObject *self = (WrappedJSObject*)_self;
    auto env = self->cpp.object_reference.Env();
    auto wrapped = self->cpp.object_reference.Value();

    if (!wrapped.IsFunction()) {
        PyErr_SetString(PyExc_RuntimeError, "Error calling javascript function");
        Py_RETURN_NONE;
    }

    auto wrappedFunc = wrapped.As<Napi::Function>();

    py_object_owned seq(PySequence_Fast(args, "*args must be a sequence"));
    Py_ssize_t len = PySequence_Size(args);
    auto jsargs = std::vector<Napi::Value>(len);
    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *arg = PySequence_Fast_GET_ITEM(seq.get(), i);
        jsargs[i] = ConvertFromPython(env, arg);
    }

    Napi::Object thisPtr = self->cpp.object_reference.Value();

    auto result = wrappedFunc.Call(thisPtr, jsargs);
    if (!result) {
        PyErr_SetString(PyExc_RuntimeError, "Error calling javascript function");
        Py_RETURN_NONE;
    }

    py_object_owned pyval = ConvertToPython(result);
    if (!pyval) {
        PyErr_SetString(PyExc_RuntimeError, "Error converting JS return value to Python");
        Py_RETURN_NONE;
    }

    return pyval.release();
}

PyObject * WrappedJSObject_str(PyObject *_self) {
    WrappedJSObject *self = (WrappedJSObject *)_self;

    auto wrapped = self->cpp.object_reference.Value();
    auto result = wrapped.ToString();
    if (result.IsEmpty()) {
        PyErr_SetString(PyExc_RuntimeError, "Error coercing javascript value to string");
        Py_RETURN_NONE;
    }

    /* Result should just be a JavaScript string at this point */
    py_object_owned pyval = ConvertToPython(result);
    if (pyval == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Error converting JavaScript ToString item to Python");
        Py_RETURN_NONE;
    }
    return pyval.release();
}

PyTypeObject WrappedJSType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

PyTypeObject ExistingPyWrapperType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

PyObject *WrappedJSObject_New(Napi::Object value) {
    /* Call the class object. */
    py_object_owned obj(PyObject_CallNoArgs((PyObject *)&WrappedJSType));
    if (!obj) {
        PyErr_Print();
        Py_RETURN_NONE;
    }

    auto self = ((WrappedJSObject*)obj.get());
    self->cpp.object_reference = Napi::Persistent(value);

    return obj.release();
}

PyObject* ExistingPyWrapper_New(Napi::Object value) {
    py_object_owned obj(PyObject_CallNoArgs((PyObject*)&ExistingPyWrapperType));
    if (!obj) {
        PyErr_Print();
        Py_RETURN_NONE;
    }

    auto self = ((WrappedJSObject*)obj.get());
    self->cpp.object_reference = Napi::Weak(value);

    return obj.release();
}

Napi::Object WrappedJSObject_get_napi_value(PyObject* s) {
    if (s && Py_IS_TYPE(s, &WrappedJSType))
    {
        WrappedJSObject* self = (WrappedJSObject*)s;
        return self->cpp.object_reference.Value();
    }
    return {};
}

Napi::Object ExistingPyWrapper_get_napi_value(PyObject* s) {
    if (s && Py_IS_TYPE(s, &ExistingPyWrapperType))
    {
        WrappedJSObject* self = (WrappedJSObject*)s;
        return self->cpp.object_reference.Value();
    }
    return {};
}

static PyModuleDef pynodemodule = {
    PyModuleDef_HEAD_INIT,
};

PyMODINIT_FUNC
PyInit_jswrapper(void)
{

    WrappedJSType.tp_name = "pynode.WrappedJSObject";
    WrappedJSType.tp_doc = "A JavaScript object";
    WrappedJSType.tp_basicsize = sizeof(WrappedJSObject);
    WrappedJSType.tp_itemsize = 0;
    WrappedJSType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    WrappedJSType.tp_new = WrappedJSObject_new;
    WrappedJSType.tp_init = WrappedJSObject_init;
    WrappedJSType.tp_dealloc = WrappedJSObject_dealloc;
    WrappedJSType.tp_members = WrappedJSObject_members;
    WrappedJSType.tp_methods = WrappedJSObject_methods;
    WrappedJSType.tp_call = WrappedJSObject_call;
    WrappedJSType.tp_getattro = WrappedJSObject_getattro;
    WrappedJSType.tp_str = WrappedJSObject_str;


    ExistingPyWrapperType.tp_name = "pynode.ExistingPyWrapperType";
    ExistingPyWrapperType.tp_basicsize = sizeof(WrappedJSObject);
    ExistingPyWrapperType.tp_itemsize = 0;
    ExistingPyWrapperType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    ExistingPyWrapperType.tp_new = WrappedJSObject_new;
    ExistingPyWrapperType.tp_init = WrappedJSObject_init;
    ExistingPyWrapperType.tp_dealloc = WrappedJSObject_dealloc;
    ExistingPyWrapperType.tp_members = WrappedJSObject_members;
    ExistingPyWrapperType.tp_methods = WrappedJSObject_methods;

    pynodemodule.m_name = "pynode";
    pynodemodule.m_doc = "Python <3 JavaScript.";
    pynodemodule.m_size = -1;

    if (PyType_Ready(&WrappedJSType) < 0)
        return NULL;

    if (PyType_Ready(&ExistingPyWrapperType) < 0)
        return NULL;

    py_object_owned m(PyModule_Create(&pynodemodule));
    if (m == NULL)
        return NULL;

    if (PyModule_AddObject(m.get(), "WrappedJSObject", (PyObject *) &WrappedJSType) < 0) {
        return NULL;
    }

    if (PyModule_AddObject(m.get(), "ExistingPyWrapperType", (PyObject*)&ExistingPyWrapperType) < 0) {
        return NULL;
    }

    return m.release();
}
