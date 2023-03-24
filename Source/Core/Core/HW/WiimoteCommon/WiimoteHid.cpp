// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteCommon/WiimoteHid.h"

namespace WiimoteCommon
{
u32 GetExpectedInputReportSize(InputReportID report_id)
{
  switch (report_id)
  {
  case InputReportID::ReportDisabled:
    break;
  case InputReportID::Status:
    return sizeof(InputReportStatus);
  case InputReportID::ReadDataReply:
    return sizeof(InputReportReadDataReply);
  case InputReportID::Ack:
    return sizeof(InputReportAck);
  case InputReportID::ReportCore:
    return 2;
  case InputReportID::ReportCoreAccel:
    return 5;
  case InputReportID::ReportCoreExt8:
    return 10;
  case InputReportID::ReportCoreAccelIR12:
    return 17;
  case InputReportID::ReportCoreExt19:
  case InputReportID::ReportCoreAccelExt16:
  case InputReportID::ReportCoreIR10Ext9:
  case InputReportID::ReportCoreAccelIR10Ext6:
  case InputReportID::ReportExt21:
  case InputReportID::ReportInterleave1:
  case InputReportID::ReportInterleave2:
    return 21;
  }

  return 0;
}

}  // namespace WiimoteCommon
