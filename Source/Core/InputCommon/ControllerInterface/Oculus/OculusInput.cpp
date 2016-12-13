// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Oculus/OculusInput.h"
#include "VideoCommon/VR.h"

namespace ciface
{
namespace OculusInput
{
static const struct
{
  const char* const name;
  const u32 bitmask;
} named_buttons[] = {{"Select", OCULUS_BUTTON_ENTER}, {"Back", OCULUS_BUTTON_BACK},
                     {"Up", OCULUS_BUTTON_UP},        {"Down", OCULUS_BUTTON_DOWN},
                     {"Left", OCULUS_BUTTON_LEFT},    {"Right", OCULUS_BUTTON_RIGHT},
                     {"Minus", OCULUS_BUTTON_MINUS},  {"Plus", OCULUS_BUTTON_PLUS},
                     {"Oculus", OCULUS_BUTTON_HOME}},
  touch_buttons[] = {{"A", OCULUS_BUTTON_A},           {"B", OCULUS_BUTTON_B},
                     {"X", OCULUS_BUTTON_X},           {"Y", OCULUS_BUTTON_Y},
                     {"Enter", OCULUS_BUTTON_ENTER},
                     {"LThumb", OCULUS_BUTTON_LTHUMB}, {"RThumb", OCULUS_BUTTON_RTHUMB},
                     {"Oculus", OCULUS_BUTTON_HOME}},
  touch_touches[] = {{"TouchA", OCULUS_BUTTON_A},
                     {"TouchB", OCULUS_BUTTON_B},
                     {"TouchX", OCULUS_BUTTON_X},
                     {"TouchY", OCULUS_BUTTON_Y},
                     {"TouchLThumb", OCULUS_BUTTON_LTHUMB},
                     {"TouchRThumb", OCULUS_BUTTON_RTHUMB},
                     {"TouchLTrigger", OCULUS_BUTTON_LTRIGGER},
                     {"TouchRTrigger", OCULUS_BUTTON_RTRIGGER},
                     {"TouchLRest", OCULUS_BUTTON_LSHOULDER},
                     {"TouchRRest", OCULUS_BUTTON_RSHOULDER}},
  hmd_gestures[] = {{"Tap", OCULUS_BUTTON_A}};

static const char* const named_triggers[] = {"LTrigger", "RTrigger", "LGrip", "RGrip"};

static const char* const named_axes[] = {"LStickX", "LStickY", "RStickX", "RStickY"};

static const char* const named_motors[] = {
    "LRumble00", "LRumble05", "LRumble10", "RRumble00", "RRumble05", "RRumble10",
};

void Init()
{
}

void PopulateDevices()
{
  g_controller_interface.AddDevice(std::make_shared<SIDRemote>());
  g_controller_interface.AddDevice(std::make_shared<OculusTouch>());
  g_controller_interface.AddDevice(std::make_shared<HMDDevice>());
}

void DeInit()
{
}

SIDRemote::SIDRemote()
{
  // get supported buttons
  for (int i = 0; i != sizeof(named_buttons) / sizeof(*named_buttons); ++i)
  {
    AddInput(new Button(i, m_buttons));
  }

  ZeroMemory(&m_buttons, sizeof(m_buttons));
}

std::string SIDRemote::GetName() const
{
  return "Remote";
}

std::string SIDRemote::GetSource() const
{
  return "VR";
}

// Update I/O

void SIDRemote::UpdateInput()
{
  VR_GetRemoteButtons(&m_buttons);
}

// GET name/source/id

std::string SIDRemote::Button::GetName() const
{
  return named_buttons[m_index].name;
}

// GET / SET STATES

ControlState SIDRemote::Button::GetGatedState()
{
  return GetState();
}

ControlState SIDRemote::Button::GetState() const
{
  return (m_buttons & named_buttons[m_index].bitmask) > 0;
}

u32 SIDRemote::Button::GetStates() const
{
  return (u32)m_buttons;
}

OculusTouch::OculusTouch()
{
  for (int i = 0; i != sizeof(touch_buttons) / sizeof(*touch_buttons); ++i)
  {
    AddInput(new Button(i, m_buttons));
  }
  for (int i = 0; i < 4; ++i)
  {
    AddInput(new Trigger(i, m_triggers));
  }
  for (int i = 0; i < 4; ++i)
  {
    AddInput(new Axis(i, -1, m_axes));
    AddInput(new Axis(i, 1, m_axes));
  }
  for (int i = 0; i != sizeof(touch_touches) / sizeof(*touch_touches); ++i)
  {
    AddInput(new Touch(i, m_touches));
  }

  for (int i = 0; i != sizeof(named_motors) / sizeof(*named_motors); ++i)
  {
    AddOutput(new Motor(i, this, m_motors[i]));
  }

  ZeroMemory(&m_buttons, sizeof(m_buttons));
  ZeroMemory(&m_touches, sizeof(m_touches));
  ZeroMemory(m_triggers, sizeof(m_triggers));
  ZeroMemory(m_axes, sizeof(m_axes));
  ZeroMemory(m_motors, sizeof(m_motors));
}

std::string OculusTouch::GetName() const
{
  return "Touch";
}

std::string OculusTouch::GetSource() const
{
  return "VR";
}

// Update I/O

void OculusTouch::UpdateInput()
{
  VR_GetTouchButtons(&m_buttons, &m_touches, m_triggers, m_axes);
  UpdateMotors();
}

void OculusTouch::UpdateMotors()
{
  for (int hand = 0; hand < 2; ++hand)
  {
    float f, a;
    if (m_motors[hand * 3 + 0] > m_motors[hand * 3 + 1] &&
        m_motors[hand * 3 + 0] > m_motors[hand * 3 + 2])
      f = 0.0f;
    else if (m_motors[hand * 3 + 1] >= m_motors[hand * 3 + 0] &&
             m_motors[hand * 3 + 1] >= m_motors[hand * 3 + 2])
      f = 0.5f;
    else
      f = 1.0f;
    a = m_motors[hand * 3 + 0] + m_motors[hand * 3 + 1] + m_motors[hand * 3 + 2];
    VR_SetTouchVibration(hand + 1, f, a);
  }
}

// GET name/source/id

std::string OculusTouch::Button::GetName() const
{
  return touch_buttons[m_index].name;
}

// GET / SET STATES

ControlState OculusTouch::Button::GetState() const
{
  return (m_buttons & touch_buttons[m_index].bitmask) > 0;
}

u32 OculusTouch::Button::GetStates() const
{
  return (u32)m_buttons;
}

// GET name/source/id

std::string OculusTouch::Touch::GetName() const
{
  return touch_touches[m_index].name;
}

std::string OculusTouch::Axis::GetName() const
{
  return std::string(named_axes[m_index]) + (m_range < 0 ? '-' : '+');
}

std::string OculusTouch::Trigger::GetName() const
{
  return named_triggers[m_index];
}

std::string OculusTouch::Motor::GetName() const
{
  return named_motors[m_index];
}

// GET / SET STATES

ControlState OculusTouch::Button::GetGatedState()
{
  return GetState();
}

ControlState OculusTouch::Touch::GetGatedState()
{
  return GetState();
}

ControlState OculusTouch::Trigger::GetGatedState()
{
  return GetState();
}

ControlState OculusTouch::Axis::GetGatedState()
{
  return GetState();
}

ControlState OculusTouch::Touch::GetState() const
{
  return (m_touches & touch_touches[m_index].bitmask) > 0;
}

ControlState OculusTouch::Trigger::GetState() const
{
  return ControlState(m_triggers[m_index]);
}

ControlState OculusTouch::Axis::GetState() const
{
  return std::max(0.0, ControlState(m_axes[m_index] * m_range));
}

void OculusTouch::Motor::SetGatedState(ControlState state)
{
  SetState(state);
}

void OculusTouch::Motor::SetState(ControlState state)
{
  m_motor = (float)state;
  m_parent->UpdateMotors();
}

u32 OculusTouch::Touch::GetStates() const
{
  return (u32)m_touches;
}

HMDDevice::HMDDevice()
{
  for (int i = 0; i != sizeof(hmd_gestures) / sizeof(*hmd_gestures); ++i)
  {
    AddInput(new Gesture(i, m_gestures));
  }

  ZeroMemory(&m_gestures, sizeof(m_gestures));
}

std::string HMDDevice::GetName() const
{
  return "HMD";
}

std::string HMDDevice::GetSource() const
{
  return "VR";
}

// Update I/O

void HMDDevice::UpdateInput()
{
  VR_GetHMDGestures(&m_gestures);
}

// GET name/source/id

std::string HMDDevice::Gesture::GetName() const
{
  return hmd_gestures[m_index].name;
}

// GET / SET STATES

ControlState HMDDevice::Gesture::GetGatedState()
{
  return GetState();
}

ControlState HMDDevice::Gesture::GetState() const
{
  return (m_gestures & hmd_gestures[m_index].bitmask) > 0;
}

u32 HMDDevice::Gesture::GetStates() const
{
  return (u32)m_gestures;
}
}
}
