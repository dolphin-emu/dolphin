// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitFieldView.h"

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
#pragma pack(push, 1)
  struct DataFormat
  {
    u32 _data1;

    BFVIEW_IN(_data1, u8, 0, 6, sx);
    BFVIEW_IN(_data1, u8, 6, 2, rtable3);
    BFVIEW_IN(_data1, u8, 8, 6, sy);
    BFVIEW_IN(_data1, u8, 14, 2, rtable2);
    BFVIEW_IN(_data1, u8, 16, 1, rtable4);
    BFVIEW_IN(_data1, u8, 17, 4, slider);
    BFVIEW_IN(_data1, u8, 21, 2, dial2);
    BFVIEW_IN(_data1, u8, 23, 1, rtable1);
    BFVIEW_IN(_data1, u8, 24, 5, ltable1);
    BFVIEW_IN(_data1, u8, 29, 3, dial1);

    union
    {
      u16 _data2;
      u16 bt;  // buttons
    };

    BFVIEW_IN(_data2, u8, 0, 1, ltable2);
  };
#pragma pack(pop)
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  Turntable();

  void Update() override;
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
  // TODO: Test real hardware. Is this accurate?
  static constexpr u8 STICK_GATE_RADIUS = 0x16;

  static constexpr int TABLE_BIT_COUNT = 6;
  static constexpr u8 TABLE_RANGE = (1 << STICK_BIT_COUNT) / 2 - 1;

  static constexpr int EFFECT_DIAL_BIT_COUNT = 5;
  static constexpr u8 EFFECT_DIAL_CENTER = (1 << EFFECT_DIAL_BIT_COUNT) / 2;
  static constexpr u8 EFFECT_DIAL_RANGE = EFFECT_DIAL_CENTER - 1;

  static constexpr int CROSSFADE_BIT_COUNT = 4;
  static constexpr u8 CROSSFADE_CENTER = (1 << CROSSFADE_BIT_COUNT) / 2;
  static constexpr u8 CROSSFADE_RANGE = CROSSFADE_CENTER - 1;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Slider* m_effect_dial;
  ControllerEmu::Slider* m_left_table;
  ControllerEmu::Slider* m_right_table;
  ControllerEmu::Slider* m_crossfade;
};
}  // namespace WiimoteEmu
