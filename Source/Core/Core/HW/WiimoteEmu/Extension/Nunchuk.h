// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class Force;
class Tilt;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class NunchukGroup
{
  Buttons,
  Stick,
  Tilt,
  Swing,
  Shake
};

class Nunchuk : public Extension1stParty
{
public:
  union ButtonFormat
  {
    u8 hex;

    struct
    {
      u8 z : 1;
      u8 c : 1;

      // LSBs of accelerometer
      u8 acc_x_lsb : 2;
      u8 acc_y_lsb : 2;
      u8 acc_z_lsb : 2;
    };
  };
  static_assert(sizeof(ButtonFormat) == 1, "Wrong size");

  union DataFormat
  {
    struct
    {
      // joystick x, y
      u8 jx;
      u8 jy;

      // accelerometer
      u8 ax;
      u8 ay;
      u8 az;

      // buttons + accelerometer LSBs
      ButtonFormat bt;
    };
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  Nunchuk();

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ControllerEmu::ControlGroup* GetGroup(NunchukGroup group);

  static constexpr u8 BUTTON_C = 0x02;
  static constexpr u8 BUTTON_Z = 0x01;

  static constexpr u8 ACCEL_ZERO_G = 0x80;
  static constexpr u8 ACCEL_ONE_G = 0xB3;

  static constexpr u8 STICK_CENTER = 0x80;
  static constexpr u8 STICK_RADIUS = 0x7F;
  static constexpr u8 STICK_GATE_RADIUS = 0x52;

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::Shake* m_shake;
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;

  // Dynamics:
  MotionState m_swing_state;
  RotationalState m_tilt_state;
  PositionalState m_shake_state;
};
}  // namespace WiimoteEmu
