#include "pynode.hpp"
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "helpers.hpp"
#include "worker.hpp"
#include "pywrapper.hpp"
#include "jswrapper.hpp"
#include <iostream>

std::unordered_set<PyNodeEnvData*> PyNodeEnvData::s_envData;

Napi::Value StartInterpreter(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (Py_IsInitialized())
      return env.Undefined();

#ifdef LINUX_SO_NAME 
  //automatically do the workaround of manually loading the python .so file on linux
  dlopen(Py_STRINGIFY(LINUX_SO_NAME), RTLD_LAZY | RTLD_GLOBAL);
#endif

  PyImport_AppendInittab("pynode", &PyInit_jswrapper);

  //Assume this is a path to venv folder
  if (info.Length() == 1 && info[0].IsString()) {
    std::string pathString = info[0].As<Napi::String>();
    std::wstring path(pathString.length(), L'#');
    mbstowcs(&path[0], pathString.c_str(), pathString.length());
    
    PyPreConfig preConfig;
    PyPreConfig_InitIsolatedConfig(&preConfig);
    Py_PreInitialize(&preConfig);
    
    PyConfig pyConfig;
    PyConfig_InitIsolatedConfig(&pyConfig);
    PyConfig_SetString(&pyConfig, &pyConfig.executable, path.c_str());

    Py_InitializeFromConfig(&pyConfig);

    PyConfig_Clear(&pyConfig);
  }
  else
  {
    Py_Initialize();
  }

  auto instData = env.GetInstanceData<PyNodeEnvData>();
  /* Load PyNode's own module into Python. This makes WrappedJSObject instances
     behave better (eg, having attributes) */
  py_object_owned pName(PyUnicode_DecodeFSDefault("pynode"));
  instData->pPyNodeModule.reset(PyImport_Import(pName.get()));
  if (instData->pPyNodeModule == NULL) {
    PyErr_Print();
    Napi::Error::New(env, "Failed to load the pynode module into the Python interpreter")
        .ThrowAsJavaScriptException();
  }

  /* Release the GIL. The other entry points back into Python re-acquire it */
  PyEval_SaveThread();

  return env.Undefined();
}

Napi::Value AppendSysPath(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (!info[0] || !info[0].IsString()) {
    Napi::Error::New(env, "Must pass a string to 'appendSysPath'")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string pathName = info[0].As<Napi::String>().ToString();
  char *appendPathStr;
  size_t len = (size_t)snprintf(NULL, 0, "import sys;sys.path.append(r\"%s\")",
                                pathName.c_str());
  appendPathStr = (char *)malloc(len + 1);
  snprintf(appendPathStr, len + 1, "import sys;sys.path.append(r\"%s\")",
           pathName.c_str());

  {
    py_ensure_gil ctx;
    PyRun_SimpleString(appendPathStr);
    free(appendPathStr);
  }

  return env.Undefined();
}

Napi::Value OpenFile(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (!info[0] || !info[0].IsString()) {
    Napi::Error::New(env, "Must pass a string to 'openFile'")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string fileName = info[0].As<Napi::String>().ToString();

  {
    py_ensure_gil ctx;

    py_object_owned pName(PyUnicode_DecodeFSDefault(fileName.c_str()));
    py_object_owned pFile(PyImport_Import(pName.get()));

    if (pFile == NULL) {
      PyErr_Print();
      std::cerr << "Failed to load module: " << fileName << std::endl;
      Napi::Error::New(env, "Failed to load python module")
          .ThrowAsJavaScriptException();
    } else {
        return ConvertFromPython(env, pFile.get());
    }
  }

  return env.Undefined();
}

Napi::Value ImportModule(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (!info[0] || !info[0].IsString()) {
    Napi::Error::New(env, "Must pass a string to 'import'")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string moduleName = info[0].As<Napi::String>();

  {
    py_ensure_gil ctx;

    py_object_owned module_object(PyImport_ImportModule(moduleName.c_str()));

    if (module_object == NULL) {
      PyErr_Print();
      std::cerr << "Failed to load module: " << moduleName << std::endl;
      Napi::Error::New(env, "Failed to load python module")
          .ThrowAsJavaScriptException();
    }
    return ConvertFromPython(env, module_object.get());
  }

  return env.Undefined();
}

Napi::Value Eval(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (!info[0] || !info[0].IsString()) {
    Napi::TypeError::New(env, "Must pass a string to 'eval'")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string statement = info[0].As<Napi::String>();
  int response;
  {
    py_ensure_gil ctx;

    response = PyRun_SimpleString(statement.c_str());
  }

  return Napi::Number::New(env, response);
}


Napi::Object PyNodeInit(Napi::Env env, Napi::Object exports) {

  env.SetInstanceData(new PyNodeEnvData());
  
  exports.Set(Napi::String::New(env, "startInterpreter"),
              Napi::Function::New(env, StartInterpreter));

  exports.Set(Napi::String::New(env, "appendSysPath"),
              Napi::Function::New(env, AppendSysPath));

  exports.Set(Napi::String::New(env, "openFile"),
              Napi::Function::New(env, OpenFile));

  exports.Set(Napi::String::New(env, "import"),
              Napi::Function::New(env, ImportModule));

  exports.Set(Napi::String::New(env, "eval"), Napi::Function::New(env, Eval));

  PyNodeWrappedPythonObject::Init(env, exports);

  return exports;
}
