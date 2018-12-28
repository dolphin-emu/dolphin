// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class MixedTriggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class ClassicGroup;
struct ExtensionReg;

class Classic : public Attachment
{
public:
  explicit Classic(ExtensionReg& reg);
  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

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
