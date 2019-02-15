// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
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

class Nunchuk : public EncryptedExtension
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

  ControllerEmu::ControlGroup* GetGroup(NunchukGroup group);

  enum
  {
    BUTTON_C = 0x02,
    BUTTON_Z = 0x01,
  };

  enum
  {
    ACCEL_ZERO_G = 0x80,
    ACCEL_ONE_G = 0xB3,
  };

  enum
  {
    STICK_CENTER = 0x80,
    STICK_RADIUS = 0x7F,
    STICK_GATE_RADIUS = 0x52,
  };

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Tilt* m_tilt;

  ControllerEmu::Force* m_swing;
  ControllerEmu::Force* m_swing_slow;
  ControllerEmu::Force* m_swing_fast;

  ControllerEmu::Buttons* m_shake;
  ControllerEmu::Buttons* m_shake_soft;
  ControllerEmu::Buttons* m_shake_hard;

  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;

  std::array<u8, 3> m_shake_step{};
  std::array<u8, 3> m_shake_soft_step{};
  std::array<u8, 3> m_shake_hard_step{};
};
}  // namespace WiimoteEmu
