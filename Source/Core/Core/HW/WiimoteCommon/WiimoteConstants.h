// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace WiimoteCommon
{
// Note this size includes the HID header.
// e.g. 0xa1 0x3d 0x...
// TODO: Kill/rename this constant so it's more clear.
constexpr u8 MAX_PAYLOAD = 23;

enum class InputReportID : u8
{
  Status = 0x20,
  ReadDataReply = 0x21,
  Ack = 0x22,

  // Not a real value on the wiimote, just a state to disable reports:
  ReportDisabled = 0x00,

  ReportCore = 0x30,
  ReportCoreAccel = 0x31,
  ReportCoreExt8 = 0x32,
  ReportCoreAccelIR12 = 0x33,
  ReportCoreExt19 = 0x34,
  ReportCoreAccelExt16 = 0x35,
  ReportCoreIR10Ext9 = 0x36,
  ReportCoreAccelIR10Ext6 = 0x37,

  ReportExt21 = 0x3d,
  ReportInterleave1 = 0x3e,
  ReportInterleave2 = 0x3f,
};

enum class OutputReportID : u8
{
  Rumble = 0x10,
  LED = 0x11,
  ReportMode = 0x12,
  IRLogicEnable = 0x13,
  SpeakerEnable = 0x14,
  RequestStatus = 0x15,
  WriteData = 0x16,
  ReadData = 0x17,
  SpeakerData = 0x18,
  SpeakerMute = 0x19,
  IRLogicEnable2 = 0x1a,
};

enum class IRReportFormat : u8
{
  None,
  Basic,     // from ReportCoreIR10Ext9 or ReportCoreAccelIR10Ext6
  Extended,  // from ReportCoreAccelIR12
  Full1,     // from ReportInterleave1
  Full2,     // from ReportInterleave2
};

enum class LED : u8
{
  None = 0x00,
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
  // I2CBusAlt is never used by games but it does function on a real wiimote.
  I2CBus = 0x01,
  I2CBusAlt = 0x02,
};

enum class ErrorCode : u8
{
  // Normal result.
  Success = 0,
  // Produced by read/write attempts during an active read.
  Busy = 4,
  // Produced by using something other than the above AddressSpace values.
  InvalidSpace = 6,
  // Produced by an i2c read/write with a non-responding slave address.
  Nack = 7,
  // Produced by accessing invalid regions of EEPROM or the EEPROM directly over i2c.
  InvalidAddress = 8,
};

}  // namespace WiimoteCommon
