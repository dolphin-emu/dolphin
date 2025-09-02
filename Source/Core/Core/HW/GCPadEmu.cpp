// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCPadEmu.h"

#include <array>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/GCPad.h"

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
  using Translatability = ControllerEmu::Translatability;

  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(BUTTONS_GROUP));
  for (const char* named_button : {A_BUTTON, B_BUTTON, X_BUTTON, Y_BUTTON, Z_BUTTON})
  {
    m_buttons->AddInput(Translatability::DoNotTranslate, named_button);
  }
  // i18n: The START/PAUSE button on GameCube controllers
  m_buttons->AddInput(Translatability::Translate, START_BUTTON, _trans("START"));

  // sticks
  groups.emplace_back(m_main_stick = new ControllerEmu::OctagonAnalogStick(
                          MAIN_STICK_GROUP, _trans("Control Stick"), MAIN_STICK_GATE_RADIUS));
  groups.emplace_back(m_c_stick = new ControllerEmu::OctagonAnalogStick(
                          C_STICK_GROUP, _trans("C-Stick"), C_STICK_GATE_RADIUS));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(TRIGGERS_GROUP));
  for (const char* named_trigger : {L_DIGITAL, R_DIGITAL, L_ANALOG, R_ANALOG})
  {
    m_triggers->AddInput(Translatability::Translate, named_trigger);
  }

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(DPAD_GROUP));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(Translatability::Translate, named_direction);
  }

  // Microphone
  groups.emplace_back(m_mic = new ControllerEmu::Buttons(MIC_GROUP));
  m_mic->AddInput(Translatability::Translate, _trans("Button"));

  // rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(RUMBLE_GROUP));
  m_rumble->AddOutput(Translatability::Translate, _trans("Motor"));

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

InputConfig* GCPad::GetConfig() const
{
  return Pad::GetConfig();
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
  using ControllerEmu::MapFloat;

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

  // Rumble
  m_rumble->SetControlExpression(0, "Motor");

  // Buttons: A, B, X, Y, Z
  m_buttons->SetControlExpression(0, "G|`Button S`");
  m_buttons->SetControlExpression(1, "F|`Button W`");
  m_buttons->SetControlExpression(2, "H|`Button E`");
  m_buttons->SetControlExpression(3, "T|`Button N`");
#ifdef __APPLE__
  m_buttons->SetControlExpression(4, "Option|Tab|`Shoulder R`|Back");
#else
  m_buttons->SetControlExpression(4, "Alt|Tab|`Shoulder R`|Back");
#endif

#ifdef _WIN32
  m_buttons->SetControlExpression(5, "Enter|Start");  // Start
#else
  // OS X/Linux
  // Start
  m_buttons->SetControlExpression(5, "Return|Start");
#endif

  // D-Pad
  m_dpad->SetControlExpression(0, "Up|`Pad N`");  // Up
  m_dpad->SetControlExpression(1, "Down|`Pad S`");  // Down
  m_dpad->SetControlExpression(2, "Left|`Pad W`");  // Left
  m_dpad->SetControlExpression(3, "Right|`Pad E`");  // Right

  // Control Stick
  m_main_stick->SetControlExpression(0, "W|`Left Y+`");     // Up
  m_main_stick->SetControlExpression(1, "S|`Left Y-`");   // Down
  m_main_stick->SetControlExpression(2, "A|`Left X-`");   // Left
  m_main_stick->SetControlExpression(3, "D|`Left X+`");  // Right
  // Modifier
  m_main_stick->SetControlExpression(4, "`Left Shift`");

  // C-Stick
  m_c_stick->SetControlExpression(0, "I|`Right Y+`");  // Up
  m_c_stick->SetControlExpression(1, "K|`Right Y-`");  // Down
  m_c_stick->SetControlExpression(2, "J|`Right X-`");  // Left
  m_c_stick->SetControlExpression(3, "L|`Right X+`");  // Right
  // Modifier
  m_c_stick->SetControlExpression(4, "`Right Shift`");

  // Because our defaults use keyboard input, set calibration shapes to squares.
  m_c_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));
  m_main_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));

  // Triggers
#ifdef _WIN32
  m_triggers->SetControlExpression(0, "Ctrl|`Trigger L`");  // L (digital)
#else
  m_triggers->SetControlExpression(0, "Control|`Trigger L`");  // L (digital)
  m_triggers->SetControlExpression(1, "Space|`Trigger R`");  // R (digital)
  m_triggers->SetControlExpression(2, "`Trigger L`");  // L (analog)
  m_triggers->SetControlExpression(3, "`Trigger R`");  // R (analog)
#endif
}

bool GCPad::GetMicButton() const
{
  const auto lock = GetStateLock();
  return m_mic->controls.back()->GetState<bool>();
}
