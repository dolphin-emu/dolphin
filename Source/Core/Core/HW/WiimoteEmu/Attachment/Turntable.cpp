// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Turntable.h"

#include <array>
#include <cassert>

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
    Turntable::BUTTON_L_GREEN, Turntable::BUTTON_L_RED, Turntable::BUTTON_L_BLUE,
    Turntable::BUTTON_R_GREEN, Turntable::BUTTON_R_RED, Turntable::BUTTON_R_BLUE,
    Turntable::BUTTON_MINUS, Turntable::BUTTON_PLUS, Turntable::BUTTON_EUPHORIA,
}};

constexpr std::array<const char*, 9> turntable_button_names{{
    _trans("Green Left"), _trans("Red Left"), _trans("Blue Left"), _trans("Green Right"),
    _trans("Red Right"), _trans("Blue Right"), "-", "+",
    // i18n: This button name refers to a gameplay element in DJ Hero
    _trans("Euphoria"),
}};

Turntable::Turntable(ExtensionReg& reg) : Attachment(_trans("Turntable"), reg)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (auto& turntable_button_name : turntable_button_names)
    m_buttons->controls.emplace_back(new ControllerEmu::Input(turntable_button_name));

  // turntables
  // i18n: "Table" refers to a turntable
  groups.emplace_back(m_left_table = new ControllerEmu::Slider("Table Left", _trans("Left Table")));
  groups.emplace_back(m_right_table =
                          // i18n: "Table" refers to a turntable
                      new ControllerEmu::Slider("Table Right", _trans("Right Table")));

  // stick
  groups.emplace_back(
      m_stick = new ControllerEmu::AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

  // effect dial
  groups.emplace_back(m_effect_dial = new ControllerEmu::Triggers(_trans("Effect")));
  m_effect_dial->controls.emplace_back(new ControllerEmu::Input(_trans("Dial")));

  // crossfade
  groups.emplace_back(m_crossfade = new ControllerEmu::Slider(_trans("Crossfade")));

  // set up register
  m_id = turntable_id;
}

void Turntable::GetState(u8* const data)
{
  wm_turntable_extension* const ttdata = reinterpret_cast<wm_turntable_extension*>(data);
  ttdata->bt = 0;

  // stick
  {
    ControlState x, y;
    m_stick->GetState(&x, &y);

    ttdata->sx = static_cast<u8>((x * 0x1F) + 0x20);
    ttdata->sy = static_cast<u8>((y * 0x1F) + 0x20);
  }

  // left table
  {
    ControlState tt;
    s8 tt_;
    m_left_table->GetState(&tt);

    tt_ = static_cast<s8>(tt * 0x1F);

    ttdata->ltable1 = tt_;
    ttdata->ltable2 = tt_ >> 5;
  }

  // right table
  {
    ControlState tt;
    s8 tt_;
    m_right_table->GetState(&tt);

    tt_ = static_cast<s8>(tt * 0x1F);

    ttdata->rtable1 = tt_;
    ttdata->rtable2 = tt_ >> 1;
    ttdata->rtable3 = tt_ >> 3;
    ttdata->rtable4 = tt_ >> 5;
  }

  // effect dial
  {
    ControlState dial;
    u8 dial_;
    m_effect_dial->GetState(&dial);

    dial_ = static_cast<u8>(dial * 0x0F);

    ttdata->dial1 = dial_;
    ttdata->dial2 = dial_ >> 3;
  }

  // crossfade slider
  {
    ControlState cfs;
    m_crossfade->GetState(&cfs);

    ttdata->slider = static_cast<u8>((cfs * 0x07) + 0x08);
  }

  // buttons
  m_buttons->GetState(&ttdata->bt, turntable_button_bitmasks.data());

  // flip button bits :/
  ttdata->bt ^= (BUTTON_L_GREEN | BUTTON_L_RED | BUTTON_L_BLUE | BUTTON_R_GREEN | BUTTON_R_RED |
                 BUTTON_R_BLUE | BUTTON_MINUS | BUTTON_PLUS | BUTTON_EUPHORIA);
}

bool Turntable::IsButtonPressed() const
{
  u16 buttons = 0;
  m_buttons->GetState(&buttons, turntable_button_bitmasks.data());
  return buttons != 0;
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
}
