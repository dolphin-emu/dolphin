// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "savestatemodule.h"

#include "Common/Logging/Log.h"
#include "Core/State.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

struct SavestateModuleState
{
};

void SetupSavestateModule(PyObject* module, SavestateModuleState* state)
{
}

void CleanupSavestateModule(PyObject* module, SavestateModuleState* state)
{
}

PyObject* SaveToSlot(PyObject* self, PyObject* args)
{
  auto slot_opt = Py::ParseTuple<u32>(args);
  if (!slot_opt.has_value())
    return nullptr;
  u32 slot = std::get<0>(slot_opt.value());
  if (slot < 0 || slot > 99)
  {
    PyErr_SetString(PyExc_ValueError, "slot number must be between 0 and 99");
    return nullptr;
  }
  State::Save(slot);
  Py_RETURN_NONE;
}

PyObject* LoadFromSlot(PyObject* self, PyObject* args)
{
  auto slot_opt = Py::ParseTuple<u32>(args);
  if (!slot_opt.has_value())
    return nullptr;
  u32 slot = std::get<0>(slot_opt.value());
  if (slot < 0 || slot > 99)
  {
    PyErr_SetString(PyExc_ValueError, "slot number must be between 0 and 99");
    return nullptr;
  }
  State::Load(slot);
  Py_RETURN_NONE;
}

PyObject* SaveToFile(PyObject* self, PyObject* args)
{
  auto filename_opt = Py::ParseTuple<const char*>(args);
  if (!filename_opt.has_value())
    return nullptr;
  const char* filename = std::get<0>(filename_opt.value());
  State::SaveAs(std::string(filename));
  Py_RETURN_NONE;
}

PyObject* LoadFromFile(PyObject* self, PyObject* args)
{
  auto filename_opt = Py::ParseTuple<const char*>(args);
  if (!filename_opt.has_value())
    return nullptr;
  const char* filename = std::get<0>(filename_opt.value());
  State::LoadAs(std::string(filename));
  Py_RETURN_NONE;
}

PyObject* SaveToBytes(PyObject* self, PyObject* args)
{
  std::vector<u8> buffer;
  State::SaveToBuffer(buffer);
  const u8* data = buffer.data();
  PyObject* pybytes = PyBytes_FromStringAndSize(reinterpret_cast<const char*>(data), buffer.size());
  if (pybytes == nullptr)
  {
    ERROR_LOG(SCRIPTING, "Failed to turn buffer into python bytes object");
    return nullptr;
  }
  return pybytes;
}

PyObject* LoadFromBytes(PyObject* self, PyObject* args)
{
  auto bytes_opt = Py::ParseTuple<PyBytesObject*>(args);
  if (!bytes_opt.has_value())
    return nullptr;
  PyObject* pybytes = (PyObject*)std::get<0>(bytes_opt.value());
  auto length = PyBytes_Size(pybytes);

  std::vector<u8> buffer(length, 0);
  u8* data = buffer.data();
  u32 result = PyBytes_AsStringAndSize(pybytes, reinterpret_cast<char**>(&data), &length);
  if (result == -1)
    return nullptr;
  // I don't understand where and why the buffer gets copied and why this is necessary...
  buffer.assign(data, data+length); 
  State::LoadFromBuffer(buffer);
  Py_RETURN_NONE;
}

PyMODINIT_FUNC PyInit_savestate()
{
  static PyMethodDef methods[] = {
      {"save_to_slot", SaveToSlot, METH_VARARGS, ""},
      {"save_to_file", SaveToFile, METH_VARARGS, ""},
      {"save_to_bytes", SaveToBytes, METH_NOARGS, ""},
      {"load_from_slot", LoadFromSlot, METH_VARARGS, ""},
      {"load_from_file", LoadFromFile, METH_VARARGS, ""},
      {"load_from_bytes", LoadFromBytes, METH_VARARGS, ""},

      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<SavestateModuleState, SetupSavestateModule, CleanupSavestateModule>(
          "savestate", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
