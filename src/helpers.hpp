#ifndef PYNODE_HELPERS_HPP
#define PYNODE_HELPERS_HPP

#include <memory>
#include "napi.h"
#include <Python.h>

struct PyObjectDeleter {
  void operator()(PyObject* b) { Py_XDECREF(b); }
};

using py_object_owned = std::unique_ptr<PyObject, PyObjectDeleter>;

inline py_object_owned ConvertBorrowedObjectToOwned(PyObject* o)
{
  Py_XINCREF(o);
  return py_object_owned(o);
}

/* entry points to threads should grab a py_thread_context for the duration of the thread */
class py_thread_context {
public:
  py_thread_context() { gstate = PyGILState_Ensure(); }

  ~py_thread_context() {
    PyGILState_Release(gstate);
    if (PyGILState_Check() == 1)
      pts = PyEval_SaveThread();
  }

private:
  PyGILState_STATE gstate;
  PyThreadState *pts;
};

/* anywhere needing to call python functions (that isn't using py_thread_context) should create a py_ensure_gil object */
class py_ensure_gil {
public:
  py_ensure_gil() {
    gstate = PyGILState_Ensure();
  }

  ~py_ensure_gil() {
    PyGILState_Release(gstate);
  }

private:
  PyGILState_STATE gstate;
};

// v8 to Python
py_object_owned BuildPyArray(Napi::Env env, Napi::Value arg);
py_object_owned BuildPyDict(Napi::Env env, Napi::Value arg);
py_object_owned BuildWrappedJSObject(Napi::Object arg);
py_object_owned BuildPyArgs(const Napi::CallbackInfo &info, size_t start_index, size_t count);
py_object_owned ConvertToPython(Napi::Value);

// Python to v8
Napi::Array BuildV8Array(Napi::Env env, PyObject *obj);
Napi::Object BuildV8Dict(Napi::Env env, PyObject *obj);
Napi::Value ConvertFromPython(Napi::Env env, PyObject *obj);

int Py_GetNumArguments(PyObject *pFunc);

#endif
