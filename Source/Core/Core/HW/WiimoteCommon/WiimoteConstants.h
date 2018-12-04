// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// Wiimote internal codes

// Communication channels
enum WiimoteChannel
{
  WC_OUTPUT = 0x11,
  WC_INPUT = 0x13
};

// The 4 most significant bits of the first byte of an outgoing command must be
// 0x50 if sending on the command channel and 0xA0 if sending on the interrupt
// channel. On Mac and Linux we use interrupt channel; on Windows, command.
enum WiimoteReport
{
#ifdef _WIN32
  WR_SET_REPORT = 0x50
#else
  WR_SET_REPORT = 0xA0
#endif
};

enum WiimoteBT
{
  BT_INPUT = 0x01,
  BT_OUTPUT = 0x02
};

// LED bit masks
enum WiimoteLED
{
  LED_NONE = 0x00,
  LED_1 = 0x10,
  LED_2 = 0x20,
  LED_3 = 0x40,
  LED_4 = 0x80
};

enum class WiimoteAddressSpace : u8
{
  EEPROM = 0x00,
  // 0x01 is never used but it does function on a real wiimote:
  I2C_BUS_ALT = 0x01,
  I2C_BUS = 0x02,
};

enum class WiimoteErrorCode : u8
{
  SUCCESS = 0,
  INVALID_SPACE = 6,
  NACK = 7,
  INVALID_ADDRESS = 8,
};

constexpr u8 MAX_PAYLOAD = 23;
constexpr u32 WIIMOTE_DEFAULT_TIMEOUT = 1000;
