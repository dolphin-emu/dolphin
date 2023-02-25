// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>

#include "Common/Matrix.h"
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

  static constexpr int LEFT_STICK_BITS = 6;
  static constexpr int RIGHT_STICK_BITS = 5;
  static constexpr int TRIGGER_BITS = 5;

  struct DataFormat
  {
    using StickType = Common::TVec2<u8>;
    using LeftStickRawValue = ControllerEmu::RawValue<StickType, LEFT_STICK_BITS>;
    using RightStickRawValue = ControllerEmu::RawValue<StickType, RIGHT_STICK_BITS>;

    using TriggerType = u8;
    using TriggerRawValue = ControllerEmu::RawValue<TriggerType, TRIGGER_BITS>;

    // 6-bit X and Y values (0-63)
    auto GetLeftStick() const { return LeftStickRawValue{StickType(lx, ly)}; };
    void SetLeftStick(const StickType& value)
    {
      lx = value.x;
      ly = value.y;
    }
    // 5-bit X and Y values (0-31)
    auto GetRightStick() const
    {
      return RightStickRawValue{StickType(rx1 | rx2 << 1 | rx3 << 3, ry)};
    };
    void SetRightStick(const StickType& value)
    {
      rx1 = value.x & 0b1;
      rx2 = (value.x >> 1) & 0b11;
      rx3 = (value.x >> 3) & 0b11;
      ry = value.y;
    }
    // 5-bit values (0-31)
    auto GetLeftTrigger() const { return TriggerRawValue(lt1 | lt2 << 3); }
    void SetLeftTrigger(TriggerType value)
    {
      lt1 = value & 0b111;
      lt2 = (value >> 3) & 0b11;
    }
    auto GetRightTrigger() const { return TriggerRawValue(rt); }
    void SetRightTrigger(TriggerType value) { rt = value; }

    u16 GetButtons() const
    {
      // 0 == pressed.
      return ~bt.hex;
    }

    void SetButtons(u16 value)
    {
      // 0 == pressed.
      bt.hex = ~value;
    }

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

  static constexpr int CAL_STICK_BITS = 8;
  static constexpr int CAL_TRIGGER_BITS = 8;

  struct CalibrationData
  {
    using StickType = DataFormat::StickType;
    using TriggerType = DataFormat::TriggerType;

    using StickCalibration = ControllerEmu::ThreePointCalibration<StickType, CAL_STICK_BITS>;
    using TriggerCalibration = ControllerEmu::TwoPointCalibration<TriggerType, CAL_TRIGGER_BITS>;

    static constexpr TriggerType TRIGGER_MAX = std::numeric_limits<TriggerType>::max();

    struct StickAxis
    {
      u8 max;
      u8 min;
      u8 center;
    };

    auto GetLeftStick() const
    {
      return StickCalibration{StickType{left_stick_x.min, left_stick_y.min},
                              StickType{left_stick_x.center, left_stick_y.center},
                              StickType{left_stick_x.max, left_stick_y.max}};
    }
    auto GetRightStick() const
    {
      return StickCalibration{StickType{right_stick_x.min, right_stick_y.min},
                              StickType{right_stick_x.center, right_stick_y.center},
                              StickType{right_stick_x.max, right_stick_y.max}};
    }
    auto GetLeftTrigger() const { return TriggerCalibration{left_trigger_zero, TRIGGER_MAX}; }
    auto GetRightTrigger() const { return TriggerCalibration{right_trigger_zero, TRIGGER_MAX}; }

    StickAxis left_stick_x;
    StickAxis left_stick_y;
    StickAxis right_stick_x;
    StickAxis right_stick_y;

    u8 left_trigger_zero;
    u8 right_trigger_zero;

    std::array<u8, 2> checksum;
  };
  static_assert(sizeof(CalibrationData) == 16, "Wrong size");

  Classic();

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(ClassicGroup group);

  static constexpr u16 PAD_RIGHT = 0x80;
  static constexpr u16 PAD_DOWN = 0x40;
  static constexpr u16 TRIGGER_L = 0x20;
  static constexpr u16 BUTTON_MINUS = 0x10;
  static constexpr u16 BUTTON_HOME = 0x08;
  static constexpr u16 BUTTON_PLUS = 0x04;
  static constexpr u16 TRIGGER_R = 0x02;
  static constexpr u16 NOTHING = 0x01;
  static constexpr u16 BUTTON_ZL = 0x8000;
  static constexpr u16 BUTTON_B = 0x4000;
  static constexpr u16 BUTTON_Y = 0x2000;
  static constexpr u16 BUTTON_A = 0x1000;
  static constexpr u16 BUTTON_X = 0x0800;
  static constexpr u16 BUTTON_ZR = 0x0400;
  static constexpr u16 PAD_LEFT = 0x0200;
  static constexpr u16 PAD_UP = 0x0100;

  // Typical value pulled from physical Classic Controller.
  static constexpr u8 STICK_GATE_RADIUS = 0x61;

  static constexpr u8 CAL_STICK_CENTER = 0x80;
  static constexpr u8 CAL_STICK_RADIUS = 0x7f;
  static constexpr u8 CAL_STICK_RANGE = 0xff;

  static constexpr u8 LEFT_STICK_CENTER = CAL_STICK_CENTER >> (CAL_STICK_BITS - LEFT_STICK_BITS);
  static constexpr u8 LEFT_STICK_RANGE = CAL_STICK_RANGE >> (CAL_STICK_BITS - LEFT_STICK_BITS);

  static constexpr u8 RIGHT_STICK_CENTER = CAL_STICK_CENTER >> (CAL_STICK_BITS - RIGHT_STICK_BITS);
  static constexpr u8 RIGHT_STICK_RANGE = CAL_STICK_RANGE >> (CAL_STICK_BITS - RIGHT_STICK_BITS);

  static constexpr u8 TRIGGER_RANGE = 0x1F;

  static constexpr const char* BUTTONS_GROUP = _trans("Buttons");
  static constexpr const char* LEFT_STICK_GROUP = _trans("Left Stick");
  static constexpr const char* RIGHT_STICK_GROUP = _trans("Right Stick");
  static constexpr const char* TRIGGERS_GROUP = _trans("Triggers");
  static constexpr const char* DPAD_GROUP = _trans("D-Pad");

  static constexpr const char* A_BUTTON = "A";
  static constexpr const char* B_BUTTON = "B";
  static constexpr const char* X_BUTTON = "X";
  static constexpr const char* Y_BUTTON = "Y";
  static constexpr const char* ZL_BUTTON = "ZL";
  static constexpr const char* ZR_BUTTON = "ZR";
  static constexpr const char* MINUS_BUTTON = "-";
  static constexpr const char* PLUS_BUTTON = "+";
  static constexpr const char* HOME_BUTTON = "Home";

  // i18n: The left trigger button (labeled L on real controllers)
  static constexpr const char* L_DIGITAL = _trans("L");
  // i18n: The right trigger button (labeled R on real controllers)
  static constexpr const char* R_DIGITAL = _trans("R");
  // i18n: The left trigger button (labeled L on real controllers) used as an analog input
  static constexpr const char* L_ANALOG = _trans("L-Analog");
  // i18n: The right trigger button (labeled R on real controllers) used as an analog input
  static constexpr const char* R_ANALOG = _trans("R-Analog");

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::MixedTriggers* m_triggers;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::AnalogStick* m_left_stick;
  ControllerEmu::AnalogStick* m_right_stick;
};
}  // namespace WiimoteEmu
