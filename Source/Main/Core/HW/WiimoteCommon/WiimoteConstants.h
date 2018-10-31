// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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

enum WiimoteSpace
{
  WS_EEPROM = 0x00,
  WS_REGS1 = 0x01,
  WS_REGS2 = 0x02
};

enum WiimoteReadError
{
  RDERR_WOREG = 7,
  RDERR_NOMEM = 8
};

constexpr u8 MAX_PAYLOAD = 23;
constexpr u32 WIIMOTE_DEFAULT_TIMEOUT = 1000;
