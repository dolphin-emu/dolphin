// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/API/Memory.h"
#include "Core/HW/Memmap.h"

#include "memorymodule.h"
#include "Scripting/Python/Utils/module.h"
#include "Scripting/Python/Utils/as_py_func.h"

namespace PyScripting
{

template <auto T>
static PyObject* WithMemoryInitCheck(PyObject* self, PyObject* args)
{
  if (!Memory::IsInitialized())
  {
    PyErr_SetString(PyExc_ValueError, "memory is not initialized");
    return nullptr;
  }
  return Py::as_py_func<T>(self, args);
}

static PyMethodDef MemoryMethods[] = {
    {"add_memcheck", WithMemoryInitCheck<API::Memory::AddMemcheck>, METH_VARARGS, ""},
    {"remove_memcheck", WithMemoryInitCheck<API::Memory::RemoveMemcheck>, METH_VARARGS, ""},

    {"read_u8", WithMemoryInitCheck<API::Memory::Read_U8>, METH_VARARGS, ""},
    {"read_u16", WithMemoryInitCheck<API::Memory::Read_U16>, METH_VARARGS, ""},
    {"read_u32", WithMemoryInitCheck<API::Memory::Read_U32>, METH_VARARGS, ""},
    {"read_u64", WithMemoryInitCheck<API::Memory::Read_U64>, METH_VARARGS, ""},

    {"read_s8", WithMemoryInitCheck<API::Memory::Read_S8>, METH_VARARGS, ""},
    {"read_s16", WithMemoryInitCheck<API::Memory::Read_S16>, METH_VARARGS, ""},
    {"read_s32", WithMemoryInitCheck<API::Memory::Read_S32>, METH_VARARGS, ""},
    {"read_s64", WithMemoryInitCheck<API::Memory::Read_S64>, METH_VARARGS, ""},

    {"read_f32", WithMemoryInitCheck<API::Memory::Read_F32>, METH_VARARGS, ""},
    {"read_f64", WithMemoryInitCheck<API::Memory::Read_F64>, METH_VARARGS, ""},

    {"write_u8", WithMemoryInitCheck<API::Memory::Write_U8>, METH_VARARGS, ""},
    {"write_u16", WithMemoryInitCheck<API::Memory::Write_U16>, METH_VARARGS, ""},
    {"write_u32", WithMemoryInitCheck<API::Memory::Write_U32>, METH_VARARGS, ""},
    {"write_u64", WithMemoryInitCheck<API::Memory::Write_U64>, METH_VARARGS, ""},

    {"write_s8", WithMemoryInitCheck<API::Memory::Write_S8>, METH_VARARGS, ""},
    {"write_s16", WithMemoryInitCheck<API::Memory::Write_S16>, METH_VARARGS, ""},
    {"write_s32", WithMemoryInitCheck<API::Memory::Write_S32>, METH_VARARGS, ""},
    {"write_s64", WithMemoryInitCheck<API::Memory::Write_S64>, METH_VARARGS, ""},

    {"write_f32", WithMemoryInitCheck<API::Memory::Write_F32>, METH_VARARGS, ""},
    {"write_f64", WithMemoryInitCheck<API::Memory::Write_F64>, METH_VARARGS, ""},

    {nullptr, nullptr, 0, nullptr}  // Sentinel
};

PyMODINIT_FUNC PyInit_memory()
{
  static PyModuleDef def = Py::MakeModuleDef("memory", MemoryMethods);
  return PyModule_Create(&def);
}

}  // namespace PyScripting
