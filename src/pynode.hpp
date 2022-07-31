#ifndef PYNODE_HPP
#define PYNODE_HPP

#include "napi.h"
#include <Python.h>

struct PyNodeEnvData
{
    Napi::FunctionReference PyNodeWrappedPythonObjectConstructor;
};

Napi::Object PyNodeInit(Napi::Env env, Napi::Object exports);

#endif
