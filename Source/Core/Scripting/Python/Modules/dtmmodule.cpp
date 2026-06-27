// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dtmmodule.h"

#include <algorithm>
#include <limits>

#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/System.h"

#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{
namespace
{
struct DTMModuleState
{
};

PyObject* BoolObject(const bool value)
{
  return Py_NewRef(value ? Py_True : Py_False);
}

template <typename Setter>
bool SetBoolFromDict(PyObject* dict, const char* key, Setter setter)
{
  PyObject* item = PyDict_GetItemString(dict, key);
  if (!item)
    return true;

  const int truth = PyObject_IsTrue(item);
  if (truth < 0)
    return false;

  setter(truth != 0);
  return true;
}

template <typename T>
bool SetClampedIntFromDict(PyObject* dict, const char* key, T* value)
{
  PyObject* item = PyDict_GetItemString(dict, key);
  if (!item)
    return true;

  const unsigned long raw = PyLong_AsUnsignedLong(item);
  if (PyErr_Occurred())
    return false;

  constexpr unsigned long max_value = std::numeric_limits<T>::max();
  *value = static_cast<T>(std::min(raw, max_value));
  return true;
}

PyObject* ControllerStateToPyDict(const Movie::ControllerState& state)
{
  PyObject* dict = PyDict_New();
  if (!dict)
    return nullptr;

  const auto set_bool = [dict](const char* key, bool value) {
    PyObject* object = BoolObject(value);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
  };
  const auto set_int = [dict](const char* key, u8 value) {
    PyObject* object = PyLong_FromUnsignedLong(value);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
  };

  set_bool("Connected", state.is_connected);
  set_bool("Start", state.Start);
  set_bool("A", state.A);
  set_bool("B", state.B);
  set_bool("X", state.X);
  set_bool("Y", state.Y);
  set_bool("Z", state.Z);
  set_bool("L", state.L);
  set_bool("R", state.R);
  set_bool("Up", state.DPadUp);
  set_bool("Down", state.DPadDown);
  set_bool("Left", state.DPadLeft);
  set_bool("Right", state.DPadRight);
  set_bool("Disc", state.disc);
  set_bool("Reset", state.reset);
  set_bool("GetOrigin", state.get_origin);
  set_int("StickX", state.AnalogStickX);
  set_int("StickY", state.AnalogStickY);
  set_int("CStickX", state.CStickX);
  set_int("CStickY", state.CStickY);
  set_int("TriggerLeft", state.TriggerL);
  set_int("TriggerRight", state.TriggerR);

  return dict;
}

bool ApplyControllerStateDict(Movie::ControllerState* state, PyObject* dict)
{
  return SetBoolFromDict(dict, "Connected", [&](bool value) { state->is_connected = value; }) &&
         SetBoolFromDict(dict, "Start", [&](bool value) { state->Start = value; }) &&
         SetBoolFromDict(dict, "A", [&](bool value) { state->A = value; }) &&
         SetBoolFromDict(dict, "B", [&](bool value) { state->B = value; }) &&
         SetBoolFromDict(dict, "X", [&](bool value) { state->X = value; }) &&
         SetBoolFromDict(dict, "Y", [&](bool value) { state->Y = value; }) &&
         SetBoolFromDict(dict, "Z", [&](bool value) { state->Z = value; }) &&
         SetBoolFromDict(dict, "L", [&](bool value) { state->L = value; }) &&
         SetBoolFromDict(dict, "R", [&](bool value) { state->R = value; }) &&
         SetBoolFromDict(dict, "Up", [&](bool value) { state->DPadUp = value; }) &&
         SetBoolFromDict(dict, "Down", [&](bool value) { state->DPadDown = value; }) &&
         SetBoolFromDict(dict, "Left", [&](bool value) { state->DPadLeft = value; }) &&
         SetBoolFromDict(dict, "Right", [&](bool value) { state->DPadRight = value; }) &&
         SetBoolFromDict(dict, "Disc", [&](bool value) { state->disc = value; }) &&
         SetBoolFromDict(dict, "Reset", [&](bool value) { state->reset = value; }) &&
         SetBoolFromDict(dict, "GetOrigin", [&](bool value) { state->get_origin = value; }) &&
         SetClampedIntFromDict(dict, "StickX", &state->AnalogStickX) &&
         SetClampedIntFromDict(dict, "StickY", &state->AnalogStickY) &&
         SetClampedIntFromDict(dict, "CStickX", &state->CStickX) &&
         SetClampedIntFromDict(dict, "CStickY", &state->CStickY) &&
         SetClampedIntFromDict(dict, "TriggerLeft", &state->TriggerL) &&
         SetClampedIntFromDict(dict, "TriggerRight", &state->TriggerR);
}

PyObject* WiiButtonsToPyDict(const WiimoteCommon::ButtonData& buttons)
{
  PyObject* dict = PyDict_New();
  if (!dict)
    return nullptr;

  const auto set_bool = [dict](const char* key, bool value) {
    PyObject* object = BoolObject(value);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
  };

  set_bool("Left", buttons.left);
  set_bool("Right", buttons.right);
  set_bool("Down", buttons.down);
  set_bool("Up", buttons.up);
  set_bool("Plus", buttons.plus);
  set_bool("Two", buttons.two);
  set_bool("One", buttons.one);
  set_bool("B", buttons.b);
  set_bool("A", buttons.a);
  set_bool("Minus", buttons.minus);
  set_bool("Home", buttons.home);
  return dict;
}

bool ApplyWiiButtonsDict(WiimoteCommon::ButtonData* buttons, PyObject* dict)
{
  return SetBoolFromDict(dict, "Left", [&](bool value) { buttons->left = value; }) &&
         SetBoolFromDict(dict, "Right", [&](bool value) { buttons->right = value; }) &&
         SetBoolFromDict(dict, "Down", [&](bool value) { buttons->down = value; }) &&
         SetBoolFromDict(dict, "Up", [&](bool value) { buttons->up = value; }) &&
         SetBoolFromDict(dict, "Plus", [&](bool value) { buttons->plus = value; }) &&
         SetBoolFromDict(dict, "Two", [&](bool value) { buttons->two = value; }) &&
         SetBoolFromDict(dict, "One", [&](bool value) { buttons->one = value; }) &&
         SetBoolFromDict(dict, "B", [&](bool value) { buttons->b = value; }) &&
         SetBoolFromDict(dict, "A", [&](bool value) { buttons->a = value; }) &&
         SetBoolFromDict(dict, "Minus", [&](bool value) { buttons->minus = value; }) &&
         SetBoolFromDict(dict, "Home", [&](bool value) { buttons->home = value; });
}

template <typename TMetadata>
PyObject* MetadataToPyDict(const char* kind, const TMetadata& metadata)
{
  PyObject* dict = Py_BuildValue("{s:s,s:K,s:K,s:K,s:K,s:O,s:O,s:O}", "Kind", kind, "RowCount",
                                 metadata.row_count, "CurrentFrame", metadata.current_frame,
                                 "CurrentInputRow", metadata.current_input_row, "DataGeneration",
                                 metadata.data_generation, "IsRecording",
                                 metadata.is_recording ? Py_True : Py_False, "IsPlaying",
                                 metadata.is_playing ? Py_True : Py_False, "IsReadOnly",
                                 metadata.is_read_only ? Py_True : Py_False);
  if (!dict)
    return nullptr;

  const auto set_alias = [dict](const char* alias, const char* original) {
    PyObject* value = PyDict_GetItemString(dict, original);
    if (value)
      PyDict_SetItemString(dict, alias, value);
  };
  set_alias("kind", "Kind");
  set_alias("row_count", "RowCount");
  set_alias("current_frame", "CurrentFrame");
  set_alias("current_input_row", "CurrentInputRow");
  set_alias("data_generation", "DataGeneration");
  set_alias("is_recording", "IsRecording");
  set_alias("is_playing", "IsPlaying");
  set_alias("is_read_only", "IsReadOnly");

  return dict;
}

template <typename TArray>
void AddActiveList(PyObject* dict, const char* name, const TArray& active)
{
  PyObject* list = PyList_New(active.size());
  if (!list)
    return;

  for (size_t i = 0; i < active.size(); ++i)
    PyList_SetItem(list, static_cast<Py_ssize_t>(i), BoolObject(active[i]));

  PyDict_SetItemString(dict, name, list);
  Py_DECREF(list);
}

Movie::MovieManager& GetMovie()
{
  return Core::System::GetInstance().GetMovie();
}

static PyObject* get_metadata(PyObject*, PyObject*)
{
  auto& movie = GetMovie();
  if (const auto gc = movie.GetGCRuntimeFrameMetadata())
  {
    PyObject* dict = MetadataToPyDict("gc", *gc);
    if (dict)
    {
      AddActiveList(dict, "ActiveControllers", gc->active_controllers);
      if (PyObject* active = PyDict_GetItemString(dict, "ActiveControllers"))
        PyDict_SetItemString(dict, "active_controllers", active);
    }
    return dict;
  }

  if (const auto wii = movie.GetWiiRuntimeFrameMetadata())
  {
    PyObject* dict = MetadataToPyDict("wii", *wii);
    if (dict)
    {
      AddActiveList(dict, "ActiveWiimotes", wii->active_wiimotes);
      if (PyObject* active = PyDict_GetItemString(dict, "ActiveWiimotes"))
        PyDict_SetItemString(dict, "active_wiimotes", active);
    }
    return dict;
  }

  Py_RETURN_NONE;
}

static PyObject* get_kind(PyObject*, PyObject*)
{
  auto& movie = GetMovie();
  if (movie.GetGCRuntimeFrameMetadata())
    return PyUnicode_FromString("gc");
  if (movie.GetWiiRuntimeFrameMetadata())
    return PyUnicode_FromString("wii");
  Py_RETURN_NONE;
}

static PyObject* get_gc(PyObject*, PyObject* args)
{
  unsigned long long row;
  int controller;
  if (!PyArg_ParseTuple(args, "Ki", &row, &controller))
    return nullptr;

  const auto row_data = GetMovie().GetGCRuntimeFrameRow(row);
  if (!row_data.has_value() || controller < 0 || controller >= 4)
    Py_RETURN_NONE;

  return ControllerStateToPyDict(row_data->at(static_cast<size_t>(controller)));
}

static PyObject* set_gc(PyObject*, PyObject* args)
{
  unsigned long long row;
  int controller;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "KiO!", &row, &controller, &PyDict_Type, &dict))
    return nullptr;

  auto& movie = GetMovie();
  const auto row_data = movie.GetGCRuntimeFrameRow(row);
  if (!row_data.has_value() || controller < 0 || controller >= 4)
    Py_RETURN_FALSE;

  Movie::ControllerState state = row_data->at(static_cast<size_t>(controller));
  if (!ApplyControllerStateDict(&state, dict))
    return nullptr;

  if (!movie.SetGCRuntimeFrameState(row, controller, state))
    Py_RETURN_FALSE;

  Py_RETURN_TRUE;
}

static PyObject* get_wii(PyObject*, PyObject* args)
{
  unsigned long long row;
  if (!PyArg_ParseTuple(args, "K", &row))
    return nullptr;

  const auto row_data = GetMovie().GetWiiRuntimeFrameRow(row);
  if (!row_data.has_value())
    Py_RETURN_NONE;

  PyObject* dict = Py_BuildValue("{s:i,s:O}", "Wiimote", row_data->wiimote, "Reset",
                                 row_data->is_reset ? Py_True : Py_False);
  if (!dict)
    return nullptr;

  PyObject* serialized =
      PyBytes_FromStringAndSize(reinterpret_cast<const char*>(row_data->serialized_state.data.data()),
                                row_data->is_reset ? 0 : row_data->serialized_state.length);
  PyDict_SetItemString(dict, "SerializedState", serialized);
  PyDict_SetItemString(dict, "serialized_state", serialized);
  Py_DECREF(serialized);

  if (!row_data->is_reset)
  {
    WiimoteEmu::DesiredWiimoteState state;
    if (WiimoteEmu::DeserializeDesiredState(&state, row_data->serialized_state))
    {
      PyObject* buttons = WiiButtonsToPyDict(state.buttons);
      PyDict_SetItemString(dict, "Buttons", buttons);
      PyDict_SetItemString(dict, "buttons", buttons);
      Py_DECREF(buttons);
    }
  }

  return dict;
}

static PyObject* get_wii_buttons(PyObject*, PyObject* args)
{
  unsigned long long row;
  if (!PyArg_ParseTuple(args, "K", &row))
    return nullptr;

  const auto row_data = GetMovie().GetWiiRuntimeFrameRow(row);
  if (!row_data.has_value() || row_data->is_reset)
    Py_RETURN_NONE;

  WiimoteEmu::DesiredWiimoteState state;
  if (!WiimoteEmu::DeserializeDesiredState(&state, row_data->serialized_state))
    Py_RETURN_NONE;

  return WiiButtonsToPyDict(state.buttons);
}

static PyObject* set_wii_buttons(PyObject*, PyObject* args)
{
  unsigned long long row;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "KO!", &row, &PyDict_Type, &dict))
    return nullptr;

  auto& movie = GetMovie();
  const auto row_data = movie.GetWiiRuntimeFrameRow(row);
  if (!row_data.has_value())
    Py_RETURN_FALSE;

  WiimoteEmu::DesiredWiimoteState state;
  if (!row_data->is_reset && !WiimoteEmu::DeserializeDesiredState(&state, row_data->serialized_state))
    Py_RETURN_FALSE;

  if (!ApplyWiiButtonsDict(&state.buttons, dict))
    return nullptr;

  if (!movie.SetWiiRuntimeFrameState(row, WiimoteEmu::SerializeDesiredState(state)))
    Py_RETURN_FALSE;

  Py_RETURN_TRUE;
}

static PyObject* set_wii_serialized(PyObject*, PyObject* args)
{
  unsigned long long row;
  PyObject* bytes_object;
  if (!PyArg_ParseTuple(args, "KO!", &row, &PyBytes_Type, &bytes_object))
    return nullptr;

  char* bytes;
  Py_ssize_t length;
  if (PyBytes_AsStringAndSize(bytes_object, &bytes, &length) < 0)
    return nullptr;

  WiimoteEmu::SerializedWiimoteState serialized{};
  if (length < 0 || static_cast<size_t>(length) > serialized.data.size())
  {
    PyErr_SetString(PyExc_ValueError, "serialized Wii state must be at most 31 bytes");
    return nullptr;
  }

  serialized.length = static_cast<u8>(length);
  std::copy_n(reinterpret_cast<const u8*>(bytes), static_cast<size_t>(length),
              serialized.data.data());

  if (!GetMovie().SetWiiRuntimeFrameState(row, serialized))
    Py_RETURN_FALSE;

  Py_RETURN_TRUE;
}

static PyObject* save_to_savestate_slot(PyObject*, PyObject* args)
{
  unsigned int slot;
  if (!PyArg_ParseTuple(args, "I", &slot))
    return nullptr;

  if (slot > 99)
  {
    PyErr_SetString(PyExc_ValueError, "slot number must be between 0 and 99");
    return nullptr;
  }

  return BoolObject(State::SaveMovieToSlot(Core::System::GetInstance(), static_cast<int>(slot)));
}

static void setup_dtm_module(PyObject*, DTMModuleState*)
{
}
}  // namespace

PyMODINIT_FUNC PyInit_dtm()
{
  static PyMethodDef method_defs[] = {
      {"get_metadata", get_metadata, METH_NOARGS, "Return metadata for the active live DTM."},
      {"get_kind", get_kind, METH_NOARGS, "Return 'gc', 'wii', or None for the active live DTM."},
      {"get_gc", get_gc, METH_VARARGS,
       "get_gc(row, controller) -> dict. Return a GameCube controller row state."},
      {"set_gc", set_gc, METH_VARARGS,
       "set_gc(row, controller, dict) -> bool. Partially update a GameCube controller row state."},
      {"get_wii", get_wii, METH_VARARGS, "get_wii(row) -> dict. Return a Wii input row."},
      {"get_wii_buttons", get_wii_buttons, METH_VARARGS,
       "get_wii_buttons(row) -> dict. Return a Wii row's button state."},
      {"set_wii_buttons", set_wii_buttons, METH_VARARGS,
       "set_wii_buttons(row, dict) -> bool. Partially update a Wii row's buttons."},
      {"set_wii_serialized", set_wii_serialized, METH_VARARGS,
       "set_wii_serialized(row, bytes) -> bool. Replace a Wii row's serialized input state."},
      {"save_to_savestate_slot", save_to_savestate_slot, METH_VARARGS,
       "save_to_savestate_slot(slot) -> bool. Save the active live DTM beside a savestate slot."},
      {nullptr, nullptr, 0, nullptr},
  };

  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<DTMModuleState, setup_dtm_module>("dtm", method_defs);
  return PyModuleDef_Init(&module_def);
}
}  // namespace PyScripting
