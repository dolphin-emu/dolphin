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
  API::WiiButtonsManip* wii_buttons_manip;
  API::WiiIRManip* wii_ir_manip;
};

static PyObject* GCPadStatusToPyDict(GCPadStatus status) {
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

static GCPadStatus GCPadStatusFromPyDict(PyObject* dict) {
  PyObject* py_button_left = PyDict_GetItemString(dict, "Left");
  PyObject* py_button_right = PyDict_GetItemString(dict, "Right");
  PyObject* py_button_down = PyDict_GetItemString(dict, "Down");
  PyObject* py_button_up = PyDict_GetItemString(dict, "Up");
  PyObject* py_trigger_z = PyDict_GetItemString(dict, "Z");
  PyObject* py_trigger_r = PyDict_GetItemString(dict, "R");
  PyObject* py_trigger_l = PyDict_GetItemString(dict, "L");
  PyObject* py_button_a = PyDict_GetItemString(dict, "A");
  PyObject* py_button_b = PyDict_GetItemString(dict, "B");
  PyObject* py_button_x = PyDict_GetItemString(dict, "X");
  PyObject* py_button_y = PyDict_GetItemString(dict, "Y");
  PyObject* py_button_start = PyDict_GetItemString(dict, "Start");
  bool button_left = py_button_left != nullptr && PyObject_IsTrue(py_button_left);
  bool button_right = py_button_right != nullptr && PyObject_IsTrue(py_button_right);
  bool button_down = py_button_down != nullptr && PyObject_IsTrue(py_button_down);
  bool button_up = py_button_up != nullptr && PyObject_IsTrue(py_button_up);
  bool trigger_z = py_trigger_z != nullptr && PyObject_IsTrue(py_trigger_z);
  bool trigger_r = py_trigger_r != nullptr && PyObject_IsTrue(py_trigger_r);
  bool trigger_l = py_trigger_l != nullptr && PyObject_IsTrue(py_trigger_l);
  bool button_a = py_button_a != nullptr && PyObject_IsTrue(py_button_a);
  bool button_b = py_button_b != nullptr && PyObject_IsTrue(py_button_b);
  bool button_x = py_button_x != nullptr && PyObject_IsTrue(py_button_x);
  bool button_y = py_button_y != nullptr && PyObject_IsTrue(py_button_y);
  bool button_start = py_button_start != nullptr && PyObject_IsTrue(py_button_start);

  PyObject* py_stick_x = PyDict_GetItemString(dict, "StickX");
  PyObject* py_stick_y = PyDict_GetItemString(dict, "StickY");
  PyObject* py_substick_x = PyDict_GetItemString(dict, "CStickX");
  PyObject* py_substick_y = PyDict_GetItemString(dict, "CStickY");
  PyObject* py_trigger_left = PyDict_GetItemString(dict, "TriggerLeft");
  PyObject* py_trigger_right = PyDict_GetItemString(dict, "TriggerRight");
  PyObject* py_analog_a = PyDict_GetItemString(dict, "AnalogA");
  PyObject* py_analog_b = PyDict_GetItemString(dict, "AnalogB");
  u8 stick_x = py_stick_x == nullptr ? 128 : PyLong_AsUnsignedLong(py_stick_x);
  u8 stick_y = py_stick_y == nullptr ? 128 : PyLong_AsUnsignedLong(py_stick_y);
  u8 substick_x = py_substick_x == nullptr ? 128 : PyLong_AsUnsignedLong(py_substick_x);
  u8 substick_y = py_substick_y == nullptr ? 128 : PyLong_AsUnsignedLong(py_substick_y);
  u8 trigger_left = py_trigger_left == nullptr ? 0 : PyLong_AsUnsignedLong(py_trigger_left);
  u8 trigger_right = py_trigger_right == nullptr ? 0 : PyLong_AsUnsignedLong(py_trigger_right);
  u8 analog_a = py_analog_a == nullptr ? 0 : PyLong_AsUnsignedLong(py_analog_a);
  u8 analog_b = py_analog_b == nullptr ? 0 : PyLong_AsUnsignedLong(py_analog_b);

  PyObject* py_connected = PyDict_GetItemString(dict, "Connected");
  bool connected = py_connected != nullptr && PyObject_IsTrue(py_connected);

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

static PyObject* WiiButtonDataToPyDict(WiimoteCommon::ButtonData status) {
  return Py_BuildValue("{s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O,s:O}",
      "Left", status.left ? Py_True : Py_False,
      "Right", status.right ? Py_True : Py_False,
      "Down", status.down ? Py_True : Py_False,
      "Up", status.up ? Py_True : Py_False,
      "Plus", status.plus ? Py_True : Py_False,
      "Two", status.two ? Py_True : Py_False,
      "One", status.one ? Py_True : Py_False,
      "B", status.b ? Py_True : Py_False,
      "A", status.a ? Py_True : Py_False,
      "Minus", status.minus ? Py_True : Py_False,
      "Home", status.home ? Py_True : Py_False
  );
}

static WiimoteCommon::ButtonData WiiButtonDataFromPyDict(PyObject* dict) {
  WiimoteCommon::ButtonData status;
  status.hex = 0;
  PyObject* py_left = PyDict_GetItemString(dict, "Left");
  PyObject* py_right = PyDict_GetItemString(dict, "Right");
  PyObject* py_down = PyDict_GetItemString(dict, "Down");
  PyObject* py_up = PyDict_GetItemString(dict, "Up");
  PyObject* py_plus = PyDict_GetItemString(dict, "Plus");
  PyObject* py_two = PyDict_GetItemString(dict, "Two");
  PyObject* py_one = PyDict_GetItemString(dict, "One");
  PyObject* py_b = PyDict_GetItemString(dict, "B");
  PyObject* py_a = PyDict_GetItemString(dict, "A");
  PyObject* py_minus = PyDict_GetItemString(dict, "Minus");
  PyObject* py_home = PyDict_GetItemString(dict, "Home");
  status.left = py_left != nullptr && PyObject_IsTrue(py_left);
  status.right = py_right != nullptr && PyObject_IsTrue(py_right);
  status.down = py_down != nullptr && PyObject_IsTrue(py_down);
  status.up = py_up != nullptr && PyObject_IsTrue(py_up);
  status.plus = py_plus != nullptr && PyObject_IsTrue(py_plus);
  status.two = py_two != nullptr && PyObject_IsTrue(py_two);
  status.one = py_one != nullptr && PyObject_IsTrue(py_one);
  status.b = py_b != nullptr && PyObject_IsTrue(py_b);
  status.a = py_a != nullptr && PyObject_IsTrue(py_a);
  status.minus = py_minus != nullptr && PyObject_IsTrue(py_minus);
  status.home = py_home != nullptr && PyObject_IsTrue(py_home);
  return status;
}

static PyObject* get_gc_buttons(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  GCPadStatus pad_status = state->gc_manip->Get(controller_id);
  return GCPadStatusToPyDict(pad_status);
}

static PyObject* set_gc_buttons(PyObject* module, PyObject* args)
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

static PyObject* get_wii_buttons(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  WiimoteCommon::ButtonData status = state->wii_buttons_manip->Get(controller_id);
  return WiiButtonDataToPyDict(status);
}

static PyObject* set_wii_buttons(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  WiimoteCommon::ButtonData status = WiiButtonDataFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->wii_buttons_manip->Set(status, controller_id, API::ClearOn::NextFrame);
  Py_RETURN_NONE;
}

static PyObject* set_wii_ircamera_transform(PyObject* module, PyObject* args)
{
  int controller_id;
  float x, y;
  float z = -2; // 2 meters away from sensor bar by default
  float pitch, yaw, roll;
  if (!PyArg_ParseTuple(args, "ifff|fff", &controller_id, &x, &y, &z, &pitch, &yaw, &roll))
    return nullptr;
  const ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);

  state->wii_ir_manip->Set({{x, y, z}, {pitch, yaw, roll}}, controller_id, API::ClearOn::NextFrame);
  Py_RETURN_NONE;
}

static void setup_controller_module(PyObject* module, ControllerModuleState* state)
{
  state->gc_manip = PyScriptingBackend::GetCurrent()->GetGCManip();
  state->wii_buttons_manip = PyScriptingBackend::GetCurrent()->GetWiiButtonsManip();
  state->wii_ir_manip = PyScriptingBackend::GetCurrent()->GetWiiIRManip();
  PyScriptingBackend::GetCurrent()->AddCleanupFunc([state] {
    state->gc_manip->Clear();
    state->wii_buttons_manip->Clear();
    state->wii_ir_manip->Clear();
  });
}

PyMODINIT_FUNC PyInit_controller()
{
  static PyMethodDef method_defs[] = {
      {"get_gc_buttons", get_gc_buttons, METH_VARARGS, ""},
      {"set_gc_buttons", set_gc_buttons, METH_VARARGS, ""},
      {"get_wii_buttons", get_wii_buttons, METH_VARARGS, ""},
      {"set_wii_buttons", set_wii_buttons, METH_VARARGS, ""},
      {"set_wii_ircamera_transform", set_wii_ircamera_transform, METH_VARARGS, ""},
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<ControllerModuleState, setup_controller_module>("controller", method_defs);
  return PyModuleDef_Init(&module_def);
}

}  // namespace PyScripting
