// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCPadEmu.h"

#include <array>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/StickGate.h"

#include "InputCommon/GCPadStatus.h"

static const u16 button_bitmasks[] = {
    PAD_BUTTON_A,
    PAD_BUTTON_B,
    PAD_BUTTON_X,
    PAD_BUTTON_Y,
    PAD_TRIGGER_Z,
    PAD_BUTTON_START,
    0  // MIC HAX
};

static const u16 trigger_bitmasks[] = {
    PAD_TRIGGER_L,
    PAD_TRIGGER_R,
};

static const u16 dpad_bitmasks[] = {PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT,
                                    PAD_BUTTON_RIGHT};

GCPad::GCPad(const unsigned int index) : m_index(index)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(BUTTONS_GROUP));
  for (const char* named_button : {A_BUTTON, B_BUTTON, X_BUTTON, Y_BUTTON, Z_BUTTON})
  {
    m_buttons->AddInput(ControllerEmu::DoNotTranslate, named_button);
  }
  // i18n: The START/PAUSE button on GameCube controllers
  m_buttons->AddInput(ControllerEmu::Translate, START_BUTTON, _trans("START"));

  // sticks
  groups.emplace_back(m_main_stick = new ControllerEmu::OctagonAnalogStick(
                          MAIN_STICK_GROUP, _trans("Control Stick"), MAIN_STICK_GATE_RADIUS));
  groups.emplace_back(m_c_stick = new ControllerEmu::OctagonAnalogStick(
                          C_STICK_GROUP, _trans("C Stick"), C_STICK_GATE_RADIUS));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(TRIGGERS_GROUP));
  for (const char* named_trigger : {L_DIGITAL, R_DIGITAL, L_ANALOG, R_ANALOG})
  {
    m_triggers->AddInput(ControllerEmu::Translate, named_trigger);
  }

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(DPAD_GROUP));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }

  // Microphone
  groups.emplace_back(m_mic = new ControllerEmu::Buttons(MIC_GROUP));
  m_mic->AddInput(ControllerEmu::Translate, _trans("Button"));

  // rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(RUMBLE_GROUP));
  m_rumble->AddOutput(ControllerEmu::Translate, _trans("Motor"));

  // options
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(OPTIONS_GROUP));
  m_options->AddSetting(
      &m_always_connected_setting,
      // i18n: Treat a controller as always being connected regardless of what
      // devices the user actually has plugged in
      {_trans("Always Connected"), nullptr,
       _trans("If checked, the emulated controller is always connected.\n"
              "If unchecked, the connection state of the emulated controller is linked\n"
              "to the connection state of the real default device (if there is one).")},
      false);
}

std::string GCPad::GetName() const
{
  return std::string("GCPad") + char('1' + m_index);
}

ControllerEmu::ControlGroup* GCPad::GetGroup(PadGroup group)
{
  switch (group)
  {
  case PadGroup::Buttons:
    return m_buttons;
  case PadGroup::MainStick:
    return m_main_stick;
  case PadGroup::CStick:
    return m_c_stick;
  case PadGroup::DPad:
    return m_dpad;
  case PadGroup::Triggers:
    return m_triggers;
  case PadGroup::Rumble:
    return m_rumble;
  case PadGroup::Mic:
    return m_mic;
  case PadGroup::Options:
    return m_options;
  default:
    return nullptr;
  }
}

GCPadStatus GCPad::GetInput() const
{
  const auto lock = GetStateLock();
  GCPadStatus pad = {};

  if (!(m_always_connected_setting.GetValue() || IsDefaultDeviceConnected() ||
        m_input_override_function))
  {
    pad.isConnected = false;
    return pad;
  }

  // buttons
  m_buttons->GetState(&pad.button, button_bitmasks, m_input_override_function);

  // set analog A/B analog to full or w/e, prolly not needed
  if (pad.button & PAD_BUTTON_A)
    pad.analogA = 0xFF;
  if (pad.button & PAD_BUTTON_B)
    pad.analogB = 0xFF;

  // dpad
  m_dpad->GetState(&pad.button, dpad_bitmasks, m_input_override_function);

  // sticks
  const auto main_stick_state = m_main_stick->GetState(m_input_override_function);
  pad.stickX = MapFloat<u8>(main_stick_state.x, GCPadStatus::MAIN_STICK_CENTER_X, 1);
  pad.stickY = MapFloat<u8>(main_stick_state.y, GCPadStatus::MAIN_STICK_CENTER_Y, 1);

  const auto c_stick_state = m_c_stick->GetState(m_input_override_function);
  pad.substickX = MapFloat<u8>(c_stick_state.x, GCPadStatus::C_STICK_CENTER_X, 1);
  pad.substickY = MapFloat<u8>(c_stick_state.y, GCPadStatus::C_STICK_CENTER_Y, 1);

  // triggers
  std::array<ControlState, 2> triggers;
  m_triggers->GetState(&pad.button, trigger_bitmasks, triggers.data(), m_input_override_function);
  pad.triggerLeft = MapFloat<u8>(triggers[0], 0);
  pad.triggerRight = MapFloat<u8>(triggers[1], 0);

  return pad;
}

void GCPad::SetOutput(const ControlState strength)
{
  const auto lock = GetStateLock();
  m_rumble->controls[0]->control_ref->State(strength);
}

void GCPad::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

#ifdef ANDROID
  // Rumble
  m_rumble->SetControlExpression(0, "`Android/0/Device Sensors:Motor 0`");
#else
  // Buttons
  m_buttons->SetControlExpression(0, "`X`");       // A
  m_buttons->SetControlExpression(1, "`Z`");       // B
  m_buttons->SetControlExpression(2, "`C`");       // X
  m_buttons->SetControlExpression(3, "`S`");       // Y
  m_buttons->SetControlExpression(4, "`D`");       // Z
#ifdef _WIN32
  m_buttons->SetControlExpression(5, "`RETURN`");  // Start
#else
  // OS X/Linux
  // Start
  m_buttons->SetControlExpression(5, "`Return`");
#endif

  // D-Pad
  m_dpad->SetControlExpression(0, "`T`");  // Up
  m_dpad->SetControlExpression(1, "`G`");  // Down
  m_dpad->SetControlExpression(2, "`F`");  // Left
  m_dpad->SetControlExpression(3, "`H`");  // Right

  // C Stick
  m_c_stick->SetControlExpression(0, "`I`");  // Up
  m_c_stick->SetControlExpression(1, "`K`");  // Down
  m_c_stick->SetControlExpression(2, "`J`");  // Left
  m_c_stick->SetControlExpression(3, "`L`");  // Right
  // Modifier
  m_c_stick->SetControlExpression(4, "`Ctrl`");

  // Control Stick
#ifdef _WIN32
  m_main_stick->SetControlExpression(0, "`UP`");     // Up
  m_main_stick->SetControlExpression(1, "`DOWN`");   // Down
  m_main_stick->SetControlExpression(2, "`LEFT`");   // Left
  m_main_stick->SetControlExpression(3, "`RIGHT`");  // Right
#elif __APPLE__
  m_main_stick->SetControlExpression(0, "`Up Arrow`");     // Up
  m_main_stick->SetControlExpression(1, "`Down Arrow`");   // Down
  m_main_stick->SetControlExpression(2, "`Left Arrow`");   // Left
  m_main_stick->SetControlExpression(3, "`Right Arrow`");  // Right
#else
  m_main_stick->SetControlExpression(0, "`Up`");     // Up
  m_main_stick->SetControlExpression(1, "`Down`");   // Down
  m_main_stick->SetControlExpression(2, "`Left`");   // Left
  m_main_stick->SetControlExpression(3, "`Right`");  // Right
#endif
  // Modifier
  m_main_stick->SetControlExpression(4, "`Shift`");

  // Because our defaults use keyboard input, set calibration shapes to squares.
  m_c_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));
  m_main_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));

  // Triggers
  m_triggers->SetControlExpression(0, "`Q`");  // L
  m_triggers->SetControlExpression(1, "`W`");  // R
#endif
}

bool GCPad::GetMicButton() const
{
  const auto lock = GetStateLock();
  return m_mic->controls.back()->GetState<bool>();
}
