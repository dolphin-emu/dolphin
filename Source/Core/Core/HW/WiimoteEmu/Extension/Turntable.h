// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class Slider;
class Triggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class TurntableGroup
{
  Buttons,
  Stick,
  EffectDial,
  LeftTable,
  RightTable,
  Crossfade
};

// The DJ Hero Turntable uses the "1st-party" extension encryption scheme.
class Turntable : public Extension1stParty
{
public:
  struct DataFormat
  {
    u8 sx : 6;
    u8 rtable3 : 2;

    u8 sy : 6;
    u8 rtable2 : 2;

    u8 rtable4 : 1;
    u8 slider : 4;
    u8 dial2 : 2;
    u8 rtable1 : 1;

    u8 ltable1 : 5;
    u8 dial1 : 3;

    union
    {
      u16 ltable2 : 1;
      u16 bt;  // buttons
    };
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  Turntable();

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(TurntableGroup group);

  static constexpr u16 BUTTON_EUPHORIA = 0x1000;

  static constexpr u16 BUTTON_L_GREEN = 0x0800;
  static constexpr u16 BUTTON_L_RED = 0x20;
  static constexpr u16 BUTTON_L_BLUE = 0x8000;

  static constexpr u16 BUTTON_R_GREEN = 0x2000;
  static constexpr u16 BUTTON_R_RED = 0x02;
  static constexpr u16 BUTTON_R_BLUE = 0x0400;

  static constexpr u16 BUTTON_MINUS = 0x10;
  static constexpr u16 BUTTON_PLUS = 0x04;

  static constexpr int STICK_BIT_COUNT = 6;
  static constexpr u8 STICK_CENTER = (1 << STICK_BIT_COUNT) / 2;
  static constexpr u8 STICK_RADIUS = STICK_CENTER - 1;
  static constexpr u8 STICK_RANGE = (1 << STICK_BIT_COUNT) - 1;
  // TODO: Test real hardware. Is this accurate?
  static constexpr u8 STICK_GATE_RADIUS = 0x16;

  static constexpr int TABLE_BIT_COUNT = 6;
  static constexpr u8 TABLE_RANGE = (1 << TABLE_BIT_COUNT) / 2 - 1;

  static constexpr int EFFECT_DIAL_BIT_COUNT = 5;
  static constexpr u8 EFFECT_DIAL_CENTER = (1 << EFFECT_DIAL_BIT_COUNT) / 2;
  static constexpr u8 EFFECT_DIAL_RANGE = (1 << EFFECT_DIAL_BIT_COUNT) - 1;

  static constexpr int CROSSFADE_BIT_COUNT = 4;
  static constexpr u8 CROSSFADE_CENTER = (1 << CROSSFADE_BIT_COUNT) / 2;
  static constexpr u8 CROSSFADE_RANGE = (1 << CROSSFADE_BIT_COUNT) - 1;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Slider* m_effect_dial;
  ControllerEmu::Slider* m_left_table;
  ControllerEmu::Slider* m_right_table;
  ControllerEmu::Slider* m_crossfade;
};
}  // namespace WiimoteEmu
