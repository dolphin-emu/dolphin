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

namespace
{
constexpr std::array<u8, 6> GenerateClassicControllerID(bool is_pro_controller)
{
  return {{is_pro_controller, 0x00, 0xa4, 0x20, 0x01, 0x01}};
}

class DisableableMixedTriggers : public ControllerEmu::MixedTriggers
{
public:
  DisableableMixedTriggers(const ControllerEmu::SettingValue<bool>* disable_setting)
      : MixedTriggers(_trans("Triggers")), m_analog_disabled(*disable_setting)
  {
  }

  bool IsAnalogEnabled() const override { return !m_analog_disabled.GetValue(); }

private:
  const ControllerEmu::SettingValue<bool>& m_analog_disabled;
};

}  // namespace

namespace WiimoteEmu
{
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
  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / CAL_STICK_RANGE;
  groups.emplace_back(m_left_stick =
                          new ControllerEmu::OctagonAnalogStick(_trans("Left Stick"), gate_radius));
  groups.emplace_back(
      m_right_stick = new ControllerEmu::OctagonAnalogStick(_trans("Right Stick"), gate_radius));

  // triggers
  groups.emplace_back(m_triggers = new DisableableMixedTriggers(&m_cc_pro_setting));
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

  // options
  m_options =
      groups.emplace_back(std::make_unique<ControllerEmu::ControlGroup>(_trans("Options"))).get();

  m_options->AddSetting(&m_cc_pro_setting, {_trans("Classic Controller Pro")}, false);
}

void Classic::Update()
{
  DataFormat classic_data = {};

  // left stick
  {
    const ControllerEmu::AnalogStick::StateData left_stick_state = m_left_stick->GetState();

    classic_data.lx = static_cast<u8>(LEFT_STICK_CENTER + (left_stick_state.x * LEFT_STICK_RADIUS));
    classic_data.ly = static_cast<u8>(LEFT_STICK_CENTER + (left_stick_state.y * LEFT_STICK_RADIUS));
  }

  // right stick
  {
    const ControllerEmu::AnalogStick::StateData right_stick_data = m_right_stick->GetState();

    const u8 x = static_cast<u8>(RIGHT_STICK_CENTER + (right_stick_data.x * RIGHT_STICK_RADIUS));
    const u8 y = static_cast<u8>(RIGHT_STICK_CENTER + (right_stick_data.y * RIGHT_STICK_RADIUS));

    classic_data.rx1 = x;
    classic_data.rx2 = x >> 1;
    classic_data.rx3 = x >> 3;
    classic_data.ry = y;
  }

  // triggers
  {
    ControlState trigs[2] = {0, 0};
    m_triggers->GetState(&classic_data.bt.hex, classic_trigger_bitmasks.data(), trigs);

    if (IsCurrentlyClassicControllerPro())
    {
      // CC Pro trigger values are either 0 OR 31 depending on the trigger button state.
      trigs[0] = classic_data.bt.lt;
      trigs[1] = classic_data.bt.rt;
    }

    const u8 lt = static_cast<u8>(trigs[0] * TRIGGER_RANGE);
    const u8 rt = static_cast<u8>(trigs[1] * TRIGGER_RANGE);

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

  m_reg.identifier = GenerateClassicControllerID(m_cc_pro_setting.GetValue());

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

bool Classic::IsResetNeeded() const
{
  // If we wish to change the type of controller we are emulating the extension must be reconnected.
  return IsCurrentlyClassicControllerPro() != m_cc_pro_setting.GetValue();
}

bool Classic::IsCurrentlyClassicControllerPro() const
{
  return m_reg.identifier[0] == 0x01;
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
  case ClassicGroup::Options:
    return m_options;
  default:
    assert(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
