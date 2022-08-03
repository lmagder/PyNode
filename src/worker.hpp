#ifndef PYNODE_WORKER_HPP
#define PYNODE_WORKER_HPP

#include "Python.h"
#include "pywrapper.hpp"
#include "helpers.hpp"
#include "napi.h"
#include <optional>

class PyNodeWorker : public Napi::AsyncWorker {
public:
  PyNodeWorker(Napi::Function callback, py_object_owned&& pyArgs, py_object_owned&& pFunc);
  PyNodeWorker(Napi::Promise::Deferred promise, py_object_owned&& pyArgs, py_object_owned&& pFunc);
  void Execute() override;
  std::vector<napi_value> GetResult(Napi::Env env) override;
  void OnOK() override;
  void OnError(const Napi::Error &e) override;
private:
  std::optional<Napi::Promise::Deferred> promise;
  py_object_owned pyArgs;
  py_object_owned pFunc;
  py_object_owned pValue;
};

#endif
