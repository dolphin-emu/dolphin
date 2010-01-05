// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _EMU_SUBFUNCTIONS_
#define _EMU_SUBFUNCTIONS_

#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>

#include "Common.h" // Common
#include "main.h"
#include "wiimote_hid.h" // Local
#include "EmuDefinitions.h"
#include "Encryption.h"

extern SWiimoteInitialize g_WiimoteInitialize;


namespace WiiMoteEmu
{

void HidOutputReport(u16 _channelID, wm_report* sr);

void WmReadData(u16 _channelID, wm_read_data* rd);
void WmWriteData(u16 _channelID, wm_write_data* wd);
void WmRequestStatus(u16 _channelID, wm_request_status* rs, int Extension = -1);
void WmRequestStatus_(u16 _channelID, int a);
void WmReportMode(u16 _channelID, wm_report_mode* dr);

void SendReportCoreAccel(u16 _channelID);
void SendReportCoreAccelIr12(u16 _channelID);
void SendReportCore(u16 _channelID);
void SendReportCoreAccelExt16(u16 _channelID);
void SendReportCoreAccelIr10Ext(u16 _channelID);

int WriteWmReportHdr(u8* dst, u8 wm);
void WmSendAck(u16 _channelID, u8 _reportID);
void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, int _Size);

void FillReportAcc(wm_accel& _acc);
void FillReportInfo(wm_core& _core);
void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1);
void FillReportIRBasic(wm_ir_basic& _ir0, wm_ir_basic& _ir1);
void FillReportExtension(wm_extension& _ext);
void FillReportClassicExtension(wm_classic_extension& _ext);
void FillReportGuitarHero3Extension(wm_GH3_extension& _ext);

} // namespace

#endif	//_EMU_DECLARATIONS_
