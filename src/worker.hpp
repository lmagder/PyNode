#ifndef PYNODE_WORKER_HPP
#define PYNODE_WORKER_HPP

#include "Python.h"
#include "pywrapper.hpp"
#include "helpers.hpp"
#include "napi.h"

class PyNodeWorker : public Napi::AsyncWorker {
public:
  PyNodeWorker(Napi::Function &callback, py_object_owned&& pyArgs, py_object_owned&& pFunc);
  ~PyNodeWorker();
  void Execute();
  void OnOK();
  void OnError(const Napi::Error &e);

private:
  py_object_owned pyArgs;
  py_object_owned pFunc;
  py_object_owned pValue;
};

#endif
