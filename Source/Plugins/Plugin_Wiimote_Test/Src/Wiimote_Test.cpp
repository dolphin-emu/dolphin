#include <wx/aboutdlg.h>

#include "Common.h"
#include "StringUtil.h"

#include "pluginspecs_wiimote.h"

#include "wiimote_hid.h"

SWiimoteInitialize g_WiimoteInitialize;
#define VERSION_STRING "0.1"

//******************************************************************************
// Definitions and variable declarations
//******************************************************************************
#define WIIMOTE_EEPROM_SIZE (16*1024)
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34

u8 g_Leds;

u8 g_Eeprom[WIIMOTE_EEPROM_SIZE];

u8 g_RegSpeaker[WIIMOTE_REG_SPEAKER_SIZE];
u8 g_RegExt[WIIMOTE_REG_EXT_SIZE];
u8 g_RegIr[WIIMOTE_REG_IR_SIZE];

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
void __Log(int log, const char *format, ...)
{
	char* temp = (char*)alloca(strlen(format)+512);
	va_list args;
	va_start(args, format);
	CharArrayFromFormatV(temp, 512, format, args);
	va_end(args);
	g_WiimoteInitialize.pLog(temp);
}
//void PanicAlert(const char* fmt, ...);

void HidOutputReport(wm_report* sr);

void WmLeds(wm_leds* leds);
void WmReadData(wm_read_data* rd);
void WmWriteData(wm_write_data* wd);
void WmRequestStatus(wm_request_status* rs);
void WmDataReporting(wm_data_reporting* dr);

void SendReadDataReply(void* _Base, u16 _Address, u8 _Size);
void SendWriteDataReply();
void SendReportCoreAccel();
void SendReportCoreAccelIr12();

int WriteWmReport(u8* dst, u8 channel);

static u32 convert24bit(const u8* src) {
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

static u16 convert16bit(const u8* src) {
	return (src[0] << 8) | src[1];
}

//******************************************************************************
// Exports
//******************************************************************************
extern "C" void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_WIIMOTE;
#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Wiimote Test (DebugFast) " VERSION_STRING);
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Wiimote Test " VERSION_STRING);
#else
	sprintf(_PluginInfo->Name, "Wiimote Test (Debug) " VERSION_STRING);
#endif
#endif
}


extern "C" void DllAbout(HWND _hParent) 
{
	wxAboutDialogInfo info;
	info.AddDeveloper(_T("masken (masken3@gmail.com)"));
	info.SetDescription(_T("Wiimote test plugin"));
	wxAboutBox(info);
}

extern "C" void DllConfig(HWND _hParent)
{
}


extern "C" void Wiimote_Initialize(SWiimoteInitialize _WiimoteInitialize)
{
	g_WiimoteInitialize = _WiimoteInitialize;

	memset(g_Eeprom, 0, WIIMOTE_EEPROM_SIZE);
	memcpy(g_Eeprom, EepromData_0, sizeof(EepromData_0));
	memcpy(g_Eeprom + 0x16D0, EepromData_16D0, sizeof(EepromData_16D0));
}

extern "C" void Wiimote_DoState(void* ptr, int mode) {
}

extern "C" void Wiimote_Shutdown(void) 
{
}

extern "C" void Wiimote_Output(const void* _pData, u32 _Size) {
	const u8* data = (const u8*)_pData;
	// dump raw data
	{
		LOG(WIIMOTE, "Wiimote_Output");
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", data[j]);
			Temp.append(Buffer);
		}
		LOG(WIIMOTE, "   Data: %s", Temp.c_str());
	}

	hid_packet* hidp = (hid_packet*) data;

	if(hidp->type == HID_TYPE_SET_REPORT &&
		hidp->param == HID_PARAM_OUTPUT)
	{
		HidOutputReport((wm_report*)hidp->data);
	} else {
		PanicAlert("HidOutput: Unknown type 0x%02x", data[0]);
	}
}

extern "C" unsigned int Wiimote_GetAttachedControllers() {
	return 1;
}

//******************************************************************************
// Subroutines
//******************************************************************************
void HidOutputReport(wm_report* sr) {
	LOG(WIIMOTE, "  HidOutputReport(0x%02x)", sr->channel);

	switch(sr->channel)
	{
	case WM_LEDS:
		WmLeds((wm_leds*)sr->data);
		break;
	case WM_READ_DATA:
		WmReadData((wm_read_data*)sr->data);
		break;
	case WM_REQUEST_STATUS:
		WmRequestStatus((wm_request_status*)sr->data);
		break;
	case WM_IR_PIXEL_CLOCK:
	case WM_IR_LOGIC:
		LOG(WIIMOTE, " IR Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	case WM_WRITE_DATA:
		WmWriteData((wm_write_data*)sr->data);
		break;
	case WM_DATA_REPORTING:
		WmDataReporting((wm_data_reporting*)sr->data);
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->channel);
		return;
	}
}

void WmLeds(wm_leds* leds) {
	LOG(WIIMOTE, " Set LEDs");
	LOG(WIIMOTE, "  Leds: %x", leds->leds);
	LOG(WIIMOTE, "  Rumble: %x", leds->rumble);

	g_Leds = leds->leds;
}

void WmDataReporting(wm_data_reporting* dr) {
	LOG(WIIMOTE, " Set Data reporting mode");
	LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
	LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
	LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

	if(dr->mode == 0x33)
		SendReportCoreAccelIr12();
	else if(dr->mode == 0x31)
		SendReportCoreAccel();
}

void SendReportCoreAccelIr12() {
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_IR12);

	wm_report_core_accel_ir12* pReport = (wm_report_core_accel_ir12*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ir12);
	memset(pReport, 0, sizeof(wm_report_core_accel_ir12));
	memset(pReport->ir, 0xFF, sizeof(pReport->ir));
	pReport->c.b = 1;
	pReport->a.x = 0x81;
	pReport->a.y = 0x78;
	pReport->a.z = 0xD9;
	pReport->ir[0].x = 320 & 0xFF;
	pReport->ir[0].y = 240;
	pReport->ir[0].size = 10;
	pReport->ir[0].xHi = 320 >> 8;
	pReport->ir[0].yHi = 0;

	LOG(WIIMOTE, "  SendReportCoreAccelIr12()");

	g_WiimoteInitialize.pWiimoteInput(DataFrame, Offset);
}

void SendReportCoreAccel() {
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL);

	wm_report_core_accel* pReport = (wm_report_core_accel*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel);
	memset(pReport, 0, sizeof(wm_report_core_accel));
	pReport->c.a = 1;
	pReport->a.x = 0x82;
	pReport->a.y = 0x75;
	pReport->a.z = 0xD6;

	LOG(WIIMOTE, "  SendReportCoreAccel()");

	g_WiimoteInitialize.pWiimoteInput(DataFrame, Offset);
}

void WmReadData(wm_read_data* rd) {
	u32 address = convert24bit(rd->address);
	u16 size = convert16bit(rd->size);
	LOG(WIIMOTE, " Read data");
	LOG(WIIMOTE, "  Address space: %x", rd->space);
	LOG(WIIMOTE, "  Address: 0x%06x", address);
	LOG(WIIMOTE, "  Size: 0x%04x", size);
	LOG(WIIMOTE, "  Rumble: %x", rd->rumble);

	if(size <= 16 && rd->space == 0) {
		SendReadDataReply(g_Eeprom, address, (u8)size);
	} else {
		PanicAlert("WmReadData: unimplemented parameters!");
	}
}

void WmWriteData(wm_write_data* wd) {
	u32 address = convert24bit(wd->address);
	LOG(WIIMOTE, " Write data");
	LOG(WIIMOTE, "  Address space: %x", wd->space);
	LOG(WIIMOTE, "  Address: 0x%06x", address);
	LOG(WIIMOTE, "  Size: 0x%02x", wd->size);
	LOG(WIIMOTE, "  Rumble: %x", wd->rumble);

	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom + address, wd->data, wd->size);
		SendWriteDataReply();
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
		memcpy(block + address, wd->data, wd->size);
		SendWriteDataReply();
	} else {
		PanicAlert("WmWriteData: unimplemented parameters!");
	}
}

void SendWriteDataReply() {
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_WRITE_DATA_REPLY);

	LOG(WIIMOTE, "  SendWriteDataReply()");

	g_WiimoteInitialize.pWiimoteInput(DataFrame, Offset);
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

void WmRequestStatus(wm_request_status* rs) {
	LOG(WIIMOTE, " Request Status");
	LOG(WIIMOTE, "  Rumble: %x", rs->rumble);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report));
	pStatus->leds = g_Leds;
	pStatus->ir = 1;
	pStatus->battery = 100;	//arbitrary number

	LOG(WIIMOTE, "  SendStatusReport()");
	LOG(WIIMOTE, "    Flags: 0x%02x", pStatus->padding1[2]);
	LOG(WIIMOTE, "    Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(DataFrame, Offset);
}

void SendReadDataReply(void* _Base, u16 _Address, u8 _Size)
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_READ_DATA_REPLY);

	_dbg_assert_(WIIMOTE, _Size <= 16);

	wm_read_data_reply* pReply = (wm_read_data_reply*)(DataFrame + Offset);
	Offset += sizeof(wm_read_data_reply);
	pReply->buttons = 0;
	pReply->error = 0;
	pReply->size = _Size - 1;
	pReply->address = Common::swap16(_Address);
	memcpy(pReply->data, _Base, _Size);
	if(_Size < 16) {
		memset(pReply->data + _Size, 0, 16 - _Size);
	}

	LOG(WIIMOTE, "  SendReadDataReply()");
	LOG(WIIMOTE, "    Buttons: 0x%04x", pReply->buttons);
	LOG(WIIMOTE, "    Error: 0x%x", pReply->error);
	LOG(WIIMOTE, "    Size: 0x%x", pReply->size);
	LOG(WIIMOTE, "    Address: 0x%04x", pReply->address);

	g_WiimoteInitialize.pWiimoteInput(DataFrame, Offset);
}
