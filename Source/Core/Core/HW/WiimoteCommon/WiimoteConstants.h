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
  // 0x01 is never used but it does function on a real wiimote:
  I2CBusAlt = 0x01,
  I2CBus = 0x02,
};

enum class ErrorCode : u8
{
  Success = 0,
  InvalidSpace = 6,
  Nack = 7,
  InvalidAddress = 8,
};

}  // namespace WiimoteCommon
