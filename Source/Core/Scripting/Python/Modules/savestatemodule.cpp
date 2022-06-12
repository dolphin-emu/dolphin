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
  // If State wasn't static, you'd store an instance here:
  //API::SavestateManager* stateManager; // or however it would be called
};

static PyObject* SaveToSlot(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  auto slot_opt = Py::ParseTuple<u32>(args);
  if (!slot_opt.has_value())
    return nullptr;
  u32 slot = std::get<0>(slot_opt.value());
  if (slot > 99)
  {
    PyErr_SetString(PyExc_ValueError, "slot number must be between 0 and 99");
    return nullptr;
  }
  State::Save(slot);
  Py_RETURN_NONE;
}

static PyObject* LoadFromSlot(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  auto slot_opt = Py::ParseTuple<u32>(args);
  if (!slot_opt.has_value())
    return nullptr;
  u32 slot = std::get<0>(slot_opt.value());
  if (slot > 99)
  {
    PyErr_SetString(PyExc_ValueError, "slot number must be between 0 and 99");
    return nullptr;
  }
  State::Load(slot);
  Py_RETURN_NONE;
}

static PyObject* SaveToFile(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  auto filename_opt = Py::ParseTuple<const char*>(args);
  if (!filename_opt.has_value())
    return nullptr;
  const char* filename = std::get<0>(filename_opt.value());
  State::SaveAs(std::string(filename));
  Py_RETURN_NONE;
}

static PyObject* LoadFromFile(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  auto filename_opt = Py::ParseTuple<const char*>(args);
  if (!filename_opt.has_value())
    return nullptr;
  const char* filename = std::get<0>(filename_opt.value());
  State::LoadAs(std::string(filename));
  Py_RETURN_NONE;
}

static PyObject* SaveToBytes(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  std::vector<u8> buffer;
  State::SaveToBuffer(buffer);
  const u8* data = buffer.data();
  PyObject* pybytes = PyBytes_FromStringAndSize(reinterpret_cast<const char*>(data), buffer.size());
  if (pybytes == nullptr)
  {
    ERROR_LOG_FMT(SCRIPTING, "Failed to turn buffer into python bytes object");
    return nullptr;
  }
  return pybytes;
}

static PyObject* LoadFromBytes(PyObject* self, PyObject* args)
{
  // If State wasn't static, you'd get the state-manager instance from the module state:
  //SavestateModuleState* state = Py::GetState<SavestateModuleState>();
  auto bytes_opt = Py::ParseTuple<PyBytesObject*>(args);
  if (!bytes_opt.has_value())
    return nullptr;
  PyObject* pybytes = (PyObject*)std::get<0>(bytes_opt.value());
  auto length = PyBytes_Size(pybytes);

  std::vector<u8> buffer(length, 0);
  u8* data = buffer.data();
  int result = PyBytes_AsStringAndSize(pybytes, reinterpret_cast<char**>(&data), &length);
  if (result == -1)
    return nullptr;
  // I don't understand where and why the buffer gets copied and why this is necessary...
  buffer.assign(data, data+length);
  State::LoadFromBuffer(buffer);
  Py_RETURN_NONE;
}

static void SetupSavestateModule(PyObject* module, SavestateModuleState* state)
{
  // If State wasn't static, you'd store a state manager instance in the module state:
  //API::StateManager* sm = PyScripting::PyScriptingBackend::GetCurrent()->GetStateManager();
  //state->stateManager = sm;
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
      Py::MakeStatefulModuleDef<SavestateModuleState, SetupSavestateModule>("savestate", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
