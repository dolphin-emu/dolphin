// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/BitFieldView.h"
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

  // Actual size varies
  u8 rpt_stub;

  // Enable/disable rumble. (Valid for ALL output reports)
  BFVIEW_M(rpt_stub, bool, 0, 1, rumble);
};
static_assert(sizeof(OutputReportGeneric) == 2, "Wrong size");

// TODO: The following structs don't have the report_id header byte.
// This is fine but the naming conventions are poor.

struct OutputReportRumble
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::Rumble;

  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
};
static_assert(sizeof(OutputReportRumble) == 1, "Wrong size");

struct OutputReportEnableFeature
{
  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  // Respond with an ack.
  BFVIEW_M(_rpt, bool, 1, 1, ack);
  // Enable/disable certain feature.
  BFVIEW_M(_rpt, bool, 2, 1, enable);
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

  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  BFVIEW_M(_rpt, bool, 1, 1, ack);
  BFVIEW_M(_rpt, u8, 4, 4, leds);
};
static_assert(sizeof(OutputReportLeds) == 1, "Wrong size");

struct OutputReportMode
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::ReportMode;

  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  BFVIEW_M(_rpt, bool, 1, 1, ack);
  BFVIEW_M(_rpt, bool, 2, 1, continuous);

  InputReportID mode;
};
static_assert(sizeof(OutputReportMode) == 2, "Wrong size");

struct OutputReportRequestStatus
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::RequestStatus;

  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
};
static_assert(sizeof(OutputReportRequestStatus) == 1, "Wrong size");

struct OutputReportWriteData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::WriteData;

  u16 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  BFVIEW_M(_rpt, AddressSpace, 2, 2, space);
  // A real wiimote ignores the i2c read/write bit.
  BFVIEW_M(_rpt, bool, 0, 1, i2c_rw_ignored);
  // Used only for register space (i2c bus) (7-bits):
  BFVIEW_M(_rpt, u8, 1, 7, slave_address);

  // big endian:
  u8 address[2];
  u8 size;
  u8 data[16];
};
static_assert(sizeof(OutputReportWriteData) == 21, "Wrong size");

struct OutputReportReadData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::ReadData;

  u16 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  BFVIEW_M(_rpt, AddressSpace, 2, 2, space);
  // A real wiimote ignores the i2c read/write bit.
  BFVIEW_M(_rpt, bool, 0, 1, i2c_rw_ignored);
  // Used only for register space (i2c bus) (7-bits):
  BFVIEW_M(_rpt, u8, 1, 7, slave_address);

  // big endian:
  u8 address[2];
  u8 size[2];
};
static_assert(sizeof(OutputReportReadData) == 6, "Wrong size");

struct OutputReportSpeakerData
{
  static constexpr OutputReportID REPORT_ID = OutputReportID::SpeakerData;

  u8 _rpt;

  BFVIEW_M(_rpt, bool, 0, 1, rumble);
  BFVIEW_M(_rpt, u8, 3, 5, length);

  u8 data[20];
};
static_assert(sizeof(OutputReportSpeakerData) == 21, "Wrong size");

// FYI: Also contains LSB of accel data:
struct ButtonData
{
  static constexpr u16 BUTTON_MASK = ~0x60e0;

  u16 hex;

  BFVIEW_M(hex, bool, 0, 1, left);
  BFVIEW_M(hex, bool, 1, 1, right);
  BFVIEW_M(hex, bool, 2, 1, down);
  BFVIEW_M(hex, bool, 3, 1, up);
  BFVIEW_M(hex, bool, 4, 1, plus);
  // For most input reports this is the 2 LSbs of accel.x:
  // For interleaved reports this is alternating bits of accel.z:
  BFVIEW_M(hex, u16, 5, 2, acc_bits);
  BFVIEW_M(hex, u16, 7, 1, unknown);

  BFVIEW_M(hex, bool, 8, 1, two);
  BFVIEW_M(hex, bool, 9, 1, one);
  BFVIEW_M(hex, bool, 10, 1, b);
  BFVIEW_M(hex, bool, 11, 1, a);
  BFVIEW_M(hex, bool, 12, 1, minus);
  // For most input reports this is bits of accel.y/z:
  // For interleaved reports this is alternating bits of accel.z:
  BFVIEW_M(hex, u16, 13, 2, acc_bits2);
  BFVIEW_M(hex, bool, 15, 1, home);
};
static_assert(sizeof(ButtonData) == 2, "Wrong size");

struct InputReportStatus
{
  static constexpr InputReportID REPORT_ID = InputReportID::Status;

  ButtonData buttons;
  u8 _status;

  BFVIEW_M(_status, bool, 0, 1, battery_low);
  BFVIEW_M(_status, bool, 1, 1, extension);
  BFVIEW_M(_status, bool, 2, 1, speaker);
  BFVIEW_M(_status, bool, 3, 1, ir);
  BFVIEW_M(_status, u8, 4, 4, leds);

  u16 : 16;  // padding
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
  u8 _reply;

  BFVIEW_M(_reply, ErrorCode, 0, 4, error);
  BFVIEW_M(_reply, u8, 4, 4, size_minus_one);

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
  u16 GetX() const { return x2 << 2 | x1(); }
  u16 GetY() const { return y2 << 2 | y1(); }
  u16 GetZ() const { return z2 << 2 | z1(); }
  auto Get() const { return AccelType{GetX(), GetY(), GetZ()}; }

  void SetX(u16 x)
  {
    x2 = x >> 2;
    x1() = x;
  }
  void SetY(u16 y)
  {
    y2 = y >> 2;
    y1() = y;
  }
  void SetZ(u16 z)
  {
    z2 = z >> 2;
    z1() = z;
  }
  void Set(AccelType accel)
  {
    SetX(accel.x);
    SetY(accel.y);
    SetZ(accel.z);
  }

  u8 x2, y2, z2;
  u8 _x1y1z1;

  BFVIEW_M(_x1y1z1, u8, 0, 2, z1);
  BFVIEW_M(_x1y1z1, u8, 2, 2, y1);
  BFVIEW_M(_x1y1z1, u8, 4, 2, x1);
};

// Located at 0x16 and 0x20 of Wii Remote EEPROM.
struct AccelCalibrationData
{
  using Calibration = ControllerEmu::TwoPointCalibration<AccelType, 10>;

  auto GetCalibration() const { return Calibration(zero_g.Get(), one_g.Get()); }

  AccelCalibrationPoint zero_g;
  AccelCalibrationPoint one_g;

  u8 _data1;
  u8 checksum;

  BFVIEW_M(_data1, u8, 0, 7, volume);
  BFVIEW_M(_data1, bool, 7, 1, motor);
};

}  // namespace WiimoteCommon

#pragma pack(pop)
