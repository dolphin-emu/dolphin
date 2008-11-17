// Copyright (C) 2003-2008 Dolphin Project.

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


#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>
#include "Common.h"
#include "wiimote_hid.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd
#include "Config.h" // for g_Config

extern SWiimoteInitialize g_WiimoteInitialize;
//extern void __Log(int log, const char *format, ...);
//extern void __Log(int log, int v, const char *format, ...);

namespace WiiMoteEmu
{


//******************************************************************************
// Subroutines
//******************************************************************************



void HidOutputReport(u16 _channelID, wm_report* sr) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");

	LOGV(WII_IPC_WIIMOTE, 0, "HidOutputReport(0x%02x)", sr->channel);

	switch(sr->channel)
	{
	case 0x10:
		LOGV(WII_IPC_WIIMOTE, 0, "HidOutputReport: unknown sr->channel 0x10");
		break;
	case WM_LEDS:
		WmLeds(_channelID, (wm_leds*)sr->data);
		break;
	case WM_READ_DATA:
		WmReadData(_channelID, (wm_read_data*)sr->data);
		break;
	case WM_REQUEST_STATUS:
		WmRequestStatus(_channelID, (wm_request_status*)sr->data);
		break;
	case WM_IR_PIXEL_CLOCK:
	case WM_IR_LOGIC:
		LOGV(WII_IPC_WIIMOTE, 0, "  IR Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	case WM_WRITE_DATA:
		WmWriteData(_channelID, (wm_write_data*)sr->data);
		break;
	case WM_DATA_REPORTING:
		WmDataReporting(_channelID, (wm_data_reporting*)sr->data);
		break;

	case WM_SPEAKER_ENABLE:
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Speaker Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;

	case WM_SPEAKER_MUTE:
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Mute Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->channel);
		return;
	}
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}

void WmLeds(u16 _channelID, wm_leds* leds) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");

	LOG(WII_IPC_WIIMOTE, " Set LEDs");
	LOG(WII_IPC_WIIMOTE, "  Leds: %x", leds->leds);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", leds->rumble);

	g_Leds = leds->leds;
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}
void WmSendAck(u16 _channelID, u8 _reportID)
{
	u8 DataFrame[1024];

	u32 Offset = 0;

	// header
	hid_packet* pHidHeader = (hid_packet*)(DataFrame + Offset);
	pHidHeader->type = HID_TYPE_DATA;
	pHidHeader->param = HID_PARAM_INPUT;
	Offset += sizeof(hid_packet);

	wm_acknowledge* pData = (wm_acknowledge*)(DataFrame + Offset);
	pData->Channel = WM_WRITE_DATA_REPLY;
	pData->unk0 = 0;
	pData->unk1 = 0;
	pData->reportID = _reportID;
	pData->errorID = 0;
	Offset += sizeof(wm_acknowledge);


	LOGV(WII_IPC_WIIMOTE, 2, "  WMSendAck()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


void WmDataReporting(u16 _channelID, wm_data_reporting* dr) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
	LOG(WII_IPC_WIIMOTE, " Set Data reporting mode");
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", dr->rumble);
	LOG(WII_IPC_WIIMOTE, "  Continuous: %x", dr->continuous);
	LOG(WII_IPC_WIIMOTE, "  All The Time: %x (not only on data change)", dr->all_the_time);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", dr->rumble);
	LOG(WII_IPC_WIIMOTE, "  Mode: 0x%02x", dr->mode);

	g_ReportingMode = dr->mode;
	g_ReportingChannel = _channelID;
	switch(dr->mode) {	//see Wiimote_Update()
	case WM_REPORT_CORE:
	case WM_REPORT_CORE_ACCEL:
	case WM_REPORT_CORE_ACCEL_IR12:
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:
		break;
	default:
		PanicAlert("Wiimote: Unknown reporting mode 0x%x", dr->mode);
	}

	// WmSendAck(_channelID, WM_DATA_REPORTING);

	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}


void SendReportCore(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE);

	wm_report_core* pReport = (wm_report_core*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core);
	memset(pReport, 0, sizeof(wm_report_core));

	FillReportInfo(pReport->c);

	LOG(WII_IPC_WIIMOTE, "  SendReportCore()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


void SendReportCoreAccelIr12(u16 _channelID) {
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_IR12);

	wm_report_core_accel_ir12* pReport = (wm_report_core_accel_ir12*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ir12);
	memset(pReport, 0, sizeof(wm_report_core_accel_ir12));
	
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);
	FillReportIR(pReport->ir[0], pReport->ir[1]);

	LOGV(WII_IPC_WIIMOTE, 2, "  SendReportCoreAccelIr12()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}

void SendReportCoreAccelIr10Ext(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_IR10_EXT6);

	wm_report_core_accel_ir10_ext6* pReport = (wm_report_core_accel_ir10_ext6*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ir10_ext6);
	memset(pReport, 0, sizeof(wm_report_core_accel_ir10_ext6));

	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);
	FillReportIRBasic(pReport->ir[0], pReport->ir[1]);

	LOG(WII_IPC_WIIMOTE, "  SendReportCoreAccelIr10Ext()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


void SendReportCoreAccel(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL);

	wm_report_core_accel* pReport = (wm_report_core_accel*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel);
	memset(pReport, 0, sizeof(wm_report_core_accel));
	
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);

	LOG(WII_IPC_WIIMOTE, "  SendReportCoreAccel()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


void WmReadData(u16 _channelID, wm_read_data* rd) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
	u32 address = convert24bit(rd->address);
	u16 size = convert16bit(rd->size);
	LOG(WII_IPC_WIIMOTE, " Read data");
	LOG(WII_IPC_WIIMOTE, "  Address space: %x", rd->space);
	LOG(WII_IPC_WIIMOTE, "  Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "  Size: 0x%04x", size);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", rd->rumble);

	if(rd->space == 0) 
	{
		if (address + size > WIIMOTE_EEPROM_SIZE) 
		{
			PanicAlert("WmReadData: address + size out of bounds!");
			return;
		}
		SendReadDataReply(_channelID, g_Eeprom+address, address, (u8)size);
	} 
	else if(rd->space == WM_SPACE_REGS1 || rd->space == WM_SPACE_REGS2)
	{
		u8* block;
		u32 blockSize;
		switch((address >> 16) & 0xFE) 
		{
/*		case 0xA2:
			block = g_RegSpeaker;
			blockSize = WIIMOTE_REG_SPEAKER_SIZE;
			break;*/
		case 0xA4:
			block = g_RegExt;
			blockSize = WIIMOTE_REG_EXT_SIZE;
			//PanicAlert("WmReadData: 0xA4 g_RegExt");
			LOGV(WII_IPC_WIIMOTE, 0, "WmReadData: 0xA4 g_RegExt");
			break;
/*	case 0xB0:
			block = g_RegIr;
			blockSize = WIIMOTE_REG_IR_SIZE;
			//PanicAlert("WmReadData: 0xB0 g_RegIr");
			LOGV(WII_IPC_WIIMOTE, 0, "WmReadData: 0xB0 g_RegIr");
			break;*/
		default:
			PanicAlert("WmWriteData: bad register block!");
			return;
		}
		address &= 0xFFFF;
		if(address + size > blockSize) {
			PanicAlert("WmReadData: address + size out of bounds!");
			return;
		}

		SendReadDataReply(_channelID, block+address, address, (u8)size);
	} 
	else
	{
		PanicAlert("WmReadData: unimplemented parameters (size: %i, addr: 0x%x!", size, rd->space);
	}
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}


void WmWriteData(u16 _channelID, wm_write_data* wd) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "========================================================");
	u32 address = convert24bit(wd->address);
	LOG(WII_IPC_WIIMOTE, " Write data");
	LOG(WII_IPC_WIIMOTE, "  Address space: %x", wd->space);
	LOG(WII_IPC_WIIMOTE, "  Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "  Size: 0x%02x", wd->size);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", wd->rumble);

	// Write to EEPROM
	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom + address, wd->data, wd->size);

	//	WmSendAck(_channelID, WM_WRITE_DATA);
	}
	// Write to registers
	else if(wd->size <= 16 && (wd->space == WM_SPACE_REGS1 || wd->space == WM_SPACE_REGS2))
	{
		u8* block;
		u32 blockSize;
		switch((address >> 16) & 0xFE)
		{
			case 0xA2:
				block = g_RegSpeaker;
				blockSize = WIIMOTE_REG_SPEAKER_SIZE;
				break;
			case 0xA4:
				block = g_RegExt; // Extension Controller register
				blockSize = WIIMOTE_REG_EXT_SIZE;
				LOGV(WII_IPC_WIIMOTE, 0, "  Case 0xA4: Write g_RegExt *********************************");
				PanicAlert("  Case 0xA4: Write g_RegExt");
				break;
			case 0xB0:
				block = g_RegIr;
				blockSize = WIIMOTE_REG_IR_SIZE;
				break;
			default:
				PanicAlert("WmWriteData: bad register block!");
				return;
		}
		address &= 0xFFFF;
		if(address + wd->size > blockSize) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		// finally write the registers to memory
		memcpy(wd->data, block + address, wd->size);

	} else {
		PanicAlert("WmWriteData: unimplemented parameters!");
	}

	// just added for home brew.... hmmmm
	WmSendAck(_channelID, WM_WRITE_DATA);
	LOGV(WII_IPC_WIIMOTE, 0, "==========================================================");
}

int WriteWmReport(u8* dst, u8 channel) {
	u32 Offset = 0;
	hid_packet* pHidHeader = (hid_packet*)(dst + Offset);
	Offset += sizeof(hid_packet);
	pHidHeader->type = HID_TYPE_DATA;
	pHidHeader->param = HID_PARAM_INPUT;

	wm_report* pReport = (wm_report*)(dst + Offset);
	Offset += sizeof(wm_report);
	pReport->channel = channel;
	return Offset;
}

void WmRequestStatus(u16 _channelID, wm_request_status* rs)
{
	//PanicAlert("WmRequestStatus");
	LOGV(WII_IPC_WIIMOTE, 0, "================================================");
	LOGV(WII_IPC_WIIMOTE, 0, " Request Status");
	LOGV(WII_IPC_WIIMOTE, 0, "  Rumble: %x", rs->rumble);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report));
	pStatus->leds = g_Leds;
	pStatus->ir = 1;
	pStatus->battery = 0x4F;	//arbitrary number

	// this gets us passed the first error, but later brings up the disconnected error
	if(g_Config.bExtensionConnected)
		pStatus->extension = 1;
	else
		pStatus->extension = 0;

	LOGV(WII_IPC_WIIMOTE, 0, "  Extension: %x", pStatus->extension);
	LOGV(WII_IPC_WIIMOTE, 0, "  SendStatusReport()");
	LOGV(WII_IPC_WIIMOTE, 0, "    Flags: 0x%02x", pStatus->padding1[2]);
	LOGV(WII_IPC_WIIMOTE, 0, "    Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
	LOGV(WII_IPC_WIIMOTE, 0, "=================================================");
}

void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, u8 _Size)
{
	LOGV(WII_IPC_WIIMOTE, 0, "=========================================");
	int dataOffset = 0;
	while (_Size > 0)
	{
		u8 DataFrame[1024];
		u32 Offset = WriteWmReport(DataFrame, WM_READ_DATA_REPLY);
		
		int copySize = _Size;
		if (copySize > 16)
		{
			copySize = 16;
		}

		wm_read_data_reply* pReply = (wm_read_data_reply*)(DataFrame + Offset);
		Offset += sizeof(wm_read_data_reply);
		pReply->buttons = 0;
		pReply->error = 0;
		pReply->size = (copySize - 1) & 0xF;
		pReply->address = Common::swap16(_Address + dataOffset);
		memcpy(pReply->data + dataOffset, _Base, copySize);
		if(copySize < 16) 
		{
			memset(pReply->data + copySize, 0, 16 - copySize);
		}
		dataOffset += copySize;

		LOG(WII_IPC_WIIMOTE, "  SendReadDataReply()");
		LOG(WII_IPC_WIIMOTE, "    Buttons: 0x%04x", pReply->buttons);
		LOG(WII_IPC_WIIMOTE, "    Error: 0x%x", pReply->error);
		LOG(WII_IPC_WIIMOTE, "    Size: 0x%x", pReply->size);
		LOG(WII_IPC_WIIMOTE, "    Address: 0x%04x", pReply->address);

		g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

		_Size -= copySize;
	}

	if (_Size != 0)
	{
		PanicAlert("WiiMote-Plugin: SendReadDataReply() failed");
	}
	LOGV(WII_IPC_WIIMOTE, 0, "==========================================");
}


} // end of namespace
