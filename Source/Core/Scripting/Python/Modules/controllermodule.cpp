// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "controllermodule.h"

#include "Core/API/Controller.h"
#include "Common/Logging/Log.h"
#include "Scripting/Python/PyScriptingBackend.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{
struct ControllerModuleState
{
  API::GCManip* gc_manip;
};

PyObject* GCPadStatusToPyDict(GCPadStatus status) {
  return Py_BuildValue("{s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,"
                       "s:B,s:B,s:B,s:B,s:B,s:B,s:B,s:B,s:O}",
      "Left", status.button & PAD_BUTTON_LEFT ? Py_True : Py_False,
      "Right", status.button & PAD_BUTTON_RIGHT ? Py_True : Py_False,
      "Down", status.button & PAD_BUTTON_DOWN ? Py_True : Py_False,
      "Up", status.button & PAD_BUTTON_UP ? Py_True : Py_False,
      "Z", status.button & PAD_TRIGGER_Z ? Py_True : Py_False,
      "R", status.button & PAD_TRIGGER_R ? Py_True : Py_False,
      "L", status.button & PAD_TRIGGER_L ? Py_True : Py_False,
      "A", status.button & PAD_BUTTON_A ? Py_True : Py_False,
      "B", status.button & PAD_BUTTON_B ? Py_True : Py_False,
      "X", status.button & PAD_BUTTON_X ? Py_True : Py_False,
      "Y", status.button & PAD_BUTTON_Y ? Py_True : Py_False,
      "Start", status.button & PAD_BUTTON_START ? Py_True : Py_False,

      "StickX", status.stickX,
      "StickY", status.stickY,
      "CStickX", status.substickX,
      "CStickY", status.substickY,
      "TriggerLeft", status.triggerLeft,
      "TriggerRight", status.triggerRight,
      "AnalogA", status.analogA,
      "AnalogB", status.analogB,

      "Connected", status.isConnected ? Py_True : Py_False
  );
}

GCPadStatus GCPadStatusFromPyDict(PyObject* dict) {
  bool button_left = PyObject_IsTrue(PyDict_GetItemString(dict, "Left"));
  bool button_right = PyObject_IsTrue(PyDict_GetItemString(dict, "Right"));
  bool button_down = PyObject_IsTrue(PyDict_GetItemString(dict, "Down"));
  bool button_up = PyObject_IsTrue(PyDict_GetItemString(dict, "Up"));
  bool trigger_z = PyObject_IsTrue(PyDict_GetItemString(dict, "Z"));
  bool trigger_r = PyObject_IsTrue(PyDict_GetItemString(dict, "R"));
  bool trigger_l = PyObject_IsTrue(PyDict_GetItemString(dict, "L"));
  bool button_a = PyObject_IsTrue(PyDict_GetItemString(dict, "A"));
  bool button_b = PyObject_IsTrue(PyDict_GetItemString(dict, "B"));
  bool button_x = PyObject_IsTrue(PyDict_GetItemString(dict, "X"));
  bool button_y = PyObject_IsTrue(PyDict_GetItemString(dict, "Y"));
  bool button_start = PyObject_IsTrue(PyDict_GetItemString(dict, "Start"));

  u8 stick_x = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "StickX"));
  u8 stick_y = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "StickY"));
  u8 substick_x = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "CStickX"));
  u8 substick_y = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "CStickY"));
  u8 trigger_left = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "TriggerLeft"));
  u8 trigger_right = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "TriggerRight"));
  u8 analog_a = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "AnalogA"));
  u8 analog_b = PyLong_AsUnsignedLong(PyDict_GetItemString(dict, "AnalogB"));

  bool connected = PyObject_IsTrue(PyDict_GetItemString(dict, "Connected"));

  GCPadStatus status;
  status.button = (button_left ? PAD_BUTTON_LEFT : 0) | (button_right ? PAD_BUTTON_RIGHT : 0) |
                  (button_down ? PAD_BUTTON_DOWN : 0) | (button_up ? PAD_BUTTON_UP : 0) |
                  (trigger_z ? PAD_TRIGGER_Z : 0) | (trigger_r ? PAD_TRIGGER_R : 0) |
                  (trigger_l ? PAD_TRIGGER_L : 0) | (button_a ? PAD_BUTTON_A : 0) |
                  (button_b ? PAD_BUTTON_B : 0) | (button_x ? PAD_BUTTON_X : 0) |
                  (button_y ? PAD_BUTTON_Y : 0) | (button_start ? PAD_BUTTON_START : 0);
  status.stickX = stick_x;
  status.stickY = stick_y;
  status.substickX = substick_x;
  status.substickY = substick_y;
  status.triggerLeft = trigger_left;
  status.triggerRight = trigger_right;
  status.analogA = analog_a;
  status.analogB = analog_b;
  status.isConnected = connected;
  return status;
}

PyObject* get_gc(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  GCPadStatus pad_status = state->gc_manip->Get(controller_id);
  return GCPadStatusToPyDict(pad_status);
}

PyObject* set_gc(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  GCPadStatus status = GCPadStatusFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->gc_manip->Set(status, controller_id, API::ClearOn::NextFrame);
  Py_RETURN_NONE;
}

void setup_controller_module(PyObject* module, ControllerModuleState* state)
{
  state->gc_manip = PyScriptingBackend::GetCurrent()->GetGCManip();
}

void cleanup_controller_module(PyObject* module, ControllerModuleState* state)
{
  state->gc_manip->Clear();
}

PyMODINIT_FUNC PyInit_controller()
{
  static PyMethodDef method_defs[] = {
      {"get_gc", get_gc, METH_VARARGS, ""},
      {"set_gc", set_gc, METH_VARARGS, ""},
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<ControllerModuleState, setup_controller_module,
                                cleanup_controller_module>("controller", method_defs);
  return PyModuleDef_Init(&module_def);
}

}  // namespace PyScripting
