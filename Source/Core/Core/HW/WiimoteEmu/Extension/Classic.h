// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class MixedTriggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class ClassicGroup
{
  Buttons,
  Triggers,
  DPad,
  LeftStick,
  RightStick
};

class Classic : public Extension1stParty
{
public:
  union ButtonFormat
  {
    u16 hex;

    struct
    {
      u8 : 1;
      u8 rt : 1;  // right trigger
      u8 plus : 1;
      u8 home : 1;
      u8 minus : 1;
      u8 lt : 1;  // left trigger
      u8 dpad_down : 1;
      u8 dpad_right : 1;

      u8 dpad_up : 1;
      u8 dpad_left : 1;
      u8 zr : 1;
      u8 x : 1;
      u8 a : 1;
      u8 y : 1;
      u8 b : 1;
      u8 zl : 1;  // left z button
    };
  };
  static_assert(sizeof(ButtonFormat) == 2, "Wrong size");

  struct DataFormat
  {
    // lx/ly/lz; left joystick
    // rx/ry/rz; right joystick
    // lt; left trigger
    // rt; left trigger

    u8 lx : 6;  // byte 0
    u8 rx3 : 2;

    u8 ly : 6;  // byte 1
    u8 rx2 : 2;

    u8 ry : 5;
    u8 lt2 : 2;
    u8 rx1 : 1;

    u8 rt : 5;
    u8 lt1 : 3;

    ButtonFormat bt;  // byte 4, 5
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  Classic();

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(ClassicGroup group);

  enum
  {
    PAD_RIGHT = 0x80,
    PAD_DOWN = 0x40,
    TRIGGER_L = 0x20,
    BUTTON_MINUS = 0x10,
    BUTTON_HOME = 0x08,
    BUTTON_PLUS = 0x04,
    TRIGGER_R = 0x02,
    NOTHING = 0x01,
    BUTTON_ZL = 0x8000,
    BUTTON_B = 0x4000,
    BUTTON_Y = 0x2000,
    BUTTON_A = 0x1000,
    BUTTON_X = 0x0800,
    BUTTON_ZR = 0x0400,
    PAD_LEFT = 0x0200,
    PAD_UP = 0x0100,
  };

  enum
  {
    CAL_STICK_CENTER = 0x80,
    CAL_STICK_RANGE = 0x7f,
    CAL_STICK_BITS = 8,

    LEFT_STICK_BITS = 6,
    LEFT_STICK_CENTER_X = CAL_STICK_CENTER >> (CAL_STICK_BITS - LEFT_STICK_BITS),
    LEFT_STICK_CENTER_Y = CAL_STICK_CENTER >> (CAL_STICK_BITS - LEFT_STICK_BITS),
    LEFT_STICK_RADIUS = CAL_STICK_RANGE >> (CAL_STICK_BITS - LEFT_STICK_BITS),

    RIGHT_STICK_BITS = 5,
    RIGHT_STICK_CENTER_X = CAL_STICK_CENTER >> (CAL_STICK_BITS - RIGHT_STICK_BITS),
    RIGHT_STICK_CENTER_Y = CAL_STICK_CENTER >> (CAL_STICK_BITS - RIGHT_STICK_BITS),
    RIGHT_STICK_RADIUS = CAL_STICK_RANGE >> (CAL_STICK_BITS - RIGHT_STICK_BITS),

    LEFT_TRIGGER_RANGE = 0x1F,
    RIGHT_TRIGGER_RANGE = 0x1F,
  };

  static const u8 STICK_GATE_RADIUS = 0x16;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::MixedTriggers* m_triggers;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::AnalogStick* m_left_stick;
  ControllerEmu::AnalogStick* m_right_stick;
};
}  // namespace WiimoteEmu
