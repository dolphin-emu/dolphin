// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Extension/Classic.h"

#include <array>
#include <cassert>
#include <string_view>

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
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

constexpr std::array<std::string_view, 9> classic_button_names{{
    "A",
    "B",
    "X",
    "Y",
    "ZL",
    "ZR",
    "-",
    "+",
    "Home",
}};

constexpr std::array<u16, 2> classic_trigger_bitmasks{{
    Classic::TRIGGER_L,
    Classic::TRIGGER_R,
}};

constexpr std::array<const char*, 4> classic_trigger_names{{
    // i18n: The left trigger button (labeled L on real controllers)
    _trans("L"),
    // i18n: The right trigger button (labeled R on real controllers)
    _trans("R"),
    // i18n: The left trigger button (labeled L on real controllers) used as an analog input
    _trans("L-Analog"),
    // i18n: The right trigger button (labeled R on real controllers) used as an analog input
    _trans("R-Analog"),
}};

constexpr std::array<u16, 4> classic_dpad_bitmasks{{
    Classic::PAD_UP,
    Classic::PAD_DOWN,
    Classic::PAD_LEFT,
    Classic::PAD_RIGHT,
}};

Classic::Classic() : Extension1stParty("Classic", _trans("Classic Controller"))
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (auto& button_name : classic_button_names)
  {
    std::string_view ui_name = (button_name == "Home") ? "HOME" : button_name;
    m_buttons->AddInput(ControllerEmu::DoNotTranslate, std::string(button_name),
                        std::string(ui_name));
  }

  // sticks
  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / CAL_STICK_RANGE;
  groups.emplace_back(m_left_stick =
                          new ControllerEmu::OctagonAnalogStick(_trans("Left Stick"), gate_radius));
  groups.emplace_back(
      m_right_stick = new ControllerEmu::OctagonAnalogStick(_trans("Right Stick"), gate_radius));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(_trans("Triggers")));
  for (const char* trigger_name : classic_trigger_names)
  {
    m_triggers->AddInput(ControllerEmu::Translate, trigger_name);
  }

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }
}

void Classic::Update()
{
  DataFormat classic_data = {};

  // left stick
  {
    const ControllerEmu::AnalogStick::StateData left_stick_state = m_left_stick->GetState();

    const u8 x = static_cast<u8>(LEFT_STICK_CENTER + (left_stick_state.x * LEFT_STICK_RADIUS));
    const u8 y = static_cast<u8>(LEFT_STICK_CENTER + (left_stick_state.y * LEFT_STICK_RADIUS));

    classic_data.SetLeftStick({x, y});
  }

  // right stick
  {
    const ControllerEmu::AnalogStick::StateData right_stick_data = m_right_stick->GetState();

    const u8 x = static_cast<u8>(RIGHT_STICK_CENTER + (right_stick_data.x * RIGHT_STICK_RADIUS));
    const u8 y = static_cast<u8>(RIGHT_STICK_CENTER + (right_stick_data.y * RIGHT_STICK_RADIUS));

    classic_data.SetRightStick({x, y});
  }

  u16 buttons = 0;

  // triggers
  {
    ControlState trigs[2] = {0, 0};
    m_triggers->GetState(&buttons, classic_trigger_bitmasks.data(), trigs);

    const u8 lt = static_cast<u8>(trigs[0] * TRIGGER_RANGE);
    const u8 rt = static_cast<u8>(trigs[1] * TRIGGER_RANGE);

    classic_data.SetLeftTrigger(lt);
    classic_data.SetRightTrigger(rt);
  }

  // buttons and dpad
  m_buttons->GetState(&buttons, classic_button_bitmasks.data());
  m_dpad->GetState(&buttons, classic_dpad_bitmasks.data());

  classic_data.SetButtons(buttons);

  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = classic_data;
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
    assert(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
