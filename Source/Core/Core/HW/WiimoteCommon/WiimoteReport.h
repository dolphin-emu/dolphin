// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"

#ifdef _MSC_VER
#pragma warning(push)
// Disable warning for zero-sized array:
#pragma warning(disable : 4200)
#endif

namespace WiimoteCommon
{
#pragma pack(push, 1)
struct OutputReportGeneric
{
  OutputReportID rpt_id;

  static constexpr int HEADER_SIZE = sizeof(OutputReportID);

  union
  {
    u8 data[0];
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

struct OutputReportLeds
{
  u8 rumble : 1;
  u8 ack : 1;
  u8 : 2;
  u8 leds : 4;
};
static_assert(sizeof(OutputReportLeds) == 1, "Wrong size");

struct OutputReportMode
{
  u8 rumble : 1;
  u8 ack : 1;
  u8 continuous : 1;
  u8 : 5;
  InputReportID mode;
};
static_assert(sizeof(OutputReportMode) == 2, "Wrong size");

struct OutputReportRequestStatus
{
  u8 rumble : 1;
  u8 : 7;
};
static_assert(sizeof(OutputReportRequestStatus) == 1, "Wrong size");

struct OutputReportWriteData
{
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
  u8 rumble : 1;
  u8 : 2;
  u8 length : 5;
  u8 data[20];
};
static_assert(sizeof(OutputReportSpeakerData) == 21, "Wrong size");

// FYI: Also contains LSB of accel data:
union ButtonData
{
  static constexpr u16 BUTTON_MASK = ~0x6060;

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
  ButtonData buttons;
  u8 battery_low : 1;
  u8 extension : 1;
  u8 speaker : 1;
  u8 ir : 1;
  u8 leds : 4;
  u8 padding2[2];
  u8 battery;
};
static_assert(sizeof(InputReportStatus) == 6, "Wrong size");

struct InputReportAck
{
  ButtonData buttons;
  OutputReportID rpt_id;
  ErrorCode error_code;
};
static_assert(sizeof(InputReportAck) == 4, "Wrong size");

struct InputReportReadDataReply
{
  ButtonData buttons;
  u8 error : 4;
  u8 size_minus_one : 4;
  // big endian:
  u16 address;
  u8 data[16];
};
static_assert(sizeof(InputReportReadDataReply) == 21, "Wrong size");

}  // namespace WiimoteCommon

#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
