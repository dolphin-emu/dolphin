// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/XInput/XInput.h"
#include "VideoCommon/VR.h"

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

namespace ciface
{
namespace XInput
{
static const struct
{
  const char* const name;
  const WORD bitmask;
} named_buttons[] = {{"Button A", XINPUT_GAMEPAD_A},
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
                     {"Thumb R", XINPUT_GAMEPAD_RIGHT_THUMB}};

static const char* const named_triggers[] = {"Trigger L", "Trigger R"};

static const char* const named_axes[] = {"Left X", "Left Y", "Right X", "Right Y"};

static const char* const named_motors[] = {"Motor L", "Motor R"};

static HMODULE hXInput = nullptr;

typedef decltype(&XInputGetCapabilities) XInputGetCapabilities_t;
typedef decltype(&XInputSetState) XInputSetState_t;
typedef decltype(&XInputGetState) XInputGetState_t;

static XInputGetCapabilities_t PXInputGetCapabilities = nullptr;
static XInputSetState_t PXInputSetState = nullptr;
static XInputGetState_t PXInputGetState = nullptr;

static bool haveGuideButton = false;

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

    // Ordinal 100 is the same as XInputGetState, except it doesn't dummy out the guide
    // button info. Try loading it and fall back if needed.
    PXInputGetState = (XInputGetState_t)::GetProcAddress(hXInput, (LPCSTR)100);
    if (PXInputGetState)
      haveGuideButton = true;
    else
      PXInputGetState = (XInputGetState_t)::GetProcAddress(hXInput, "XInputGetState");

    if (!PXInputGetCapabilities || !PXInputSetState || !PXInputGetState)
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

  XINPUT_CAPABILITIES caps;
  for (int i = 0; i != 4; ++i)
    if (ERROR_SUCCESS == PXInputGetCapabilities(i, 0, &caps))
    {
      g_controller_interface.AddDevice(std::make_shared<Device>(caps, i));
    }
    else if (i == 0)
    {
      ZeroMemory(&caps, sizeof(caps));
      caps.SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
      FillMemory(&caps.Gamepad, sizeof(caps.Gamepad), 0xFF);
      FillMemory(&caps.Vibration, sizeof(caps.Vibration), 0xFF);
      g_controller_interface.AddDevice(std::make_shared<Device>(caps, i));
    }
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

  // get supported buttons
  for (int i = 0; i != sizeof(named_buttons) / sizeof(*named_buttons); ++i)
  {
    // Only add guide button if we have the 100 ordinal XInputGetState
    if (!(named_buttons[i].bitmask & XINPUT_GAMEPAD_GUIDE) || haveGuideButton)
      AddInput(new Button(i, m_state_in.Gamepad.wButtons));
  }

  // get supported triggers
  for (int i = 0; i != sizeof(named_triggers) / sizeof(*named_triggers); ++i)
  {
    AddInput(new Trigger(i, (&m_state_in.Gamepad.bLeftTrigger)[i], 255));
  }

  // get supported axes
  for (int i = 0; i != sizeof(named_axes) / sizeof(*named_axes); ++i)
  {
    const SHORT& ax = (&m_state_in.Gamepad.sThumbLX)[i];

    // each axis gets a negative and a positive input instance associated with it
    AddInput(new Axis(i, ax, -32768));
    AddInput(new Axis(i, ax, 32767));
  }

  // get supported motors
  for (int i = 0; i != sizeof(named_motors) / sizeof(*named_motors); ++i)
  {
    AddOutput(new Motor(i, this, (&m_state_out.wLeftMotorSpeed)[i], 65535));
  }

  ZeroMemory(&m_state_in, sizeof(m_state_in));
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

// Update I/O

void Device::UpdateInput()
{
  PXInputGetState(m_index, &m_state_in);
}

void Device::UpdateMotors()
{
  // this if statement is to make rumble work better when multiple ControllerInterfaces are using
  // the device
  // only calls XInputSetState if the state changed
  if (memcmp(&m_state_out, &m_current_state_out, sizeof(m_state_out)))
  {
    m_current_state_out = m_state_out;
    PXInputSetState(m_index, &m_state_out);
  }
}

// GET name/source/id

std::string Device::Button::GetName() const
{
  return named_buttons[m_index].name;
}

std::string Device::Axis::GetName() const
{
  return std::string(named_axes[m_index]) + (m_range < 0 ? '-' : '+');
}

std::string Device::Trigger::GetName() const
{
  return named_triggers[m_index];
}

std::string Device::Motor::GetName() const
{
  return named_motors[m_index];
}

// GET / SET STATES
ControlState Device::Button::GetGatedState()
{
  if (g_has_hmd || InputGateOn())
    return GetState();
  else
    return 0.0;
}

ControlState Device::Trigger::GetGatedState()
{
  if (g_has_hmd || InputGateOn())
    return GetState();
  else
    return 0.0;
}

ControlState Device::Axis::GetGatedState()
{
  if (g_has_hmd || InputGateOn())
    return GetState();
  else
    return 0.0;
}

ControlState Device::Button::GetState() const
{
  return (m_buttons & named_buttons[m_index].bitmask) > 0;
}

ControlState Device::Trigger::GetState() const
{
  return ControlState(m_trigger) / m_range;
}

ControlState Device::Axis::GetState() const
{
  return std::max(0.0, ControlState(m_axis) / m_range);
}

void Device::Motor::SetGatedState(ControlState state)
{
  if (g_has_hmd || InputGateOn())
    SetState(state);
}

void Device::Motor::SetState(ControlState state)
{
  m_motor = (WORD)(state * m_range);
  m_parent->UpdateMotors();
}
u32 Device::Button::GetStates() const
{
  return (u32)m_buttons;
}
}
}
