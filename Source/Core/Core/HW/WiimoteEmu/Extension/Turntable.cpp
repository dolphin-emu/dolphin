// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Extension/Turntable.h"

#include <array>
#include <cassert>
#include <cstring>

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> turntable_id{{0x03, 0x00, 0xa4, 0x20, 0x01, 0x03}};

constexpr std::array<u16, 9> turntable_button_bitmasks{{
    Turntable::BUTTON_L_GREEN,
    Turntable::BUTTON_L_RED,
    Turntable::BUTTON_L_BLUE,
    Turntable::BUTTON_R_GREEN,
    Turntable::BUTTON_R_RED,
    Turntable::BUTTON_R_BLUE,
    Turntable::BUTTON_MINUS,
    Turntable::BUTTON_PLUS,
    Turntable::BUTTON_EUPHORIA,
}};

constexpr std::array<const char*, 6> turntable_button_names{{
    _trans("Green Left"),
    _trans("Red Left"),
    _trans("Blue Left"),
    _trans("Green Right"),
    _trans("Red Right"),
    _trans("Blue Right"),
}};

Turntable::Turntable() : Extension1stParty("Turntable", _trans("DJ Turntable"))
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (auto& turntable_button_name : turntable_button_names)
  {
    m_buttons->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::Translate, turntable_button_name));
  }

  m_buttons->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "-"));
  m_buttons->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "+"));

  m_buttons->controls.emplace_back(
      // i18n: This button name refers to a gameplay element in DJ Hero
      new ControllerEmu::Input(ControllerEmu::Translate, _trans("Euphoria")));

  // turntables
  // i18n: "Table" refers to a turntable
  groups.emplace_back(m_left_table = new ControllerEmu::Slider("Table Left", _trans("Left Table")));
  groups.emplace_back(m_right_table =
                          // i18n: "Table" refers to a turntable
                      new ControllerEmu::Slider("Table Right", _trans("Right Table")));

  // stick
  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / STICK_RADIUS;
  groups.emplace_back(m_stick =
                          new ControllerEmu::OctagonAnalogStick(_trans("Stick"), gate_radius));

  // effect dial
  groups.emplace_back(m_effect_dial = new ControllerEmu::Slider(_trans("Effect")));

  // crossfade
  groups.emplace_back(m_crossfade = new ControllerEmu::Slider(_trans("Crossfade")));
}

void Turntable::Update()
{
  DataFormat tt_data = {};

  // stick
  {
    const ControllerEmu::AnalogStick::StateData stick_state = m_stick->GetState();

    tt_data.sx = static_cast<u8>((stick_state.x * STICK_RADIUS) + STICK_CENTER);
    tt_data.sy = static_cast<u8>((stick_state.y * STICK_RADIUS) + STICK_CENTER);
  }

  // left table
  {
    const ControllerEmu::Slider::StateData lt = m_left_table->GetState();
    const s8 tt = static_cast<s8>(lt.value * TABLE_RANGE);

    tt_data.ltable1 = tt;
    tt_data.ltable2 = tt >> 5;
  }

  // right table
  {
    const ControllerEmu::Slider::StateData rt = m_right_table->GetState();
    const s8 tt = static_cast<s8>(rt.value * TABLE_RANGE);

    tt_data.rtable1 = tt;
    tt_data.rtable2 = tt >> 1;
    tt_data.rtable3 = tt >> 3;
    tt_data.rtable4 = tt >> 5;
  }

  // effect dial
  {
    const auto dial_state = m_effect_dial->GetState();
    const u8 dial = static_cast<u8>(dial_state.value * EFFECT_DIAL_RANGE) + EFFECT_DIAL_CENTER;

    tt_data.dial1 = dial;
    tt_data.dial2 = dial >> 3;
  }

  // crossfade slider
  {
    const ControllerEmu::Slider::StateData cfs = m_crossfade->GetState();

    tt_data.slider = static_cast<u8>((cfs.value * CROSSFADE_RANGE) + CROSSFADE_CENTER);
  }

  // buttons
  m_buttons->GetState(&tt_data.bt, turntable_button_bitmasks.data());

  // flip button bits :/
  tt_data.bt ^= (BUTTON_L_GREEN | BUTTON_L_RED | BUTTON_L_BLUE | BUTTON_R_GREEN | BUTTON_R_RED |
                 BUTTON_R_BLUE | BUTTON_MINUS | BUTTON_PLUS | BUTTON_EUPHORIA);

  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = tt_data;
}

bool Turntable::IsButtonPressed() const
{
  u16 buttons = 0;
  m_buttons->GetState(&buttons, turntable_button_bitmasks.data());
  return buttons != 0;
}

void Turntable::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = turntable_id;

  // TODO: Is there calibration data?
}

ControllerEmu::ControlGroup* Turntable::GetGroup(TurntableGroup group)
{
  switch (group)
  {
  case TurntableGroup::Buttons:
    return m_buttons;
  case TurntableGroup::Stick:
    return m_stick;
  case TurntableGroup::EffectDial:
    return m_effect_dial;
  case TurntableGroup::LeftTable:
    return m_left_table;
  case TurntableGroup::RightTable:
    return m_right_table;
  case TurntableGroup::Crossfade:
    return m_crossfade;
  default:
    assert(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
