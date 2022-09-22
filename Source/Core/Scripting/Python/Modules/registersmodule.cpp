// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "registersmodule.h"

#include "Core/API/Registers.h"

#include "Scripting/Python/Utils/as_py_func.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

struct RegistersModuleState
{
};

static PyObject* ReadGPR(PyObject* self, PyObject* args)
{
  auto args_opt = Py::ParseTuple<u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 index = std::get<0>(args_opt.value());
  if (index < 0 || index > 31)
  {
    PyErr_SetString(PyExc_ValueError, "register index is out of range");
    return nullptr;
  }
  PyObject* result = Py::BuildValue(API::Registers::Read_GPR(index));
  return result;
}

static PyObject* WriteGPR(PyObject* self, PyObject* args)
{
  auto args_opt = Py::ParseTuple<u32, u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 index = std::get<0>(args_opt.value());
  if (index < 0 || index > 31)
  {
    PyErr_SetString(PyExc_ValueError, "register index is out of range");
    return nullptr;
  }
  u32 value = std::get<1>(args_opt.value());
  API::Registers::Write_GPR(index, value);
  Py_RETURN_NONE;
}

static PyObject* ReadFPR(PyObject* self, PyObject* args)
{
  auto args_opt = Py::ParseTuple<u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 index = std::get<0>(args_opt.value());
  if (index < 0 || index > 31)
  {
    PyErr_SetString(PyExc_ValueError, "register index is out of range");
    return nullptr;
  }
  PyObject* result = Py::BuildValue(API::Registers::Read_FPR(index));
  return result;
}

static PyObject* WriteFPR(PyObject* self, PyObject* args)
{
  auto args_opt = Py::ParseTuple<u32, double>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 index = std::get<0>(args_opt.value());
  if (index < 0 || index > 31)
  {
    PyErr_SetString(PyExc_ValueError, "register index is out of range");
    return nullptr;
  }
  double value = std::get<1>(args_opt.value());
  API::Registers::Write_FPR(index, value);
  Py_RETURN_NONE;
}

static void SetupRegistersModule(PyObject* module, RegistersModuleState* state)
{
}

PyMODINIT_FUNC PyInit_registers()
{
  static PyMethodDef methods[] = {
      {"read_gpr", ReadGPR, METH_VARARGS, ""},
      {"write_gpr", WriteGPR, METH_VARARGS, ""},
      {"read_fpr", ReadFPR, METH_VARARGS, ""},
      {"write_fpr", WriteFPR, METH_VARARGS, ""},
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<RegistersModuleState, SetupRegistersModule>("registers", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
