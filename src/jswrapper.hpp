#ifndef PYNODE_JSWRAPPER_HPP
#define PYNODE_JSWRAPPER_HPP

#include <Python.h>
#include <Napi.h>
#include "helpers.hpp"

PyMODINIT_FUNC PyInit_jswrapper(void);
PyObject *WrappedJSObject_New(Napi::Object value);
Napi::Object WrappedJSObject_get_napi_value(PyObject *);
extern PyTypeObject WrappedJSType;
extern PyObject* WeakRefCleanupFunc;

#endif
