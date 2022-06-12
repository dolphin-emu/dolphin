// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "memorymodule.h"

#include "Core/API/Memory.h"
#include "Core/HW/Memmap.h"

#include "Scripting/Python/Utils/module.h"
#include "Scripting/Python/Utils/as_py_func.h"

namespace PyScripting
{

struct MemoryModuleState
{
  // If Memory wasn't static, you'd store the memory instance here:
  //API::Memory* memory;
};

template <auto TRead>
static PyObject* Read(PyObject* self, PyObject* args)
{
  // If Memory wasn't static, you'd get the memory instance from the state:
  //MemoryModuleState* state = Py::GetState<MemoryModuleState>();
  if (!Memory::IsInitialized())
  {
    PyErr_SetString(PyExc_ValueError, "memory is not initialized");
    return nullptr;
  }
  auto args_opt = Py::ParseTuple<u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 addr = std::get<0>(args_opt.value());
  PyObject* result = Py::BuildValue(TRead(addr));
  return result;
}

template <auto TWrite, typename T>
static PyObject* Write(PyObject* self, PyObject* args)
{
  // If Memory wasn't static, you'd get the memory instance from the state:
  //MemoryModuleState* state = Py::GetState<MemoryModuleState>();
  if (!Memory::IsInitialized())
  {
    PyErr_SetString(PyExc_ValueError, "memory is not initialized");
    return nullptr;
  }
  auto args_opt = Py::ParseTuple<u32, T>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 addr = std::get<0>(args_opt.value());
  T value = std::get<1>(args_opt.value());
  TWrite(addr, value);
  Py_RETURN_NONE;
}

static PyObject* AddMemcheck(PyObject* self, PyObject* args)
{
  // If Memory wasn't static, you'd get the memory instance from the state:
  // MemoryModuleState* state = Py::GetState<MemoryModuleState>();
  auto args_opt = Py::ParseTuple<u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 addr = std::get<0>(args_opt.value());
  API::Memory::AddMemcheck(addr);
  Py_RETURN_NONE;
}

static PyObject* RemoveMemcheck(PyObject* self, PyObject* args)
{
  // If Memory wasn't static, you'd get the memory instance from the state:
  // MemoryModuleState* state = Py::GetState<MemoryModuleState>();
  auto args_opt = Py::ParseTuple<u32>(args);
  if (!args_opt.has_value())
    return nullptr;
  u32 addr = std::get<0>(args_opt.value());
  API::Memory::RemoveMemcheck(addr);
  Py_RETURN_NONE;
}

static void SetupMemoryModule(PyObject* module, MemoryModuleState* state)
{
  // If Memory wasn't static, you'd store the memory instance in the state:
  //API::Memory* memory = PyScripting::PyScriptingBackend::GetCurrent()->GetMemory();
  //state->memory = memory;
}

PyMODINIT_FUNC PyInit_memory()
{
  static PyMethodDef methods[] = {
      {"add_memcheck", AddMemcheck, METH_VARARGS, ""},
      {"remove_memcheck", RemoveMemcheck, METH_VARARGS, ""},

      {"read_u8", Read<API::Memory::Read_U8>, METH_VARARGS, ""},
      {"read_u16", Read<API::Memory::Read_U16>, METH_VARARGS, ""},
      {"read_u32", Read<API::Memory::Read_U32>, METH_VARARGS, ""},
      {"read_u64", Read<API::Memory::Read_U64>, METH_VARARGS, ""},

      {"read_s8", Read<API::Memory::Read_S8>, METH_VARARGS, ""},
      {"read_s16", Read<API::Memory::Read_S16>, METH_VARARGS, ""},
      {"read_s32", Read<API::Memory::Read_S32>, METH_VARARGS, ""},
      {"read_s64", Read<API::Memory::Read_S64>, METH_VARARGS, ""},

      {"read_f32", Read<API::Memory::Read_F32>, METH_VARARGS, ""},
      {"read_f64", Read<API::Memory::Read_F64>, METH_VARARGS, ""},

      {"write_u8", Write<API::Memory::Write_U8, u8>, METH_VARARGS, ""},
      {"write_u16", Write<API::Memory::Write_U16, u16>, METH_VARARGS, ""},
      {"write_u32", Write<API::Memory::Write_U32, u32>, METH_VARARGS, ""},
      {"write_u64", Write<API::Memory::Write_U64, u64>, METH_VARARGS, ""},

      {"write_s8", Write<API::Memory::Write_S8, s8>, METH_VARARGS, ""},
      {"write_s16", Write<API::Memory::Write_S16, s16>, METH_VARARGS, ""},
      {"write_s32", Write<API::Memory::Write_S32, s32>, METH_VARARGS, ""},
      {"write_s64", Write<API::Memory::Write_S64, s64>, METH_VARARGS, ""},

      {"write_f32", Write<API::Memory::Write_F32, float>, METH_VARARGS, ""},
      {"write_f64", Write<API::Memory::Write_F64, double>, METH_VARARGS, ""},

      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<MemoryModuleState, SetupMemoryModule>("memory", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
