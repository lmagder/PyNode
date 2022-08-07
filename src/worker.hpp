#ifndef PYNODE_WORKER_HPP
#define PYNODE_WORKER_HPP

#include "Python.h"
#include "pywrapper.hpp"
#include "helpers.hpp"
#include "napi.h"
#include <optional>
#include <mutex>
#include <condition_variable>

struct PyNodeWorkerCallback
{
	//it would be nice to use std::binary_semaphore but C++20 support seems sketchy in node-gyp
	std::mutex mutex;
    std::condition_variable condition;
	bool done = false;
	std::function<void()> work;
};

class PyNodeWorker : public Napi::AsyncProgressQueueWorker<std::shared_ptr<PyNodeWorkerCallback>> {
public:
  PyNodeWorker(Napi::Function callback, py_object_owned&& pyArgs, py_object_owned&& pFunc);
  PyNodeWorker(Napi::Promise::Deferred promise, py_object_owned&& pyArgs, py_object_owned&& pFunc);
  void Execute(const ExecutionProgress& progress) override;
  void OnProgress(const std::shared_ptr<PyNodeWorkerCallback>* data, size_t count) override;
  std::vector<napi_value> GetResult(Napi::Env env) override;
  void OnOK() override;
  void OnError(const Napi::Error &e) override;
  
  template <typename T>
  static void WrapJSInteractionFromAsyncThread(T&& work)
  {
	  if (s_currentWorker)
	  {
		  auto item = std::make_shared<PyNodeWorkerCallback>();
		  item->work = std::forward<T>(work);
		  s_currentWorker->execProgress->Send(&item, 1);

		  Py_BEGIN_ALLOW_THREADS
		  std::unique_lock lock(item->mutex);
		  item->condition.wait(lock, [&]() { return item->done; });
		  Py_END_ALLOW_THREADS
	  }
	  else
	  {
		  work();
	  }
  }
private:
  std::optional<Napi::Promise::Deferred> promise;
  py_object_owned pyArgs;
  py_object_owned pFunc;
  py_object_owned pValue;
  friend struct py_thread_context_worker;

  const ExecutionProgress* execProgress;
  static thread_local PyNodeWorker* s_currentWorker;
};

struct py_thread_context_worker : public py_thread_context
{
	py_thread_context_worker(PyNodeWorker* c, const PyNodeWorker::ExecutionProgress& ep)
	{
		c->execProgress = &ep;
		PyNodeWorker::s_currentWorker = c;
	}

	~py_thread_context_worker()
	{
		auto prev = std::exchange(PyNodeWorker::s_currentWorker, nullptr);
		prev->execProgress = nullptr;
	}
};

#endif
