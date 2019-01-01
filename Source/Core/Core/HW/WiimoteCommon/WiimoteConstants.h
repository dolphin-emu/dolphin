// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace WiimoteCommon
{
constexpr u8 MAX_PAYLOAD = 23;

enum class InputReportID : u8
{
  STATUS = 0x20,
  READ_DATA_REPLY = 0x21,
  ACK = 0x22,

  // Not a real value on the wiimote, just a state to disable reports:
  REPORT_DISABLED = 0x00,

  REPORT_CORE = 0x30,
  REPORT_CORE_ACCEL = 0x31,
  REPORT_CORE_EXT8 = 0x32,
  REPORT_CORE_ACCEL_IR12 = 0x33,
  REPORT_CORE_EXT19 = 0x34,
  REPORT_CORE_ACCEL_EXT16 = 0x35,
  REPORT_CORE_IR10_EXT9 = 0x36,
  REPORT_CORE_ACCEL_IR10_EXT6 = 0x37,

  REPORT_EXT21 = 0x3d,
  REPORT_INTERLEAVE1 = 0x3e,
  REPORT_INTERLEAVE2 = 0x3f,
};

enum class OutputReportID : u8
{
  RUMBLE = 0x10,
  LEDS = 0x11,
  REPORT_MODE = 0x12,
  IR_PIXEL_CLOCK = 0x13,
  SPEAKER_ENABLE = 0x14,
  REQUEST_STATUS = 0x15,
  WRITE_DATA = 0x16,
  READ_DATA = 0x17,
  SPEAKER_DATA = 0x18,
  SPEAKER_MUTE = 0x19,
  IR_LOGIC = 0x1A,
};

enum class LED : u8
{
  NONE = 0x00,
  LED_1 = 0x10,
  LED_2 = 0x20,
  LED_3 = 0x40,
  LED_4 = 0x80
};

enum class AddressSpace : u8
{
  // FYI: The EEPROM address space is offset 0x0070 on i2c slave 0x50.
  // However attempting to access this device directly results in an error.
  EEPROM = 0x00,
  // 0x01 is never used but it does function on a real wiimote:
  I2C_BUS_ALT = 0x01,
  I2C_BUS = 0x02,
};

enum class ErrorCode : u8
{
  SUCCESS = 0,
  INVALID_SPACE = 6,
  NACK = 7,
  INVALID_ADDRESS = 8,

  // Not a real value:
  DO_NOT_SEND_ACK = 0xff,
};

}  // namespace WiimoteCommon
