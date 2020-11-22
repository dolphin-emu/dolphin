// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file includes some utilities regarding the creation of a python module
// to have a slightly more high-level abstraction over the C API.

#pragma once

#include <Python.h>

#include "Scripting/Python/Utils/as_py_func.h"

namespace Py
{

template <auto TFunc>
constexpr PyMethodDef MakeMethodDef(const char* name, const char* doc = "")
{
  PyMethodDef methodDefinition{name, Py::as_py_func<TFunc>, METH_VARARGS, doc};
  return methodDefinition;
}

inline Py::Object LoadPyCodeIntoModule(PyObject* module, const char* pycode)
{
  PyObject* globals = PyModule_GetDict(module);
  if (globals == nullptr)
    return Py::Null();
  return Py::Wrap(PyRun_String(pycode, Py_file_input, globals, globals));
}

template <typename TState>
using FuncOnState = void(*)(PyObject* module, TState* state);

template <typename TState, FuncOnState<TState> TCleanup>
static PyObject* CleanupModuleWithState(PyObject* module, PyObject* args)
{
  TState** state_ptr = static_cast<TState**>(PyModule_GetState(module));
  TCleanup(module, *state_ptr);
  delete *state_ptr;
  Py_RETURN_NONE;
}

template <typename TState, FuncOnState<TState> TSetup, FuncOnState<TState> TCleanup>
static int SetupModuleWithState(PyObject* module)
{
  TState** state_ptr = static_cast<TState**>(PyModule_GetState(module));
  *state_ptr = new TState();

  // There doesn't seem to be a C-API to register atexit-callbacks,
  // so let's do it in python code. atexit-callbacks are interpreter-local.
  static PyMethodDef cleanup_module_methods[] = {
      {"_dol_mod_cleanup", CleanupModuleWithState<TState, TCleanup>, METH_NOARGS, ""},
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  PyModule_AddFunctions(module, cleanup_module_methods);
  Py::Object result = LoadPyCodeIntoModule(module, R"(
import atexit
atexit.register(_dol_mod_cleanup)
  )");
  if (result.IsNull())
  {
    return -1;
  }

  TSetup(module, *state_ptr);
  return 0;
}

template <typename TState, FuncOnState<TState> TSetup, FuncOnState<TState> TCleanup>
PyModuleDef MakeStatefulModuleDef(const char* name, PyMethodDef func_defs[])
{
  auto func = SetupModuleWithState<TState, TSetup, TCleanup>;
  static PyModuleDef_Slot slots_with_exec[] = {
      {Py_mod_exec, (void*) func}, {0, nullptr} // Sentinel
  };
  static PyModuleDef moduleDefinition{
      PyModuleDef_HEAD_INIT,
      name,
      nullptr,  // m_doc
      sizeof(TState*),
      func_defs,
      slots_with_exec,
      nullptr,  // m_traverse
      nullptr,  // m_clear
      nullptr   // m_free
  };
  return moduleDefinition;
}

template<typename TState>
static TState* GetState(PyObject* module)
{
  return *static_cast<TState**>(PyModule_GetState(module));
}

}  // namespace Py
