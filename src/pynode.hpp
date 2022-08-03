#ifndef PYNODE_HPP
#define PYNODE_HPP

#include "napi.h"
#include <Python.h>
#include "helpers.hpp"
#include <unordered_map>
#include <map>
#include <unordered_set>

struct PyNodeEnvData
{
    py_object_owned pPyNodeModule;

    Napi::FunctionReference PyNodeWrappedPythonObjectConstructor;
    
    struct WeakRef
    {
        py_object_owned pyWeakRef;
        Napi::ObjectReference existingJSObject;
    };

    std::map<PyObject*, WeakRef> objectMappings;
    std::unordered_map<PyObject*, std::map<PyObject*, WeakRef>::iterator> weakRefToSlot;

    PyNodeEnvData() { s_envData.insert(this); }
    ~PyNodeEnvData() { 
        s_envData.erase(this);

        py_ensure_gil gil;
        weakRefToSlot.clear();
        objectMappings.clear();
        pPyNodeModule.reset();
    }

    static std::unordered_set<PyNodeEnvData*> s_envData;
};

Napi::Object PyNodeInit(Napi::Env env, Napi::Object exports);

#endif
