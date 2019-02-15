// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

// TODO: Does the turntable ever use encryption?
class Turntable : public EncryptedExtension
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

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(TurntableGroup group);

  enum
  {
    BUTTON_EUPHORIA = 0x1000,

    BUTTON_L_GREEN = 0x0800,
    BUTTON_L_RED = 0x20,
    BUTTON_L_BLUE = 0x8000,

    BUTTON_R_GREEN = 0x2000,
    BUTTON_R_RED = 0x02,
    BUTTON_R_BLUE = 0x0400,

    BUTTON_MINUS = 0x10,
    BUTTON_PLUS = 0x04,
  };

  static const u8 STICK_CENTER = 0x20;
  static const u8 STICK_RADIUS = 0x1f;

  // TODO: Test real hardware. Is this accurate?
  static const u8 STICK_GATE_RADIUS = 0x16;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Triggers* m_effect_dial;
  ControllerEmu::Slider* m_left_table;
  ControllerEmu::Slider* m_right_table;
  ControllerEmu::Slider* m_crossfade;
};
}  // namespace WiimoteEmu
