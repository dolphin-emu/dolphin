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

#include <string>
#include "common.h"
#include "wiimote_hid.h"

extern SWiimoteInitialize g_WiimoteInitialize;
extern void __Log(int log, const char *format, ...);

namespace WiiMoteEmu
{
	
//******************************************************************************
// Definitions and variable declarations
//******************************************************************************

//libogc bounding box, in smoothed IR coordinates: 232,284 792,704
//we'll use it to scale our mouse coordinates
#define LEFT 232
#define TOP 284
#define RIGHT 792
#define BOTTOM 704
#define SENSOR_BAR_RADIUS 200

// vars 
#define WIIMOTE_EEPROM_SIZE (16*1024)
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34

u8 g_Leds = 0x1;

u8 g_Eeprom[WIIMOTE_EEPROM_SIZE];

u8 g_RegSpeaker[WIIMOTE_REG_SPEAKER_SIZE];
u8 g_RegExt[WIIMOTE_REG_EXT_SIZE];
u8 g_RegIr[WIIMOTE_REG_IR_SIZE];

u8 g_ReportingMode;
u16 g_ReportingChannel;

static const u8 EepromData_0[] = {
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30,
	0xA7, 0x74, 0xD3, 0xA1, 0xAA, 0x8B, 0x99, 0xAE,
	0x9E, 0x78, 0x30, 0xA7, 0x74, 0xD3, 0x82, 0x82,
	0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E,
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38,
	0x40, 0x3E
};

static const u8 EepromData_16D0[] = {
	0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
	0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
	0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13
};

//******************************************************************************
// Subroutine declarations
//******************************************************************************


void HidOutputReport(u16 _channelID, wm_report* sr);

void WmLeds(u16 _channelID, wm_leds* leds);
void WmReadData(u16 _channelID, wm_read_data* rd);
void WmWriteData(u16 _channelID, wm_write_data* wd);
void WmRequestStatus(u16 _channelID, wm_request_status* rs);
void WmDataReporting(u16 _channelID, wm_data_reporting* dr);

void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, u8 _Size);
void SendReportCoreAccel(u16 _channelID);
void SendReportCoreAccelIr12(u16 _channelID);
void SendReportCore(u16 _channelID);
void SendReportCoreAccelIr10Ext(u16 _channelID);

int WriteWmReport(u8* dst, u8 channel);
void WmSendAck(u16 _channelID, u8 _reportID);

static u32 convert24bit(const u8* src) {
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

static u16 convert16bit(const u8* src) {
	return (src[0] << 8) | src[1];
}

void GetMousePos(float& x, float& y)
{
#ifdef _WIN32
	POINT point;

	GetCursorPos(&point);
	ScreenToClient(g_WiimoteInitialize.hWnd, &point);

	RECT Rect;
	GetClientRect(g_WiimoteInitialize.hWnd, &Rect);

	int width = Rect.right - Rect.left;
	int height = Rect.bottom - Rect.top;

	x = point.x / (float)width;
	y = point.y / (float)height;
#else
	x = 0.5f;
	y = 0.5f;
#endif
}


void CryptBuffer(u8* _buffer, u8 _size)
{
	for (int i=0; i<_size; i++)
	{
		_buffer[i] = ((_buffer[i] - 0x17) ^ 0x17) & 0xFF;
	}
}

void WriteCryped16(u8* _baseBlock, u16 _address, u16 _value)
{
	u16 cryptedValue = _value;
	CryptBuffer((u8*)&cryptedValue, sizeof(u16));

	*(u16*)(_baseBlock + _address) = cryptedValue;
}

void Initialize()
{
	memset(g_Eeprom, 0, WIIMOTE_EEPROM_SIZE);
	memcpy(g_Eeprom, EepromData_0, sizeof(EepromData_0));
	memcpy(g_Eeprom + 0x16D0, EepromData_16D0, sizeof(EepromData_16D0));

	g_ReportingMode = 0;


	
	WriteCryped16(g_RegExt, 0xfe, 0x0000);

//	g_RegExt[0xfd] = 0x1e;
//	g_RegExt[0xfc] = 0x9a;

}

void DoState(void* ptr, int mode) 
{
	//TODO: implement
}

void Shutdown(void) 
{
}

void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size) 
{
	
	const u8* data = (const u8*)_pData;

	// dump raw data
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_Input");
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", data[j]);
			Temp.append(Buffer);
		}
		LOG(WII_IPC_WIIMOTE, "   Data: %s", Temp.c_str());
	}
	hid_packet* hidp = (hid_packet*) data;

	switch(hidp->type)
	{
	case HID_TYPE_DATA:
		{
			switch(hidp->param)
			{
			case HID_PARAM_OUTPUT:
				{
					wm_report* sr = (wm_report*)hidp->data;
					WmSendAck(_channelID, sr->channel);
					HidOutputReport(_channelID, sr);
				}
				break;

			default:
				PanicAlert("HidInput: HID_TYPE_DATA - param 0x%02x", hidp->type, hidp->param);
				break;
			}
		}
		break;

	default:
		PanicAlert("HidInput: Unknown type 0x%02x and param 0x%02x", hidp->type, hidp->param);
		break;
	}
}

void ControlChannel(u16 _channelID, const void* _pData, u32 _Size) 
{
	const u8* data = (const u8*)_pData;
	// dump raw data
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_ControlChannel");
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", data[j]);
			Temp.append(Buffer);
		}
		LOG(WII_IPC_WIIMOTE, "   Data: %s", Temp.c_str());
	}

	hid_packet* hidp = (hid_packet*) data;
	switch(hidp->type)
	{
	case HID_TYPE_HANDSHAKE:
		if (hidp->param == HID_PARAM_INPUT)
		{
			PanicAlert("HID_TYPE_HANDSHAKE - HID_PARAM_INPUT");
		}
		else
		{
			PanicAlert("HID_TYPE_HANDSHAKE - HID_PARAM_OUTPUT");
		}
		break;

	case HID_TYPE_SET_REPORT:
		if (hidp->param == HID_PARAM_INPUT)
		{
			PanicAlert("HID_TYPE_SET_REPORT input");
		}
		else
		{
			HidOutputReport(_channelID, (wm_report*)hidp->data);

			//return handshake
			u8 handshake = 0;
			g_WiimoteInitialize.pWiimoteInput(_channelID, &handshake, 1);
		}
		break;

	case HID_TYPE_DATA:
		PanicAlert("HID_TYPE_DATA %s", hidp->type, hidp->param == HID_PARAM_INPUT ? "input" : "output");
		break;

	default:
		PanicAlert("HidControlChanel: Unknown type %x and param %x", hidp->type, hidp->param);
		break;
	}

}

void Update() 
{
	//LOG(WII_IPC_WIIMOTE, "Wiimote_Update");

	switch(g_ReportingMode) {
	case 0:
		break;
	case WM_REPORT_CORE:			SendReportCore(g_ReportingChannel);			break;
	case WM_REPORT_CORE_ACCEL:		SendReportCoreAccel(g_ReportingChannel);	break;
	case WM_REPORT_CORE_ACCEL_IR12: SendReportCoreAccelIr12(g_ReportingChannel);break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6: SendReportCoreAccelIr10Ext(g_ReportingChannel);break;
	}
	// g_ReportingMode = 0;
}

//******************************************************************************
// Subroutines
//******************************************************************************
void HidOutputReport(u16 _channelID, wm_report* sr) {
	LOG(WII_IPC_WIIMOTE, "  HidOutputReport(0x%02x)", sr->channel);

	switch(sr->channel)
	{
	case 0x10:
		LOG(WII_IPC_WIIMOTE, "HidOutputReport: unknown sr->channel 0x10");
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
		LOG(WII_IPC_WIIMOTE, " IR Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	case WM_WRITE_DATA:
		WmWriteData(_channelID, (wm_write_data*)sr->data);
		break;
	case WM_DATA_REPORTING:
		WmDataReporting(_channelID, (wm_data_reporting*)sr->data);
		break;

	case WM_SPEAKER_ENABLE:
		LOG(WII_IPC_WIIMOTE, " WM Speaker Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;

	case WM_SPEAKER_MUTE:
		LOG(WII_IPC_WIIMOTE, " WM Mute Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->channel);
		return;
	}
}

void WmLeds(u16 _channelID, wm_leds* leds) {
	LOG(WII_IPC_WIIMOTE, " Set LEDs");
	LOG(WII_IPC_WIIMOTE, "  Leds: %x", leds->leds);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", leds->rumble);

	g_Leds = leds->leds;
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


	LOG(WII_IPC_WIIMOTE, "  WMSendAck()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


void WmDataReporting(u16 _channelID, wm_data_reporting* dr) 
{
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
}


void FillReportInfo(wm_core& _core)
{
	memset(&_core, 0x00, sizeof(wm_core));

#ifdef _WIN32
	_core.a = GetAsyncKeyState(VK_LBUTTON) ? 1 : 0;
	_core.b = GetAsyncKeyState(VK_RBUTTON) ? 1 : 0;
#else 
        // TODO: fill in
#endif
}

void FillReportAcc(wm_accel& _acc)
{
	_acc.x = 0x00;
	_acc.y = 0x00;
	_acc.z = 0x00;
}

void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1)
{
	memset(&_ir0, 0xFF, sizeof(wm_ir_extended));
	memset(&_ir1, 0xFF, sizeof(wm_ir_extended));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	int y0 = TOP + (MouseY * (BOTTOM - TOP));
	int y1 = TOP + (MouseY * (BOTTOM - TOP));

	int x0 = LEFT + (MouseX * (RIGHT - LEFT)) - SENSOR_BAR_RADIUS;
	int x1 = LEFT + (MouseX * (RIGHT - LEFT)) + SENSOR_BAR_RADIUS;

	x0 = 1023 - x0;
	_ir0.x = x0 & 0xFF;
	_ir0.y = y0 & 0xFF;
	_ir0.size = 10;
	_ir0.xHi = x0 >> 8;
	_ir0.yHi = y0 >> 8;

	x1 = 1023 - x1;
	_ir1.x = x1;
	_ir1.y = y1 & 0xFF;
	_ir1.size = 10;
	_ir1.xHi = x1 >> 8;
	_ir1.yHi = y1 >> 8;
}

void FillReportIRBasic(wm_ir_basic& _ir0, wm_ir_basic& _ir1)
{
	memset(&_ir0, 0xFF, sizeof(wm_ir_basic));
	memset(&_ir1, 0xFF, sizeof(wm_ir_basic));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	int y1 = TOP + (MouseY * (BOTTOM - TOP));
	int y2 = TOP + (MouseY * (BOTTOM - TOP));

	int x1 = LEFT + (MouseX * (RIGHT - LEFT)) - SENSOR_BAR_RADIUS;
	int x2 = LEFT + (MouseX * (RIGHT - LEFT)) + SENSOR_BAR_RADIUS;

	x1 = 1023 - x1;
	_ir0.x1 = x1 & 0xFF;
	_ir0.y1 = y1 & 0xFF;
	_ir0.x1High = (x1 >> 8) & 0x3;
	_ir0.y1High = (y1 >> 8) & 0x3;

	x2 = 1023 - x2;
	_ir1.x2 = x2 & 0xFF;
	_ir1.y2 = y2 & 0xFF;
	_ir1.x2High = (x2 >> 8) & 0x3;
	_ir1.y2High = (y2 >> 8) & 0x3;
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

	LOG(WII_IPC_WIIMOTE, "  SendReportCoreAccelIr12()");

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
			PanicAlert("fsfsd");
			break;
/*		case 0xB0:
			block = g_RegIr;
			blockSize = WIIMOTE_REG_IR_SIZE;
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
}

void WmWriteData(u16 _channelID, wm_write_data* wd) 
{
	u32 address = convert24bit(wd->address);
	LOG(WII_IPC_WIIMOTE, " Write data");
	LOG(WII_IPC_WIIMOTE, "  Address space: %x", wd->space);
	LOG(WII_IPC_WIIMOTE, "  Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "  Size: 0x%02x", wd->size);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", wd->rumble);

	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom + address, wd->data, wd->size);

	//	WmSendAck(_channelID, WM_WRITE_DATA);
	}
	else if(wd->size <= 16 && (wd->space == WM_SPACE_REGS1 || wd->space == WM_SPACE_REGS2))
	{
		u8* block;
		u32 blockSize;
		switch((address >> 16) & 0xFE) {
		case 0xA2:
			block = g_RegSpeaker;
			blockSize = WIIMOTE_REG_SPEAKER_SIZE;
			break;
		case 0xA4:
			block = g_RegExt;
			blockSize = WIIMOTE_REG_EXT_SIZE;
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
		memcpy(wd->data, block + address, wd->size);

	} else {
		PanicAlert("WmWriteData: unimplemented parameters!");
	}

	// just added for home brew.... hmmmm
	WmSendAck(_channelID, WM_WRITE_DATA);
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

void WmRequestStatus(u16 _channelID, wm_request_status* rs) {
	LOG(WII_IPC_WIIMOTE, " Request Status");
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", rs->rumble);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report));
	pStatus->leds = g_Leds;
	pStatus->ir = 1;
	pStatus->battery = 0x4F;	//arbitrary number
	pStatus->extension = 0;

	LOG(WII_IPC_WIIMOTE, "  SendStatusReport()");
	LOG(WII_IPC_WIIMOTE, "    Flags: 0x%02x", pStatus->padding1[2]);
	LOG(WII_IPC_WIIMOTE, "    Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}

void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, u8 _Size)
{
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
}


} // end of namespace