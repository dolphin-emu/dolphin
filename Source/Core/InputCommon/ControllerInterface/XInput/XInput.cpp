// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/XInput/XInput.h"

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

namespace ciface::XInput
{
struct ButtonDef
{
  const char* name;
  WORD bitmask;
};
static constexpr std::array<ButtonDef, 15> named_buttons{{
    {"Button A", XINPUT_GAMEPAD_A},
    {"Button B", XINPUT_GAMEPAD_B},
    {"Button X", XINPUT_GAMEPAD_X},
    {"Button Y", XINPUT_GAMEPAD_Y},
    {"Pad N", XINPUT_GAMEPAD_DPAD_UP},
    {"Pad S", XINPUT_GAMEPAD_DPAD_DOWN},
    {"Pad W", XINPUT_GAMEPAD_DPAD_LEFT},
    {"Pad E", XINPUT_GAMEPAD_DPAD_RIGHT},
    {"Start", XINPUT_GAMEPAD_START},
    {"Back", XINPUT_GAMEPAD_BACK},
    {"Shoulder L", XINPUT_GAMEPAD_LEFT_SHOULDER},
    {"Shoulder R", XINPUT_GAMEPAD_RIGHT_SHOULDER},
    {"Guide", XINPUT_GAMEPAD_GUIDE},
    {"Thumb L", XINPUT_GAMEPAD_LEFT_THUMB},
    {"Thumb R", XINPUT_GAMEPAD_RIGHT_THUMB},
}};

static constexpr std::array named_triggers{"Trigger L", "Trigger R"};

static constexpr std::array named_axes{"Left X", "Left Y", "Right X", "Right Y"};

static constexpr std::array named_motors{"Motor L", "Motor R"};

class Button final : public Core::Device::Input
{
public:
  Button(u8 index, const WORD& buttons) : m_buttons(buttons), m_index(index) {}
  std::string GetName() const override { return named_buttons[m_index].name; }
  ControlState GetState() const override
  {
    return (m_buttons & named_buttons[m_index].bitmask) > 0;
  }

private:
  const WORD& m_buttons;
  const u8 m_index;
};

class Axis final : public Core::Device::Input
{
public:
  Axis(u8 index, const SHORT& axis, SHORT range) : m_axis(axis), m_range(range), m_index(index) {}
  std::string GetName() const override
  {
    return std::string(named_axes[m_index]) + (m_range < 0 ? '-' : '+');
  }
  ControlState GetState() const override { return ControlState(m_axis) / m_range; }

private:
  const SHORT& m_axis;
  const SHORT m_range;
  const u8 m_index;
};

class Trigger final : public Core::Device::Input
{
public:
  Trigger(u8 index, const BYTE& trigger, BYTE range)
      : m_trigger(trigger), m_range(range), m_index(index)
  {
  }
  std::string GetName() const override { return named_triggers[m_index]; }
  ControlState GetState() const override { return ControlState(m_trigger) / m_range; }

private:
  const BYTE& m_trigger;
  const BYTE m_range;
  const u8 m_index;
};

class Motor final : public Core::Device::Output
{
public:
  Motor(u8 index, Device* parent, WORD& motor, WORD range)
      : m_motor(motor), m_range(range), m_index(index), m_parent(parent)
  {
  }

  std::string GetName() const override { return named_motors[m_index]; }
  void SetState(ControlState state) override
  {
    const auto old_value = m_motor;
    m_motor = (WORD)(state * m_range);

    // Only update if the state changed.
    if (m_motor != old_value)
      m_parent->UpdateMotors();
  }

private:
  WORD& m_motor;
  const WORD m_range;
  const u8 m_index;
  Device* m_parent;
};

class Battery final : public Core::Device::Input
{
public:
  Battery(const ControlState* level) : m_level(*level) {}
  std::string GetName() const override { return "Battery"; }
  ControlState GetState() const override { return m_level; }
  bool IsDetectable() const override { return false; }

private:
  const ControlState& m_level;
};

static HMODULE hXInput = nullptr;

typedef decltype(&XInputGetCapabilities) XInputGetCapabilities_t;
typedef decltype(&XInputSetState) XInputSetState_t;
typedef decltype(&XInputGetState) XInputGetState_t;
typedef decltype(&XInputGetBatteryInformation) XInputGetBatteryInformation_t;

static XInputGetCapabilities_t PXInputGetCapabilities = nullptr;
static XInputSetState_t PXInputSetState = nullptr;
static XInputGetState_t PXInputGetState = nullptr;
static XInputGetBatteryInformation_t PXInputGetBatteryInformation = nullptr;

static bool s_have_guide_button = false;

void Init()
{
  if (!hXInput)
  {
    // Try for the most recent version we were compiled against (will only work if running on Win8+)
    hXInput = ::LoadLibrary(XINPUT_DLL);
    if (!hXInput)
    {
      // Drop back to DXSDK June 2010 version. Requires DX June 2010 redist.
      hXInput = ::LoadLibrary(TEXT("xinput1_3.dll"));
      if (!hXInput)
      {
        return;
      }
    }

    PXInputGetCapabilities =
        (XInputGetCapabilities_t)::GetProcAddress(hXInput, "XInputGetCapabilities");
    PXInputSetState = (XInputSetState_t)::GetProcAddress(hXInput, "XInputSetState");
    PXInputGetBatteryInformation =
        (XInputGetBatteryInformation_t)::GetProcAddress(hXInput, "XInputGetBatteryInformation");

    // Ordinal 100 is the same as XInputGetState, except it doesn't dummy out the guide
    // button info. Try loading it and fall back if needed.
    PXInputGetState = (XInputGetState_t)::GetProcAddress(hXInput, (LPCSTR)100);

    s_have_guide_button = PXInputGetState != nullptr;

    if (!PXInputGetState)
      PXInputGetState = (XInputGetState_t)::GetProcAddress(hXInput, "XInputGetState");

    if (!PXInputGetCapabilities || !PXInputSetState || !PXInputGetState ||
        !PXInputGetBatteryInformation)
    {
      ::FreeLibrary(hXInput);
      hXInput = nullptr;
      return;
    }
  }
}

void PopulateDevices()
{
  if (!hXInput)
    return;

  g_controller_interface.RemoveDevice([](const auto* dev) { return dev->GetSource() == "XInput"; });

  XINPUT_CAPABILITIES caps;
  for (int i = 0; i != 4; ++i)
    if (ERROR_SUCCESS == PXInputGetCapabilities(i, 0, &caps))
      g_controller_interface.AddDevice(std::make_shared<Device>(caps, i));
}

void DeInit()
{
  if (hXInput)
  {
    ::FreeLibrary(hXInput);
    hXInput = nullptr;
  }
}

Device::Device(const XINPUT_CAPABILITIES& caps, u8 index) : m_subtype(caps.SubType), m_index(index)
{
  // XInputGetCaps can be broken on some devices, so we'll just ignore it
  // and assume all gamepad + vibration capabilities are supported

  // Buttons.
  for (size_t i = 0; i != size(named_buttons); ++i)
  {
    // Only add guide button if we have the 100 ordinal XInputGetState.
    if (named_buttons[i].bitmask == XINPUT_GAMEPAD_GUIDE && !s_have_guide_button)
      continue;

    AddInput(new Button(u8(i), m_state_in.Gamepad.wButtons));
  }

  // Triggers.
  for (size_t i = 0; i != size(named_triggers); ++i)
    AddInput(new Trigger(u8(i), (&m_state_in.Gamepad.bLeftTrigger)[i], 255));

  // Axes.
  for (size_t i = 0; i != size(named_axes); ++i)
  {
    const SHORT& ax = (&m_state_in.Gamepad.sThumbLX)[i];

    // Each axis gets a negative and a positive input instance associated with it.
    AddInput(new Axis(u8(i), ax, -32768));
    AddInput(new Axis(u8(i), ax, 32767));
  }

  // Rumble motors.
  for (size_t i = 0; i != size(named_motors); ++i)
    AddOutput(new Motor(u8(i), this, (&m_state_out.wLeftMotorSpeed)[i], 65535));

  AddInput(new Battery(&m_battery_level));
}

std::string Device::GetName() const
{
  switch (m_subtype)
  {
  case XINPUT_DEVSUBTYPE_GAMEPAD:
    return "Gamepad";
  case XINPUT_DEVSUBTYPE_WHEEL:
    return "Wheel";
  case XINPUT_DEVSUBTYPE_ARCADE_STICK:
    return "Arcade Stick";
  case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
    return "Flight Stick";
  case XINPUT_DEVSUBTYPE_DANCE_PAD:
    return "Dance Pad";
  case XINPUT_DEVSUBTYPE_GUITAR:
    return "Guitar";
  case XINPUT_DEVSUBTYPE_DRUM_KIT:
    return "Drum Kit";
  default:
    return "Device";
  }
}

std::string Device::GetSource() const
{
  return "XInput";
}

Core::DeviceRemoval Device::UpdateInput()
{
  PXInputGetState(m_index, &m_state_in);

  XINPUT_BATTERY_INFORMATION battery_info = {};
  if (SUCCEEDED(PXInputGetBatteryInformation(m_index, BATTERY_DEVTYPE_GAMEPAD, &battery_info)))
  {
    switch (battery_info.BatteryType)
    {
    case BATTERY_TYPE_DISCONNECTED:
    case BATTERY_TYPE_UNKNOWN:
      m_battery_level = 0;
      break;
    case BATTERY_TYPE_WIRED:
      m_battery_level = BATTERY_INPUT_MAX_VALUE;
      break;
    default:
      m_battery_level =
          battery_info.BatteryLevel / ControlState(BATTERY_LEVEL_FULL) * BATTERY_INPUT_MAX_VALUE;
      break;
    }
  }

  return Core::DeviceRemoval::Keep;
}

void Device::UpdateMotors()
{
  PXInputSetState(m_index, &m_state_out);
}

std::optional<int> Device::GetPreferredId() const
{
  return m_index;
}

}  // namespace ciface::XInput
