// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/Classic.h"

#include <array>
#include <string_view>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> classic_id{{0x00, 0x00, 0xa4, 0x20, 0x01, 0x01}};

constexpr std::array<u16, 9> classic_button_bitmasks{{
    Classic::BUTTON_A,
    Classic::BUTTON_B,
    Classic::BUTTON_X,
    Classic::BUTTON_Y,

    Classic::BUTTON_ZL,
    Classic::BUTTON_ZR,

    Classic::BUTTON_MINUS,
    Classic::BUTTON_PLUS,

    Classic::BUTTON_HOME,
}};

constexpr std::array<u16, 2> classic_trigger_bitmasks{{
    Classic::TRIGGER_L,
    Classic::TRIGGER_R,
}};

constexpr std::array<u16, 4> classic_dpad_bitmasks{{
    Classic::PAD_UP,
    Classic::PAD_DOWN,
    Classic::PAD_LEFT,
    Classic::PAD_RIGHT,
}};

Classic::Classic() : Extension1stParty("Classic", _trans("Classic Controller"))
{
  using Translatability = ControllerEmu::Translatability;

  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(BUTTONS_GROUP));
  for (auto& button_name :
       {A_BUTTON, B_BUTTON, X_BUTTON, Y_BUTTON, ZL_BUTTON, ZR_BUTTON, MINUS_BUTTON, PLUS_BUTTON})
  {
    m_buttons->AddInput(Translatability::DoNotTranslate, button_name);
  }
  m_buttons->AddInput(Translatability::DoNotTranslate, HOME_BUTTON, "HOME");

  // sticks
  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / CAL_STICK_RADIUS;
  groups.emplace_back(m_left_stick =
                          new ControllerEmu::OctagonAnalogStick(LEFT_STICK_GROUP, gate_radius));
  groups.emplace_back(m_right_stick =
                          new ControllerEmu::OctagonAnalogStick(RIGHT_STICK_GROUP, gate_radius));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(TRIGGERS_GROUP));
  for (const char* trigger_name : {L_DIGITAL, R_DIGITAL, L_ANALOG, R_ANALOG})
  {
    m_triggers->AddInput(Translatability::Translate, trigger_name);
  }

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(DPAD_GROUP));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(Translatability::Translate, named_direction);
  }
}

void Classic::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  DataFormat classic_data = {};

  // left stick
  {
    const ControllerEmu::AnalogStick::StateData left_stick_state =
        m_left_stick->GetState(m_input_override_function);

    const u8 x = MapFloat<u8>(left_stick_state.x, LEFT_STICK_CENTER, 0, LEFT_STICK_RANGE);
    const u8 y = MapFloat<u8>(left_stick_state.y, LEFT_STICK_CENTER, 0, LEFT_STICK_RANGE);

    classic_data.SetLeftStick({x, y});
  }

  // right stick
  {
    const ControllerEmu::AnalogStick::StateData right_stick_data =
        m_right_stick->GetState(m_input_override_function);

    const u8 x = MapFloat<u8>(right_stick_data.x, RIGHT_STICK_CENTER, 0, RIGHT_STICK_RANGE);
    const u8 y = MapFloat<u8>(right_stick_data.y, RIGHT_STICK_CENTER, 0, RIGHT_STICK_RANGE);

    classic_data.SetRightStick({x, y});
  }

  u16 buttons = 0;

  // triggers
  {
    ControlState triggers[2] = {0, 0};
    m_triggers->GetState(&buttons, classic_trigger_bitmasks.data(), triggers,
                         m_input_override_function);

    const u8 lt = MapFloat<u8>(triggers[0], 0, 0, TRIGGER_RANGE);
    const u8 rt = MapFloat<u8>(triggers[1], 0, 0, TRIGGER_RANGE);

    classic_data.SetLeftTrigger(lt);
    classic_data.SetRightTrigger(rt);
  }

  // buttons and dpad
  m_buttons->GetState(&buttons, classic_button_bitmasks.data(), m_input_override_function);
  m_dpad->GetState(&buttons, classic_dpad_bitmasks.data(), m_input_override_function);

  classic_data.SetButtons(buttons);

  target_state->data = classic_data;
}

void Classic::Update(const DesiredExtensionState& target_state)
{
  DefaultExtensionUpdate<DataFormat>(&m_reg, target_state);
}

void Classic::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = classic_id;

  // Build calibration data:
  // All values are to 8 bits of precision.
  m_reg.calibration = {{
      // Left Stick X max,min,center:
      CAL_STICK_CENTER + STICK_GATE_RADIUS,
      CAL_STICK_CENTER - STICK_GATE_RADIUS,
      CAL_STICK_CENTER,
      // Left Stick Y max,min,center:
      CAL_STICK_CENTER + STICK_GATE_RADIUS,
      CAL_STICK_CENTER - STICK_GATE_RADIUS,
      CAL_STICK_CENTER,
      // Right Stick X max,min,center:
      CAL_STICK_CENTER + STICK_GATE_RADIUS,
      CAL_STICK_CENTER - STICK_GATE_RADIUS,
      CAL_STICK_CENTER,
      // Right Stick Y max,min,center:
      CAL_STICK_CENTER + STICK_GATE_RADIUS,
      CAL_STICK_CENTER - STICK_GATE_RADIUS,
      CAL_STICK_CENTER,
      // Left/Right trigger neutrals.
      0,
      0,
      // 2 checksum bytes calculated below:
      0x00,
      0x00,
  }};

  UpdateCalibrationDataChecksum(m_reg.calibration, CALIBRATION_CHECKSUM_BYTES);
}

ControllerEmu::ControlGroup* Classic::GetGroup(ClassicGroup group)
{
  switch (group)
  {
  case ClassicGroup::Buttons:
    return m_buttons;
  case ClassicGroup::Triggers:
    return m_triggers;
  case ClassicGroup::DPad:
    return m_dpad;
  case ClassicGroup::LeftStick:
    return m_left_stick;
  case ClassicGroup::RightStick:
    return m_right_stick;
  default:
    ASSERT(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
