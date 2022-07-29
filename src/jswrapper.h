#ifndef PYNODE_JSWRAPPER_HPP
#define PYNODE_JSWRAPPER_HPP

#include <Python.h>
#include <node_api.h>
#include "helpers.h"

PyMODINIT_FUNC PyInit_jswrapper(void);
PyObject *WrappedJSObject_New(napi_env, napi_value);
napi_value WrappedJSObject_get_napi_value(PyObject *);
extern PyTypeObject WrappedJSType;


#endif
