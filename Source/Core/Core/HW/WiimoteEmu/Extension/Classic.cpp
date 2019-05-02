// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Extension/Classic.h"

#include <array>
#include <cassert>

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

constexpr std::array<const char*, 9> classic_button_names{{
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
  for (const char* button_name : classic_button_names)
  {
    const std::string& ui_name = (button_name == std::string("Home")) ? "HOME" : button_name;
    m_buttons->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::DoNotTranslate, button_name, ui_name));
  }

  // sticks
  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / LEFT_STICK_RADIUS;
  groups.emplace_back(m_left_stick =
                          new ControllerEmu::OctagonAnalogStick(_trans("Left Stick"), gate_radius));
  groups.emplace_back(
      m_right_stick = new ControllerEmu::OctagonAnalogStick(_trans("Right Stick"), gate_radius));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(_trans("Triggers")));
  for (const char* trigger_name : classic_trigger_names)
  {
    m_triggers->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::Translate, trigger_name));
  }

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::Translate, named_direction));
  }
}

void Classic::Update()
{
  DataFormat classic_data = {};

  // left stick
  {
    const ControllerEmu::AnalogStick::StateData left_stick_state = m_left_stick->GetState();

    classic_data.lx = static_cast<u8>(Classic::LEFT_STICK_CENTER_X +
                                      (left_stick_state.x * Classic::LEFT_STICK_RADIUS));
    classic_data.ly = static_cast<u8>(Classic::LEFT_STICK_CENTER_Y +
                                      (left_stick_state.y * Classic::LEFT_STICK_RADIUS));
  }

  // right stick
  {
    const ControllerEmu::AnalogStick::StateData right_stick_data = m_right_stick->GetState();

    const u8 x = static_cast<u8>(Classic::RIGHT_STICK_CENTER_X +
                                 (right_stick_data.x * Classic::RIGHT_STICK_RADIUS));
    const u8 y = static_cast<u8>(Classic::RIGHT_STICK_CENTER_Y +
                                 (right_stick_data.y * Classic::RIGHT_STICK_RADIUS));

    classic_data.rx1 = x;
    classic_data.rx2 = x >> 1;
    classic_data.rx3 = x >> 3;
    classic_data.ry = y;
  }

  // triggers
  {
    ControlState trigs[2] = {0, 0};
    m_triggers->GetState(&classic_data.bt.hex, classic_trigger_bitmasks.data(), trigs);

    const u8 lt = static_cast<u8>(trigs[0] * Classic::LEFT_TRIGGER_RANGE);
    const u8 rt = static_cast<u8>(trigs[1] * Classic::RIGHT_TRIGGER_RANGE);

    classic_data.lt1 = lt;
    classic_data.lt2 = lt >> 3;
    classic_data.rt = rt;
  }

  // buttons
  m_buttons->GetState(&classic_data.bt.hex, classic_button_bitmasks.data());
  // dpad
  m_dpad->GetState(&classic_data.bt.hex, classic_dpad_bitmasks.data());

  // flip button bits
  classic_data.bt.hex ^= 0xFFFF;

  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = classic_data;
}

bool Classic::IsButtonPressed() const
{
  u16 buttons = 0;
  std::array<ControlState, 2> trigs{};
  m_buttons->GetState(&buttons, classic_button_bitmasks.data());
  m_dpad->GetState(&buttons, classic_dpad_bitmasks.data());
  m_triggers->GetState(&buttons, classic_trigger_bitmasks.data(), trigs.data());
  return buttons != 0;
}

void Classic::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = classic_id;

  // Build calibration data:
  m_reg.calibration = {{
      // Left Stick X max,min,center:
      CAL_STICK_CENTER + CAL_STICK_RANGE,
      CAL_STICK_CENTER - CAL_STICK_RANGE,
      CAL_STICK_CENTER,
      // Left Stick Y max,min,center:
      CAL_STICK_CENTER + CAL_STICK_RANGE,
      CAL_STICK_CENTER - CAL_STICK_RANGE,
      CAL_STICK_CENTER,
      // Right Stick X max,min,center:
      CAL_STICK_CENTER + CAL_STICK_RANGE,
      CAL_STICK_CENTER - CAL_STICK_RANGE,
      CAL_STICK_CENTER,
      // Right Stick Y max,min,center:
      CAL_STICK_CENTER + CAL_STICK_RANGE,
      CAL_STICK_CENTER - CAL_STICK_RANGE,
      CAL_STICK_CENTER,
      // Left/Right trigger range: (assumed based on real calibration data values)
      LEFT_TRIGGER_RANGE,
      RIGHT_TRIGGER_RANGE,
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
