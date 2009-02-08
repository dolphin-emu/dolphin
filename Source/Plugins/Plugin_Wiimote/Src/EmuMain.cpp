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
#include "StringUtil.h" // for ArrayToString()
#include "IniFile.h"

#include "EmuDefinitions.h" // Local
#include "main.h" 
#include "wiimote_hid.h"
#include "EmuSubroutines.h"
#include "EmuMain.h"
#include "Encryption.h" // for extension encryption
#include "Logging.h" // for startConsoleWin, Console::Print, GetConsoleHwnd
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
/* Calibrate the mouse position to the emulation window. g_WiimoteInitialize.hWnd is the rendering window handle. */
// ----------------
void GetMousePos(float& x, float& y)
{
#ifdef _WIN32
	POINT point;
	// Get the cursor position for the entire screen
	GetCursorPos(&point);
	// Get the cursor position relative to the upper left corner of the rendering window
	ScreenToClient(g_WiimoteInitialize.hWnd, &point);

	// Get the size of the rendering window. In my case top and left was zero.
	RECT Rect;
	GetClientRect(g_WiimoteInitialize.hWnd, &Rect);
	// Width and height is the size of the rendering window
	int width = Rect.right - Rect.left;
	int height = Rect.bottom - Rect.top;

	// Return the mouse position as a fraction of one
	x = point.x / (float)width;
	y = point.y / (float)height;

	/*
	Console::ClearScreen();
	Console::Print("GetCursorPos: %i %i\n", point.x, point.y);
	Console::Print("GetClientRect: %i %i  %i %i\n", Rect.left, Rect.right, Rect.top, Rect.bottom);
	Console::Print("x and y: %f %f\n", x, y);
	*/
#else
    // TODO fix on linux
	x = 0.5f;
	y = 0.5f;
#endif
}
// ==============


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
/* Load pre-recorded movements */
// ----------------
void LoadRecordedMovements()
{
	IniFile file;
	file.Load("WiimoteMovement.ini");

	for(int i = 0; i < RECORDING_ROWS; i++)
	{
		// Logging
		Console::Print("Recording%i ", i + 1);

		// Get row name
		std::string SaveName = StringFromFormat("Recording%i", i + 1);

		// Get movement
		std::string TmpMovement; file.Get(SaveName.c_str(), "Movement", &TmpMovement, "");

		// Get IR
		std::string TmpIR; file.Get(SaveName.c_str(), "IR", &TmpIR, "");

		// Get time
		std::string TmpTime; file.Get(SaveName.c_str(), "Time", &TmpTime, "");

		// Get IR bytes
		int TmpIRBytes; file.Get(SaveName.c_str(), "IRBytes", &TmpIRBytes, 0);
		VRecording.at(i).IRBytes = TmpIRBytes;

		SRecording Tmp;
		for (int j = 0, k = 0, l = 0; j < TmpMovement.length(); j+=7)
		{
			// Skip blank savings
			if (TmpMovement.length() < 3) continue;

			std::string StrX = TmpMovement.substr(j, 2);
			std::string StrY = TmpMovement.substr(j + 2, 2);
			std::string StrZ = TmpMovement.substr(j + 4, 2);
			u32 TmpX, TmpY, TmpZ;
			AsciiToHex(StrX.c_str(), TmpX);
			AsciiToHex(StrY.c_str(), TmpY);
			AsciiToHex(StrZ.c_str(), TmpZ);			
			Tmp.x = (u8)TmpX;
			Tmp.y = (u8)TmpY;
			Tmp.z = (u8)TmpZ;

			// ---------------------------------
			// Go to next set of IR values
			// ---------
			// If there is no IR data saving we fill the array with zeroes. This should only be able to occur from manual ini editing
			// but we check for it anyway
			if (TmpIRBytes == 0) for(int i = 0; i < 12; i++) Tmp.IR[i] = 0;
			for(int ii = 0; ii < TmpIRBytes; ii++)
			{
				if(TmpIR.length() < (k + i + TmpIRBytes)) continue; // Safety check
				std::string TmpStr = TmpIR.substr(k + ii*2, 2);
				u32 TmpU32;
				AsciiToHex(TmpStr.c_str(), TmpU32);
				Tmp.IR[ii] = (u8)TmpU32;
			}
			if (TmpIRBytes == 10) k += (10*2 + 1); else k += (12*2 + 1);
			// ---------------------

			// Go to next set of time values
			double Time = (double)atoi(TmpTime.substr(l, 5).c_str());
			Tmp.Time = (double)(Time/1000);
			l += 6;

			// Save the values
			VRecording.at(i).Recording.push_back(Tmp);

			// ---------------------------------
			// Log results
			// ---------
			//Console::Print("Time:%f\n", Tmp.Time);
			//std::string TmpIRLog = ArrayToString(Tmp.IR, TmpIRBytes, 0, 30);
			//Console::Print("IR: %s\n", TmpIRLog.c_str());
			//Console::Print("\n");
		}

		// Get HotKey
		int TmpRecordHotKey; file.Get(SaveName.c_str(), "HotKey", &TmpRecordHotKey, -1);
		VRecording.at(i).HotKey = TmpRecordHotKey;

		// Get Recording speed
		int TmpPlaybackSpeed; file.Get(SaveName.c_str(), "PlaybackSpeed", &TmpPlaybackSpeed, -1);
		VRecording.at(i).PlaybackSpeed = TmpPlaybackSpeed;

		// ---------------------------------
		// Logging
		// ---------
		std::string TmpIRLog;
		if(TmpIRBytes > 0)
			TmpIRLog = ArrayToString(VRecording.at(i).Recording.at(0).IR, TmpIRBytes, 0, 30);
		else
			TmpIRLog = "";

		Console::Print("Size:%i HotKey:%i Speed:%i IR: %s\n",
			VRecording.at(i).Recording.size(), VRecording.at(i).HotKey, VRecording.at(i).PlaybackSpeed,
			TmpIRLog.c_str()
			);
		// ---------------------
	}
}
// ================

// Update the accelerometer neutral values
void UpdateEeprom()
{
	g_accel.cal_zero.x = g_Eeprom[22];
	g_accel.cal_zero.y = g_Eeprom[23];
	g_accel.cal_zero.z = g_Eeprom[24];
	g_accel.cal_g.x = g_Eeprom[26] - g_Eeprom[22];
	g_accel.cal_g.y = g_Eeprom[27] - g_Eeprom[24];
	g_accel.cal_g.z = g_Eeprom[28] - g_Eeprom[24];

	g_nu.cal_zero.x = g_RegExt[0x20];
	g_nu.cal_zero.y = g_RegExt[0x21];
	g_nu.cal_zero.z = g_RegExt[0x26]; // Including the g-force
	g_nu.jx.max = g_RegExt[0x28];
	g_nu.jx.min = g_RegExt[0x29];
	g_nu.jx.center = g_RegExt[0x2a];
	g_nu.jy.max = g_RegExt[0x2b];
	g_nu.jy.min = g_RegExt[0x2c];
	g_nu.jy.center = g_RegExt[0x2d];

	Console::Print("\nUpdateEeprom: %i %i %i\n",
		WiiMoteEmu::g_Eeprom[22], WiiMoteEmu::g_Eeprom[23], WiiMoteEmu::g_Eeprom[27]);

	Console::Print("UpdateExtension: %i %i   %i %i %i\n\n",
		WiiMoteEmu::g_RegExt[0x2a], WiiMoteEmu::g_RegExt[0x2d],
		WiiMoteEmu::g_RegExt[20], WiiMoteEmu::g_RegExt[21], WiiMoteEmu::g_RegExt[26]);
}

// Calculate checksum for the nunchuck calibration. The last two bytes.
void ExtensionChecksum(u8 * Calibration)
{
	u8 sum = 0; u8 Byte15, Byte16;
	for (int i = 0; i < sizeof(Calibration) - 2; i++)
	{
		sum += Calibration[i];
		printf("Plus 0x%02x\n", Calibration[i]);
	}
	Byte15 = sum + 0x55; // Byte 15
	Byte16 = sum + 0xaa; // Byte 16
}

// Set initial values
void ResetVariables()
{
	u8 g_Leds = 0x0; // 4 bits
	u8 g_Speaker = 0x0; // 1 = on
	u8 g_SpeakerVoice = 0x0; // 1 = on
	u8 g_IR = 0x0; // 1 = on

	g_ReportingMode = 0;
	g_ReportingChannel = 0;
	g_Encryption = false;

	g_EmulatedWiiMoteInitialized = false;
}

// Update the extension calibration values with our default values
void SetDefaultExtensionRegistry()
{
	// Copy extension id and calibration to its register	
	if(g_Config.bNunchuckConnected)
	{
		memcpy(g_RegExt + 0x20, nunchuck_calibration, sizeof(nunchuck_calibration));
		memcpy(g_RegExt + 0x30, nunchuck_calibration, sizeof(nunchuck_calibration));
		memcpy(g_RegExt + 0xfa, nunchuck_id, sizeof(nunchuck_id));
	}
	else if(g_Config.bClassicControllerConnected)
	{
		memcpy(g_RegExt + 0x20, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt + 0x30, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt + 0xfa, classic_id, sizeof(classic_id));
	}

	UpdateEeprom();
}

// ===================================================
/* Write initial values to Eeprom and registers. */
// ----------------
void Initialize()
{
	if (g_EmulatedWiiMoteInitialized) return;

	// Reset variables
	ResetVariables();

	// Write default Eeprom data
	memset(g_Eeprom, 0, WIIMOTE_EEPROM_SIZE);
	memcpy(g_Eeprom, EepromData_0, sizeof(EepromData_0));
	memcpy(g_Eeprom + 0x16D0, EepromData_16D0, sizeof(EepromData_16D0));

	// Copy extension id and calibration to its register	
	SetDefaultExtensionRegistry();

	g_ReportingMode = 0;
	g_EmulatedWiiMoteInitialized = true;

	// Load pre-recorded movements
	LoadRecordedMovements();

	// Set default recording values
	for (int i = 0; i < 3; i++)
	{
		g_RecordingPlaying[i] = -1;
		g_RecordingCounter[i] = 0;
		g_RecordingPoint[i] = 0;
		g_RecordingStart[i] = 0;
		g_RecordingCurrentTime[i] = 0;
	}

	/* The Nuncheck extension ID for homebrew applications that use the zero key. This writes 0x0000
	   in encrypted form (0xfefe) to 0xfe in the extension register. */
	//WriteCrypted16(g_RegExt, 0xfe, 0x0000); // Fully inserted Nunchuk

	// I forgot what these were for? Is this the zero key encrypted 0xa420?
	//	g_RegExt[0xfd] = 0x1e;
	//	g_RegExt[0xfc] = 0x9a;
}
// ================


void DoState(void* ptr, int mode) 
{
	//TODO: implement
}

/* This is not needed if we call FreeLibrary() when we stop a game, but if it's not called we need to reset
   these variables. */
void Shutdown(void) 
{
	ResetVariables();
}


// ===================================================
/* An ack delay of 1 was not small enough, but 2 seemed to work, that was about between 20 ms and
   100 ms in my case in Zelda - TP. You may have to increase this value for other things to work, for
   example in the wpad demo I had to set it to at least 3 for the Sound to be able to turned on (I have
   an update rate of around 150 fps in the wpad demo) */
// ----------------
void CreateAckDelay(u8 _ChannelID, u16 _ReportID)
{
	// Settings
	int GlobalDelay = 2;

	// Queue an acknowledgment
	wm_ackdelay Tmp;
	Tmp.Delay = GlobalDelay;
	Tmp.ChannelID = _ChannelID;
	Tmp.ReportID = (u8)_ReportID;
	AckDelay.push_back(Tmp);
}


void CheckAckDelay()
{
	for (int i = 0; i < (int)AckDelay.size(); i++)
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

			//Console::Print("%i  0x%04x  0x%02x", i, AckDelay.at(i).ChannelID, AckDelay.at(i).ReportID);
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
	//Console::Print("Emu InterruptChannel\n");

	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
	LOGV(WII_IPC_WIIMOTE, 3, "Wiimote_Input");
	const u8* data = (const u8*)_pData;

	/* Debugging. We have not yet decided how much of 'data' we will use, it's not determined
	   by sizeof(data). We have to determine it by looking at the data cases. */
	InterruptDebugging(true, data);

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

					// There are no 0x22 replys to these report from the real wiimote from what I could see
					// Report 0x10 that seems to be only used for rumble
					if(!(data[1] == WM_READ_DATA && data[2] == 0x00)
						&& !(data[1] == WM_REQUEST_STATUS)
						&& !(data[1] == WM_WRITE_SPEAKER_DATA))
						if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent) CreateAckDelay((u8)_channelID, (u16)sr->channel);
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
	//Console::Print("Emu ControlChannel\n");

	const u8* data = (const u8*)_pData;
	// Dump raw data
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_ControlChannel");
		std::string Temp = ArrayToString(data, 0, _Size);
		Console::Print("\n%s: ControlChannel: %s\n", Tm().c_str(), Temp.c_str());
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

			// Return handshake
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
	//Console::Print("Emu Update: %i\n", g_ReportingMode);

	switch(g_ReportingMode)
	{
	case 0:
		break;
	case WM_REPORT_CORE:			SendReportCore(g_ReportingChannel);				break;
	case WM_REPORT_CORE_ACCEL:		SendReportCoreAccel(g_ReportingChannel);		break;
	case WM_REPORT_CORE_ACCEL_IR12: SendReportCoreAccelIr12(g_ReportingChannel);	break;
	case WM_REPORT_CORE_ACCEL_EXT16: SendReportCoreAccelExt16(g_ReportingChannel);	break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6: SendReportCoreAccelIr10Ext(g_ReportingChannel);break;
	}

	// Potentially send a delayed acknowledgement to an InterruptChannel() Output
	CheckAckDelay();
}


} // end of namespace

