#define PY_SSIZE_T_CLEAN
#include "jswrapper.h"
#include <structmember.h>

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    napi_ref object_reference;
    napi_env env;
    napi_value bound;
} WrappedJSObject;

static void
WrappedJSObject_dealloc(PyObject* obj)
{
    WrappedJSObject *self = (WrappedJSObject *)obj;
    if (self->object_reference != NULL) {
        napi_delete_reference(self->env, self->object_reference);
        self->object_reference = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

napi_value WrappedJSObject_get_napi_value(PyObject *_self) {
    WrappedJSObject *self = (WrappedJSObject *)_self;
    napi_value wrapped;
    napi_get_reference_value(self->env, self->object_reference, &wrapped);
    return wrapped;
}

static void
WrappedJSObject_assign_napi_value(WrappedJSObject *self, napi_env env, napi_value value) {
    self->env = env;
    if (self->object_reference != NULL) {
        napi_delete_reference(env, self->object_reference);
        self->object_reference = NULL;
    }
    napi_create_reference(env, value, 1, &(self->object_reference));
}

static PyObject *
WrappedJSObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    WrappedJSObject *self;
    self = (WrappedJSObject *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->object_reference = NULL;
        self->env = NULL;
        self->bound = NULL;
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
    napi_value wrapped;
    napi_valuetype type;
    bool hasattr;
    const char * utf8name = PyUnicode_AsUTF8(attr);
    napi_value result;
    napi_get_reference_value(self->env, self->object_reference, &wrapped);
    napi_has_named_property(self->env, wrapped, utf8name, &hasattr);
    if (hasattr) {
        bool isfunc;
        napi_get_named_property(self->env, wrapped, utf8name, &result);
        napi_typeof(self->env, result, &type);
        isfunc = (type == napi_function);

        py_object_owned pyval = convert_napi_value_to_python(self->env, result);
        if (pyval != NULL) {
            if (isfunc) {
                /* "bind" the method to its instance */
                ((WrappedJSObject *)pyval.get())->bound = wrapped;
            }
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

    py_object_owned seq(PySequence_Fast(args, "*args must be a sequence"));
    Py_ssize_t len = PySequence_Size(args);
    auto jsargs = std::make_unique<napi_value[]>(len);
    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *arg = PySequence_Fast_GET_ITEM(seq.get(), i);
        napi_value jsarg = convert_python_to_napi_value(self->env, arg);
        jsargs[i] = jsarg;
    }

    napi_value wrapped;
    napi_get_reference_value(self->env, self->object_reference, &wrapped);

    napi_value thisPtr = nullptr;
    if (self->bound != NULL) {
        thisPtr = self->bound;
    } else {
        auto status = napi_get_global(self->env, &thisPtr);
        if (status != napi_ok) {
            PyErr_SetString(PyExc_RuntimeError, "Error getting JS global environment");
            Py_RETURN_NONE;
        }
    }

    napi_value result;
    auto status = napi_call_function(self->env, thisPtr, wrapped, len, jsargs.get(), &result);
    if (status != napi_ok) {
        PyErr_SetString(PyExc_RuntimeError, "Error calling javascript function");
        Py_RETURN_NONE;
    }

    py_object_owned pyval = convert_napi_value_to_python(self->env, result);
    if (pyval == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Error converting JS return value to Python");
        Py_RETURN_NONE;
    }

    return pyval.release();
}

PyObject * WrappedJSObject_str(PyObject *_self) {
    WrappedJSObject *self = (WrappedJSObject *)_self;
    napi_value global;
    napi_value result;
    napi_value wrapped;
    napi_status status;

    status = napi_get_global(self->env, &global);
    if (status != napi_ok) {
        PyErr_SetString(PyExc_RuntimeError, "Error getting JS global environment");
        Py_RETURN_NONE;
    }

    napi_get_reference_value(self->env, self->object_reference, &wrapped);

    status = napi_coerce_to_string(self->env, wrapped, &result);
    if (status != napi_ok) {
        PyErr_SetString(PyExc_RuntimeError, "Error coercing javascript value to string");
        Py_RETURN_NONE;
    }

    /* Result should just be a JavaScript string at this point */
    py_object_owned pyval = convert_napi_value_to_python(self->env, result);
    if (pyval == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Error converting JavaScript ToString item to Python");
        Py_RETURN_NONE;
    }
    return pyval.release();
}

PyTypeObject WrappedJSType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

PyObject *WrappedJSObject_New(napi_env env, napi_value value) {
    /* Call the class object. */
    py_object_owned obj(PyObject_CallNoArgs((PyObject *)&WrappedJSType));
    if (obj == NULL) {
        PyErr_Print();
    }

    WrappedJSObject_assign_napi_value((WrappedJSObject*)obj.get(), env, value);
    return obj.release();
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

    pynodemodule.m_name = "pynode";
    pynodemodule.m_doc = "Python <3 JavaScript.";
    pynodemodule.m_size = -1;

    if (PyType_Ready(&WrappedJSType) < 0)
        return NULL;

    py_object_owned m(PyModule_Create(&pynodemodule));
    if (m == NULL)
        return NULL;

    if (PyModule_AddObject(m.get(), "WrappedJSObject", (PyObject *) &WrappedJSType) < 0) {
        return NULL;
    }

    return m.release();
}
