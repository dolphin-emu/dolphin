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

constexpr PyModuleDef MakeModuleDef(const char* module_name, PyMethodDef func_defs[])
{
  PyModuleDef moduleDefinition{PyModuleDef_HEAD_INIT, module_name, nullptr, -1, func_defs};
  return moduleDefinition;
}

inline PyObject* LoadPyCodeIntoModule(PyObject* module, const char* pycode)
{
  PyObject* globals = PyModule_GetDict(module);
  if (globals == nullptr)
    return nullptr;
  if (PyRun_String(pycode, Py_file_input, globals, globals) == nullptr)
    return nullptr;
  return module;
}

}  // namespace Py
