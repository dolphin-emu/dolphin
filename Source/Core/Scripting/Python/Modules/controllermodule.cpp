// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "controllermodule.h"

#include <algorithm>

#include "Core/API/Controller.h"
#include "Core/Movie.h"
#include "Core/System.h"
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
  API::WiiAccelManip* wii_accel_manip;
  API::WiiMotionPlusManip* wii_motion_plus_manip;
  API::NunchuckButtonsManip* nunchuck_buttons_manip;
  API::NunchuckAccelManip* nunchuck_accel_manip;
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
  bool connected = py_connected == nullptr || PyObject_IsTrue(py_connected);

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

static PyObject* NunchuckButtonDataToPyDict(WiimoteEmu::Nunchuk::DataFormat status)
{
  return Py_BuildValue("{s:O,s:O,s:B,s:B}", "C",
                       status.GetButtons() & WiimoteEmu::Nunchuk::BUTTON_C ? Py_True : Py_False,
                       "Z",
                       status.GetButtons() & WiimoteEmu::Nunchuk::BUTTON_Z ? Py_True : Py_False,
                       "StickX", status.GetStick().value.x, "StickY", status.GetStick().value.y);
}

static WiimoteEmu::Nunchuk::DataFormat NunchuckButtonDataFromPyDict(PyObject* dict)
{
  WiimoteEmu::Nunchuk::DataFormat status{};
  PyObject* py_c = PyDict_GetItemString(dict, "C");
  PyObject* py_z = PyDict_GetItemString(dict, "Z");
  PyObject* py_stickx = PyDict_GetItemString(dict, "StickX");
  PyObject* py_sticky = PyDict_GetItemString(dict, "StickY");
  u8 buttons = 0;
  if (py_c != nullptr && PyObject_IsTrue(py_c))
    buttons |= WiimoteEmu::Nunchuk::BUTTON_C;
  if (py_z != nullptr && PyObject_IsTrue(py_z))
    buttons |= WiimoteEmu::Nunchuk::BUTTON_Z;
  status.SetButtons(buttons);
  status.jx = py_stickx == nullptr ? 128 : PyLong_AsUnsignedLong(py_stickx);
  status.jy = py_sticky == nullptr ? 128 : PyLong_AsUnsignedLong(py_sticky);

  return status;
}

static PyObject* AccelDataToPyDict(WiimoteCommon::AccelData status)
{
  return Py_BuildValue("{s:H,s:H,s:H}", "X", status.value.x, "Y", status.value.y, "Z",
                       status.value.z);
}

static WiimoteCommon::AccelData WiiAccelDataFromPyDict(PyObject* dict)
{
  constexpr u16 accel_min = 0;
  constexpr u16 accel_max = (1 << 10) - 1;
  const auto default_accel = WiimoteEmu::DesiredWiimoteState::DEFAULT_ACCELERATION;

  const auto get_component = [dict, accel_min, accel_max](const char* key, u16 fallback) {
    PyObject* value = PyDict_GetItemString(dict, key);
    const unsigned long raw = value == nullptr ? fallback : PyLong_AsUnsignedLong(value);
    return static_cast<u16>(std::clamp<unsigned long>(raw, accel_min, accel_max));
  };

  return WiimoteCommon::AccelData({get_component("X", default_accel.value.x),
                                   get_component("Y", default_accel.value.y),
                                   get_component("Z", default_accel.value.z)});
}

static WiimoteCommon::AccelData NunchuckAccelDataFromPyDict(PyObject* dict)
{
  constexpr u16 accel_min = 0;
  constexpr u16 accel_max = (1 << 10) - 1;
  const WiimoteCommon::AccelData default_accel(
      {WiimoteEmu::Nunchuk::ACCEL_ZERO_G << 2, WiimoteEmu::Nunchuk::ACCEL_ZERO_G << 2,
       WiimoteEmu::Nunchuk::ACCEL_ONE_G << 2});

  const auto get_component = [dict, accel_min, accel_max](const char* key, u16 fallback) {
    PyObject* value = PyDict_GetItemString(dict, key);
    const unsigned long raw = value == nullptr ? fallback : PyLong_AsUnsignedLong(value);
    return static_cast<u16>(std::clamp<unsigned long>(raw, accel_min, accel_max));
  };

  return WiimoteCommon::AccelData({get_component("X", default_accel.value.x),
                                   get_component("Y", default_accel.value.y),
                                   get_component("Z", default_accel.value.z)});
}

static PyObject* MotionPlusDataToPyDict(const WiimoteEmu::MotionPlus::DataFormat::Data& status)
{
  return Py_BuildValue("{s:H,s:H,s:H,s:O,s:O,s:O}", "X", status.gyro.value.x, "Y",
                       status.gyro.value.y, "Z", status.gyro.value.z, "SlowX",
                       status.is_slow.x ? Py_True : Py_False, "SlowY",
                       status.is_slow.y ? Py_True : Py_False, "SlowZ",
                       status.is_slow.z ? Py_True : Py_False);
}

static WiimoteEmu::MotionPlus::DataFormat::Data MotionPlusDataFromPyDict(PyObject* dict)
{
  constexpr u16 gyro_min = 0;
  constexpr u16 gyro_max = WiimoteEmu::MotionPlus::MAX_VALUE;

  const auto get_component = [dict, gyro_min, gyro_max](const char* key) {
    PyObject* value = PyDict_GetItemString(dict, key);
    const unsigned long raw = value == nullptr ? WiimoteEmu::MotionPlus::ZERO_VALUE :
                                                 PyLong_AsUnsignedLong(value);
    return static_cast<u16>(std::clamp<unsigned long>(raw, gyro_min, gyro_max));
  };
  const auto get_slow = [dict](const char* key) {
    PyObject* value = PyDict_GetItemString(dict, key);
    return value != nullptr && PyObject_IsTrue(value);
  };

  WiimoteEmu::MotionPlus::DataFormat::Data status{};
  status.gyro = WiimoteEmu::MotionPlus::DataFormat::GyroRawValue(
      WiimoteEmu::MotionPlus::DataFormat::GyroType(get_component("X"), get_component("Y"),
                                                   get_component("Z")));
  status.is_slow = WiimoteEmu::MotionPlus::DataFormat::SlowType(
      get_slow("SlowX"), get_slow("SlowY"), get_slow("SlowZ"));
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

// Input as the game sees it: the played-back DTM frame during movie playback, else the live pad.
static PyObject* get_gc_buttons_display(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  auto& movie = Core::System::GetInstance().GetMovie();
  if (movie.IsPlayingInput())
  {
    std::optional<GCPadStatus> status = movie.GetDisplayedPadStatus(controller_id);
    if (status.has_value())
      return GCPadStatusToPyDict(*status);
  }
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  return GCPadStatusToPyDict(state->gc_manip->Get(controller_id));
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

static PyObject* set_gc_input_override(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  GCPadStatus status = GCPadStatusFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->gc_manip->Set(status, controller_id, API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}

static PyObject* clear_gc_input_override(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->gc_manip->Clear(controller_id);
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
  state->wii_buttons_manip->Set(status, controller_id, API::ClearOn::NextOverride);
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

  state->wii_ir_manip->Set({{x, y, z}, {pitch, yaw, roll}}, controller_id,
                           API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}

static PyObject* get_wii_accelerometer(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  return AccelDataToPyDict(state->wii_accel_manip->Get(controller_id));
}

static PyObject* set_wii_accelerometer(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  const WiimoteCommon::AccelData status = WiiAccelDataFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->wii_accel_manip->Set(status, controller_id, API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}

static PyObject* get_wii_gyroscope(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  const auto motion_plus = state->wii_motion_plus_manip->Get(controller_id);
  if (!motion_plus.has_value())
    Py_RETURN_NONE;
  return MotionPlusDataToPyDict(*motion_plus);
}

static PyObject* set_wii_gyroscope(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  const auto status = MotionPlusDataFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->wii_motion_plus_manip->Set(status, controller_id, API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}

static PyObject* get_nunchuck_buttons(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  WiimoteEmu::Nunchuk::DataFormat status = state->nunchuck_buttons_manip->Get(controller_id);
  return NunchuckButtonDataToPyDict(status);
}

static PyObject* get_nunchuck_buttons_raw(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  WiimoteEmu::Nunchuk::DataFormat status = state->nunchuck_buttons_manip->GetRaw(controller_id);
  return NunchuckButtonDataToPyDict(status);
}

static PyObject* set_nunchuck_buttons(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  WiimoteEmu::Nunchuk::DataFormat status = NunchuckButtonDataFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->nunchuck_buttons_manip->Set(status, controller_id, API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}

static PyObject* get_nunchuck_accelerometer(PyObject* module, PyObject* args)
{
  auto controller_id_opt = Py::ParseTuple<int>(args);
  if (!controller_id_opt.has_value())
    return nullptr;
  int controller_id = std::get<0>(controller_id_opt.value());
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  return AccelDataToPyDict(state->nunchuck_accel_manip->Get(controller_id));
}

static PyObject* set_nunchuck_accelerometer(PyObject* module, PyObject* args)
{
  int controller_id;
  PyObject* dict;
  if (!PyArg_ParseTuple(args, "iO!", &controller_id, &PyDict_Type, &dict))
    return nullptr;
  const auto status = NunchuckAccelDataFromPyDict(dict);
  ControllerModuleState* state = Py::GetState<ControllerModuleState>(module);
  state->nunchuck_accel_manip->Set(status, controller_id, API::ClearOn::NextOverride);
  Py_RETURN_NONE;
}
static void ClearControllerState(ControllerModuleState* state)
{
  state->gc_manip->Clear();
  state->wii_buttons_manip->Clear();
  state->wii_ir_manip->Clear();
  state->wii_accel_manip->Clear();
  state->wii_motion_plus_manip->Clear();
  state->nunchuck_buttons_manip->Clear();
  state->nunchuck_accel_manip->Clear();
}

static PyObject* Reset(PyObject* module)
{
  ClearControllerState(Py::GetState<ControllerModuleState>(module));
  Py_RETURN_NONE;
}

static void setup_controller_module(PyObject* module, ControllerModuleState* state)
{
  state->gc_manip = PyScriptingBackend::GetCurrent()->GetGCManip();
  state->wii_buttons_manip = PyScriptingBackend::GetCurrent()->GetWiiButtonsManip();
  state->wii_ir_manip = PyScriptingBackend::GetCurrent()->GetWiiIRManip();
  state->wii_accel_manip = PyScriptingBackend::GetCurrent()->GetWiiAccelManip();
  state->wii_motion_plus_manip = PyScriptingBackend::GetCurrent()->GetWiiMotionPlusManip();
  state->nunchuck_buttons_manip = PyScriptingBackend::GetCurrent()->GetNunchuckButtonsManip();
  state->nunchuck_accel_manip = PyScriptingBackend::GetCurrent()->GetNunchuckAccelManip();
  PyScriptingBackend::GetCurrent()->AddCleanupFunc([state] { ClearControllerState(state); });
}

PyMODINIT_FUNC PyInit_controller()
{
  static PyMethodDef method_defs[] = {
      {"get_gc_buttons", get_gc_buttons, METH_VARARGS, ""},
      {"get_gc_buttons_display", get_gc_buttons_display, METH_VARARGS, ""},
      {"set_gc_buttons", set_gc_buttons, METH_VARARGS, ""},
      {"set_gc_input_override", set_gc_input_override, METH_VARARGS, ""},
      {"clear_gc_input_override", clear_gc_input_override, METH_VARARGS, ""},
      {"get_wii_buttons", get_wii_buttons, METH_VARARGS, ""},
      {"set_wii_buttons", set_wii_buttons, METH_VARARGS, ""},
      {"set_wii_ircamera_transform", set_wii_ircamera_transform, METH_VARARGS, ""},
      {"get_wii_accelerometer", get_wii_accelerometer, METH_VARARGS, ""},
      {"set_wii_accelerometer", set_wii_accelerometer, METH_VARARGS, ""},
      {"get_wii_gyroscope", get_wii_gyroscope, METH_VARARGS, ""},
      {"set_wii_gyroscope", set_wii_gyroscope, METH_VARARGS, ""},
      {"get_nunchuck_buttons", get_nunchuck_buttons, METH_VARARGS, ""},
      {"get_nunchuck_buttons_raw", get_nunchuck_buttons_raw, METH_VARARGS, ""},
      {"set_nunchuck_buttons", set_nunchuck_buttons, METH_VARARGS, ""},
      {"get_nunchuck_accelerometer", get_nunchuck_accelerometer, METH_VARARGS, ""},
      {"set_nunchuck_accelerometer", set_nunchuck_accelerometer, METH_VARARGS, ""},
      Py::MakeMethodDef<Reset>("_dolphin_reset"),
      
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<ControllerModuleState, setup_controller_module>("controller", method_defs);
  return PyModuleDef_Init(&module_def);
}

}  // namespace PyScripting
