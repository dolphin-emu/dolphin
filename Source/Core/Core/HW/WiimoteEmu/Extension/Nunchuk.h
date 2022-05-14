// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/BitField.h"
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
  Shake,
  IMUAccelerometer,
};

class Nunchuk : public Extension1stParty
{
public:
  union ButtonFormat
  {
    u8 hex;

    BitField<0, 1, bool, u8> z;
    BitField<1, 1, bool, u8> c;

    // LSBs of accelerometer
    BitField<2, 2, u8> acc_x_lsb;
    BitField<4, 2, u8> acc_y_lsb;
    BitField<6, 2, u8> acc_z_lsb;
  };
  static_assert(sizeof(ButtonFormat) == 1, "Wrong size");

  struct DataFormat
  {
    using StickType = Common::TVec2<u8>;
    using StickRawValue = ControllerEmu::RawValue<StickType, 8>;

    using AccelType = WiimoteCommon::AccelType;
    using AccelData = WiimoteCommon::AccelData;

    auto GetStick() const { return StickRawValue(StickType(jx, jy)); }

    // Components have 10 bits of precision.
    u16 GetAccelX() const { return ax << 2 | bt.acc_x_lsb; }
    u16 GetAccelY() const { return ay << 2 | bt.acc_y_lsb; }
    u16 GetAccelZ() const { return az << 2 | bt.acc_z_lsb; }
    auto GetAccel() const { return AccelData{AccelType{GetAccelX(), GetAccelY(), GetAccelZ()}}; }

    void SetAccelX(u16 val)
    {
      ax = val >> 2;
      bt.acc_x_lsb = val & 0b11;
    }
    void SetAccelY(u16 val)
    {
      ay = val >> 2;
      bt.acc_y_lsb = val & 0b11;
    }
    void SetAccelZ(u16 val)
    {
      az = val >> 2;
      bt.acc_z_lsb = val & 0b11;
    }
    void SetAccel(const AccelType& accel)
    {
      SetAccelX(accel.x);
      SetAccelY(accel.y);
      SetAccelZ(accel.z);
    }

    u8 GetButtons() const
    {
      // 0 == pressed.
      return ~bt.hex & (BUTTON_C | BUTTON_Z);
    }
    void SetButtons(u8 value)
    {
      // 0 == pressed.
      bt.hex |= (BUTTON_C | BUTTON_Z);
      bt.hex ^= value & (BUTTON_C | BUTTON_Z);
    }

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
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  struct CalibrationData
  {
    using StickType = DataFormat::StickType;
    using StickCalibration = ControllerEmu::ThreePointCalibration<StickType, 8>;

    using AccelType = WiimoteCommon::AccelType;
    using AccelCalibration = ControllerEmu::TwoPointCalibration<AccelType, 10>;

    struct Stick
    {
      u8 max;
      u8 min;
      u8 center;
    };

    auto GetStick() const
    {
      return StickCalibration(StickType{stick_x.min, stick_y.min},
                              StickType{stick_x.center, stick_y.center},
                              StickType{stick_x.max, stick_y.max});
    }
    auto GetAccel() const { return AccelCalibration(accel_zero_g.Get(), accel_one_g.Get()); }

    WiimoteCommon::AccelCalibrationPoint accel_zero_g;
    WiimoteCommon::AccelCalibrationPoint accel_one_g;

    Stick stick_x;
    Stick stick_y;

    std::array<u8, 2> checksum;
  };
  static_assert(sizeof(CalibrationData) == 16, "Wrong size");

  Nunchuk();

  void Update() override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ControllerEmu::ControlGroup* GetGroup(NunchukGroup group);

  static constexpr u8 BUTTON_C = 0x02;
  static constexpr u8 BUTTON_Z = 0x01;

  // Typical values pulled from physical Nunchuk.
  static constexpr u8 ACCEL_ZERO_G = 0x80;
  static constexpr u8 ACCEL_ONE_G = 0xB3;
  static constexpr u8 STICK_GATE_RADIUS = 0x60;

  static constexpr u8 STICK_CENTER = 0x80;
  static constexpr u8 STICK_RADIUS = 0x7F;

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::Shake* m_shake;
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::IMUAccelerometer* m_imu_accelerometer;

  // Dynamics:
  MotionState m_swing_state;
  RotationalState m_tilt_state;
  PositionalState m_shake_state;
};
}  // namespace WiimoteEmu
