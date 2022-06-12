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

template <typename TState, FuncOnState<TState> TSetup>
static int SetupModuleWithState(PyObject* module)
{
  TState** state_ptr = static_cast<TState**>(PyModule_GetState(module));
  *state_ptr = new TState();

  TSetup(module, *state_ptr);
  return 0;
}

template <typename TState, FuncOnState<TState> TSetup>
PyModuleDef MakeStatefulModuleDef(const char* name, PyMethodDef func_defs[])
{
  auto func = SetupModuleWithState<TState, TSetup>;
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
