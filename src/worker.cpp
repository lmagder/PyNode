#include "worker.hpp"
#include <frameobject.h>
#include <iostream>
#include <sstream>

thread_local PyNodeWorker* PyNodeWorker::s_currentWorker = nullptr;

PyNodeWorker::PyNodeWorker(Napi::Function callback, py_object_owned&& pyArgs,
                           py_object_owned&& pFunc)
    : Napi::AsyncProgressQueueWorker<std::shared_ptr<PyNodeWorkerCallback>>(callback), pyArgs(std::move(pyArgs)), pFunc(std::move(pFunc)), pValue(nullptr){};

PyNodeWorker::PyNodeWorker(Napi::Promise::Deferred promise, py_object_owned&& pyArgs,
    py_object_owned&& pFunc)
    :Napi::AsyncProgressQueueWorker<std::shared_ptr<PyNodeWorkerCallback>>(promise.Env()), promise(promise), pyArgs(std::move(pyArgs)), pFunc(std::move(pFunc)), pValue(nullptr) {};

void PyNodeWorker::Execute(const ExecutionProgress& progress) {
  {
    py_thread_context_worker ctx(this, progress);

    pValue.reset(PyObject_CallObject(pFunc.get(), pyArgs.get()));
    PyObject* errOccurred = PyErr_Occurred();

    if (errOccurred != NULL) {
      std::string error;
      PyObject *pErrType = nullptr, *pErrValue = nullptr, *pErrTraceback = nullptr;
      PyErr_Fetch(&pErrType, &pErrValue, &pErrTraceback);
      py_object_owned pTypeString(PyObject_Str(pErrType));
      py_object_owned pValueString(PyObject_Str(pErrValue));

      const char *value = PyUnicode_AsUTF8(pValueString.get());
      const char *type = PyUnicode_AsUTF8(pTypeString.get());
      PyTracebackObject *tb = (PyTracebackObject *)pErrTraceback;
      _frame *frame = tb ? tb->tb_frame : nullptr;

      if (!frame) {
          error.append(std::string(type) + ": " + std::string(value));
      }

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

      PyErr_Restore(pErrType, pErrValue, pErrTraceback);
      PyErr_Print();
      pValue = nullptr;
      SetError(error);
    }
    else if (!pValue)
    {
      SetError("Function call failed");
    }

    pFunc = nullptr;
    pyArgs = nullptr;
  }
}

void PyNodeWorker::OnProgress(const std::shared_ptr<PyNodeWorkerCallback>* data, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        data[i]->work();
        std::unique_lock lock(data[i]->mutex);
        data[i]->done = true;
        data[i]->condition.notify_all();
    }
}

std::vector<napi_value> PyNodeWorker::GetResult(Napi::Env env)
{
    Napi::Value ret = env.Undefined();
    {
        py_thread_context ctx;
        ret = ConvertFromPython(env, pValue.get());
        pValue = nullptr;
    }
    return { env.Null(),  ret };
}

void PyNodeWorker::OnOK() {
  if (promise)
  {
      promise->Resolve(GetResult(promise->Env())[1]);
  }
  else
  {
       Napi::AsyncWorker::OnOK();
  }
  
}

void PyNodeWorker::OnError(const Napi::Error &e) {
    if (promise) 
    {
        promise->Reject(e.Value());
    }
    else
    {
        Napi::AsyncWorker::OnError(e);
    }
}
