#include "worker.hpp"
#include <frameobject.h>
#include <iostream>
#include <sstream>

PyNodeWorker::PyNodeWorker(Napi::Function &callback, py_object_owned&& pyArgs,
                           py_object_owned&& pFunc)
    : Napi::AsyncWorker(callback), pyArgs(std::move(pyArgs)), pFunc(std::move(pFunc)), pValue(nullptr){};
PyNodeWorker::~PyNodeWorker(){};

void PyNodeWorker::Execute() {
  {
    py_thread_context ctx;

    pValue.reset(PyObject_CallObject(pFunc.get(), pyArgs.get()));
    PyObject* errOccurred = PyErr_Occurred();

    if (errOccurred != NULL) {
      std::string error;
      PyObject *pType, *pValue, *pTraceback;
      PyErr_Fetch(&pType, &pValue, &pTraceback);
      py_object_owned pTypeString(PyObject_Str(pType));
      py_object_owned pValueString(PyObject_Str(pValue));

      const char *value = PyUnicode_AsUTF8(pValueString.get());
      const char *type = PyUnicode_AsUTF8(pTypeString.get());
      PyTracebackObject *tb = (PyTracebackObject *)pTraceback;
      _frame *frame = tb->tb_frame;

      while (frame != NULL) {
        int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
        const char *filename = PyUnicode_AsUTF8(frame->f_code->co_filename);
        const char *funcname = PyUnicode_AsUTF8(frame->f_code->co_name);
        if (filename) {
          error.append("File \"" + std::string(filename) + "\"");
        }
        if (funcname) {
          error.append(" Line " + std::to_string(line) + ", in " + std::string(funcname) +
                     "\n");
        }
        error.append(std::string(type) + ": " + std::string(value));
        frame = frame->f_back;
      }

      PyErr_Restore(pType, pValue, pTraceback);
      PyErr_Print();
      SetError(error);
    }

    pFunc = nullptr;
    pyArgs = nullptr;
  }
}

void PyNodeWorker::OnOK() {
  py_thread_context ctx;
  Napi::HandleScope scope(Env());

  std::vector<Napi::Value> result = {Env().Null(), Env().Null()};

  if (pValue != NULL) {
    result[1] = ConvertFromPython(Env(), pValue.get());
    pValue = nullptr;
  } else {
    std::string error;
    error.append("Function call failed");
    PyErr_Print();

    result[0] = Napi::Error::New(Env(), error).Value();
  }
  Callback().Call({result[0], result[1]});
}

void PyNodeWorker::OnError(const Napi::Error &e) {
  Callback().Call({e.Value(), Env().Null()});
}
