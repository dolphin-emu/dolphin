// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace WiimoteCommon
{
#pragma pack(push, 1)
struct OutputReportGeneric
{
  OutputReportID rpt_id;

  static constexpr int HEADER_SIZE = sizeof(OutputReportID);

  union
  {
    // Actual size varies
    u8 data[1];
    struct
    {
      // Enable/disable rumble. (Valid for ALL output reports)
      u8 rumble : 1;
    };
  };
};
static_assert(sizeof(OutputReportGeneric) == 2, "Wrong size");

// TODO: The following structs don't have the report_id header byte.
// This is fine but the naming conventions are poor.

struct OutputReportRumble
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::Rumble;

  u8 rumble : 1;
};
static_assert(sizeof(OutputReportRumble) == 1, "Wrong size");

struct OutputReportEnableFeature
{
  u8 rumble : 1;
  // Respond with an ack.
  u8 ack : 1;
  // Enable/disable certain feature.
  u8 enable : 1;
};
static_assert(sizeof(OutputReportEnableFeature) == 1, "Wrong size");

struct OutputReportIRLogicEnable : OutputReportEnableFeature
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::IRLogicEnable;
};
static_assert(sizeof(OutputReportIRLogicEnable) == 1, "Wrong size");

struct OutputReportIRLogicEnable2 : OutputReportEnableFeature
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::IRLogicEnable2;
};
static_assert(sizeof(OutputReportIRLogicEnable2) == 1, "Wrong size");

struct OutputReportSpeakerEnable : OutputReportEnableFeature
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::SpeakerEnable;
};
static_assert(sizeof(OutputReportSpeakerEnable) == 1, "Wrong size");

struct OutputReportSpeakerMute : OutputReportEnableFeature
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::SpeakerMute;
};
static_assert(sizeof(OutputReportSpeakerMute) == 1, "Wrong size");

struct OutputReportLeds
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::LED;

  u8 rumble : 1;
  u8 ack : 1;
  u8 : 2;
  u8 leds : 4;
};
static_assert(sizeof(OutputReportLeds) == 1, "Wrong size");

struct OutputReportMode
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::ReportMode;

  u8 rumble : 1;
  u8 ack : 1;
  u8 continuous : 1;
  u8 : 5;
  InputReportID mode;
};
static_assert(sizeof(OutputReportMode) == 2, "Wrong size");

struct OutputReportRequestStatus
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::RequestStatus;

  u8 rumble : 1;
  u8 : 7;
};
static_assert(sizeof(OutputReportRequestStatus) == 1, "Wrong size");

struct OutputReportWriteData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::WriteData;

  u8 rumble : 1;
  u8 : 1;
  u8 space : 2;
  u8 : 4;
  // A real wiimote ignores the i2c read/write bit.
  u8 i2c_rw_ignored : 1;
  // Used only for register space (i2c bus) (7-bits):
  u8 slave_address : 7;
  // big endian:
  u8 address[2];
  u8 size;
  u8 data[16];
};
static_assert(sizeof(OutputReportWriteData) == 21, "Wrong size");

struct OutputReportReadData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::ReadData;

  u8 rumble : 1;
  u8 : 1;
  u8 space : 2;
  u8 : 4;
  // A real wiimote ignores the i2c read/write bit.
  u8 i2c_rw_ignored : 1;
  // Used only for register space (i2c bus) (7-bits):
  u8 slave_address : 7;
  // big endian:
  u8 address[2];
  u8 size[2];
};
static_assert(sizeof(OutputReportReadData) == 6, "Wrong size");

struct OutputReportSpeakerData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::SpeakerData;

  u8 rumble : 1;
  u8 : 2;
  u8 length : 5;
  u8 data[20];
};
static_assert(sizeof(OutputReportSpeakerData) == 21, "Wrong size");

// FYI: Also contains LSB of accel data:
union ButtonData
{
  static constexpr u16 BUTTON_MASK = ~0x60e0;

  u16 hex;

  struct
  {
    u8 left : 1;
    u8 right : 1;
    u8 down : 1;
    u8 up : 1;
    u8 plus : 1;
    // For most input reports this is the 2 LSbs of accel.x:
    // For interleaved reports this is alternating bits of accel.z:
    u8 acc_bits : 2;
    u8 unknown : 1;

    u8 two : 1;
    u8 one : 1;
    u8 b : 1;
    u8 a : 1;
    u8 minus : 1;
    // For most input reports this is bits of accel.y/z:
    // For interleaved reports this is alternating bits of accel.z:
    u8 acc_bits2 : 2;
    u8 home : 1;
  };
};
static_assert(sizeof(ButtonData) == 2, "Wrong size");

struct InputReportStatus
{
  static constexpr InputReportID REPORT_ID = InputReportID::Status;

  ButtonData buttons;
  u8 battery_low : 1;
  u8 extension : 1;
  u8 speaker : 1;
  u8 ir : 1;
  u8 leds : 4;
  u8 padding2[2];
  u8 battery;

  constexpr float GetEstimatedCharge() const
  {
    return battery * BATTERY_LEVEL_M / BATTERY_MAX + BATTERY_LEVEL_B;
  }
  void SetEstimatedCharge(float charge)
  {
    battery = u8(std::lround((charge - BATTERY_LEVEL_B) / BATTERY_LEVEL_M * BATTERY_MAX));
  }

private:
  static constexpr auto BATTERY_MAX = std::numeric_limits<decltype(battery)>::max();

  // Linear fit of battery level mid-point for charge bars in home menu.
  static constexpr float BATTERY_LEVEL_M = 2.46f;
  static constexpr float BATTERY_LEVEL_B = -0.013f;
};
static_assert(sizeof(InputReportStatus) == 6, "Wrong size");

struct InputReportAck
{
  static constexpr InputReportID REPORT_ID = InputReportID::Ack;

  ButtonData buttons;
  OutputReportID rpt_id;
  ErrorCode error_code;
};
static_assert(sizeof(InputReportAck) == 4, "Wrong size");

struct InputReportReadDataReply
{
  static constexpr InputReportID REPORT_ID = InputReportID::ReadDataReply;

  ButtonData buttons;
  u8 error : 4;
  u8 size_minus_one : 4;
  // big endian:
  u16 address;
  u8 data[16];
};
static_assert(sizeof(InputReportReadDataReply) == 21, "Wrong size");

// Accel data handled as if there were always 10 bits of precision.
using AccelType = Common::TVec3<u16>;
using AccelData = ControllerEmu::RawValue<AccelType, 10>;

// Found in Wiimote EEPROM and Nunchuk "register".
// 0g and 1g points exist.
struct AccelCalibrationPoint
{
  // All components have 10 bits of precision.
  u16 GetX() const { return x2 << 2 | x1; }
  u16 GetY() const { return y2 << 2 | y1; }
  u16 GetZ() const { return z2 << 2 | z1; }
  auto Get() const { return AccelType{GetX(), GetY(), GetZ()}; }

  void SetX(u16 x)
  {
    x2 = x >> 2;
    x1 = x;
  }
  void SetY(u16 y)
  {
    y2 = y >> 2;
    y1 = y;
  }
  void SetZ(u16 z)
  {
    z2 = z >> 2;
    z1 = z;
  }
  void Set(AccelType accel)
  {
    SetX(accel.x);
    SetY(accel.y);
    SetZ(accel.z);
  }

  u8 x2, y2, z2;
  u8 z1 : 2;
  u8 y1 : 2;
  u8 x1 : 2;
  u8 : 2;
};

// Located at 0x16 and 0x20 of Wii Remote EEPROM.
struct AccelCalibrationData
{
  using Calibration = ControllerEmu::TwoPointCalibration<AccelType, 10>;

  auto GetCalibration() const { return Calibration(zero_g.Get(), one_g.Get()); }

  AccelCalibrationPoint zero_g;
  AccelCalibrationPoint one_g;

  u8 volume : 7;
  u8 motor : 1;
  u8 checksum;
};

}  // namespace WiimoteCommon

#pragma pack(pop)
