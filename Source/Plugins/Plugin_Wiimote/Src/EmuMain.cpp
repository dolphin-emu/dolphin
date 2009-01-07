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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>

#include "Common.h" // Common
#include "StringUtil.h" // for ArrayToString

#include "wiimote_hid.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "EmuMain.h"
#include "Encryption.h" // for extension encryption
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd
#include "Config.h" // for g_Config
////////////////////////////////////

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

// ===================================================
/* Bit shift conversions */
// -------------
u32 convert24bit(const u8* src) {
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

u16 convert16bit(const u8* src) {
	return (src[0] << 8) | src[1];
}
// ==============


// ===================================================
/* Calibrate the mouse position to the emulation window. */
// ----------------
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
        // TODO fix on linux
	x = 0.5f;
	y = 0.5f;
#endif
}


// ===================================================
/* Homebrew encryption for 0x00000000 encryption keys. */
// ----------------
void CryptBuffer(u8* _buffer, u8 _size)
{
	for (int i=0; i<_size; i++)
	{
		_buffer[i] = ((_buffer[i] - 0x17) ^ 0x17) & 0xFF;
	}
}

void WriteCrypted16(u8* _baseBlock, u16 _address, u16 _value)
{
	u16 cryptedValue = _value;
	CryptBuffer((u8*)&cryptedValue, sizeof(u16));

	*(u16*)(_baseBlock + _address) = cryptedValue;
	//PanicAlert("Converted %04x to %04x", _value, cryptedValue);
}
// ================


// ===================================================
/* Write initial values to Eeprom and registers. */
// ----------------
void Initialize()
{
	memset(g_Eeprom, 0, WIIMOTE_EEPROM_SIZE);
	memcpy(g_Eeprom, EepromData_0, sizeof(EepromData_0));
	memcpy(g_Eeprom + 0x16D0, EepromData_16D0, sizeof(EepromData_16D0));

	g_ReportingMode = 0;


	/* Extension data for homebrew applications that use the 0x00000000 key. This
	   writes 0x0000 in encrypted form (0xfefe) to 0xfe in the extension register. */
	//WriteCrypted16(g_RegExt, 0xfe, 0x0000); // Fully inserted Nunchuk


	// Copy extension id and calibration to its register	
	if(g_Config.bNunchuckConnected)
	{
		memcpy(g_RegExt + 0x20, nunchuck_calibration, sizeof(nunchuck_calibration));
		memcpy(g_RegExt + 0xfa, nunchuck_id, sizeof(nunchuck_id));
	}
	else if(g_Config.bClassicControllerConnected)
	{
		memcpy(g_RegExt + 0x20, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt + 0xfa, classic_id, sizeof(classic_id));
	}


//	g_RegExt[0xfd] = 0x1e;
//	g_RegExt[0xfc] = 0x9a;
}
// ================


void DoState(void* ptr, int mode) 
{
	//TODO: implement
}

/* We don't need to do anything here. All values will be reset as FreeLibrary() is called
   when we stop a game */
void Shutdown(void) 
{
}


// ===================================================
/* An ack delay of 1 was not small enough, but 2 seemed to work, that was about between 20 ms and
   100 ms in my case. I'm not sure what it means in frame delays. */
// ----------------
void CreateAckDelay(u8 _ChannelID, u16 _ReportID)
{
	// Settings
	int GlobalDelay = 2;

	// Queue an acknowledgment
	wm_ackdelay Tmp;
	Tmp.Delay = GlobalDelay;
	Tmp.ChannelID = _ChannelID;
	Tmp.ReportID = _ReportID;
	AckDelay.push_back(Tmp);
}


void CheckAckDelay()
{
	for (int i = 0; i < AckDelay.size(); i++)
	{
		// See if there are any acks to send
		if (AckDelay.at(i).Delay >= 0)
		{
			if(AckDelay.at(i).Delay == 0)
			{
				WmSendAck(AckDelay.at(i).ChannelID, AckDelay.at(i).ReportID, 0);
				AckDelay.erase(AckDelay.begin() + i);
				continue;
			}
			AckDelay.at(i).Delay--;

			//wprintf("%i  0x%04x  0x%02x", i, AckDelay.at(i).ChannelID, AckDelay.at(i).ReportID);
		}
	}
}
// ================


// ===================================================
/* This function produce Wiimote Input, i.e. reports from the Wiimote in response
   to Output from the Wii. */
// ----------------
void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size) 
{
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
	LOGV(WII_IPC_WIIMOTE, 3, "Wiimote_Input");
	const u8* data = (const u8*)_pData;

	// -----------------------------------------------------
	/* Debugging. We have not yet decided how much of 'data' we will use, it's not determined
	   by sizeof(data). We have to determine it by looking at the data cases. */
	// -----------------	
	/*int size;
	switch(data[1])
	{
	case 0x10:
		size = 4; // I don't know the size
		break;
	case WM_LEDS: // 0x11
		size = sizeof(wm_leds);
		break;
	case WM_DATA_REPORTING:  // 0x12
		size = sizeof(wm_data_reporting);
		break;
	case WM_REQUEST_STATUS: // 0x15
		size = sizeof(wm_request_status);
		break;
	case WM_WRITE_DATA: // 0x16
		size = sizeof(wm_write_data);
		break;
	case WM_READ_DATA: // 0x17
		size = sizeof(wm_read_data);
		break;
	case WM_IR_PIXEL_CLOCK: // 0x13
	case WM_IR_LOGIC: // 0x1a
	case WM_SPEAKER_ENABLE: // 0x14
	case WM_SPEAKER_MUTE:
		size = 1;
		break;
	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", data[1]);
		return;
	}
	std::string Temp = ArrayToString(data, size + 2, 0, 30);
	//LOGV(WII_IPC_WIIMOTE, 3, "   Data: %s", Temp.c_str());
	wprintf("\n%s: InterruptChannel: %s\n", Tm(true).c_str(), Temp.c_str());*/
	// -----------------------------------

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
					HidOutputReport(_channelID, sr);

					/* This is the 0x22 answer to all Inputs. In most games it didn't matter
					   if it was written before or after HidOutputReport(), but Wii Sports
					   and Mario Galaxy would stop working if it was placed before
					   HidOutputReport(). Zelda - TP is even more sensitive and require
					   a delay after the Input for the Nunchuck to work. It seemed to be
					   enough to delay only the Nunchuck registry reads and writes but
					   for now I'm delaying all inputs. Both for status changes and Eeprom
					   and registry reads and writes. */

					// Limit the delay to certain registry reads and writes
					//if((data[1] == WM_WRITE_DATA  || data[1] == WM_READ_DATA)
					//	&& data[3] == 0xa4)
					//{
						CreateAckDelay(_channelID, sr->channel);
					//}
					//else
					//{
						//wm_write_data *wd = (wm_write_data*)sr->data;
						//u32 address = convert24bit(wd->address);
						//WmSendAck(_channelID, sr->channel, address);
					//}
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
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
}


void ControlChannel(u16 _channelID, const void* _pData, u32 _Size) 
{
	const u8* data = (const u8*)_pData;
	// dump raw data
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_ControlChannel");
		std::string Temp = ArrayToString(data, 0, _Size);
		wprintf("\n%s: ControlChannel: %s\n", Tm().c_str(), Temp.c_str());
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


// ===================================================
/* This is called from Wiimote_Update(). See SystemTimers.cpp for a documentation. I'm
   not sure exactly how often this function is called but I think it's tied to the frame
   rate of the game rather than a certain amount of times per second. */
// ----------------
void Update() 
{
	//LOG(WII_IPC_WIIMOTE, "Wiimote_Update");

	switch(g_ReportingMode)
	{
	case 0:
		break;
	case WM_REPORT_CORE:			SendReportCore(g_ReportingChannel);			break;
	case WM_REPORT_CORE_ACCEL:		SendReportCoreAccel(g_ReportingChannel);	break;
	case WM_REPORT_CORE_ACCEL_IR12: SendReportCoreAccelIr12(g_ReportingChannel);break;
	case WM_REPORT_CORE_ACCEL_EXT16: SendReportCoreAccelExt16(g_ReportingChannel);break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6: SendReportCoreAccelIr10Ext(g_ReportingChannel);break;
	}

	// Potentially send a delayed acknowledgement to an InterruptChannel() Output
	CheckAckDelay();
}


} // end of namespace

