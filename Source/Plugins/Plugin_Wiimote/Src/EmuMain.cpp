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

#include <vector>
#include <string>

#include "../../../Core/InputCommon/Src/SDL.h" // Core
#include "../../../Core/InputCommon/Src/XInput.h"

#include "Common.h" // Common
#include "StringUtil.h" // for ArrayToString()
#include "IniFile.h"
#include "pluginspecs_wiimote.h"

#include "EmuDefinitions.h" // Local
#include "main.h" 
#include "wiimote_hid.h"
#include "EmuSubroutines.h"
#include "EmuMain.h"
#include "Encryption.h" // for extension encryption
#include "Config.h" // for g_Config

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

/* Homebrew encryption for 16 byte zero keys. */
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
}

/* Calculate Extenstion Regisister Calibration Checksum */
// This function is not currently used, it's just here to show how the values
// in EmuDefinitions.h are calculated.
void GetCalibrationChecksum()
{
	u8 sum = 0;
	for (u32 i = 0; i < sizeof(nunchuck_calibration) - 2; i++)
		sum += nunchuck_calibration[i];

	INFO_LOG(WIIMOTE, "0x%02x 0x%02x",  (sum + 0x55), (sum + 0xaa));
}

// Calculate checksum for the nunchuck calibration. The last two bytes.
void ExtensionChecksum(u8 * Calibration)
{
	u8 sum = 0; //u8 Byte15, Byte16;
	for (u32 i = 0; i < sizeof(Calibration) - 2; i++)
	{
		sum += Calibration[i];
		//INFO_LOG(WIIMOTE, "Plus 0x%02x", Calibration[i]);
	}
	//	Byte15 = sum + 0x55; // Byte 15
	//	Byte16 = sum + 0xaa; // Byte 16
}

/* Bit shift conversions */
u32 convert24bit(const u8* src) {
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

u16 convert16bit(const u8* src) {
	return (src[0] << 8) | src[1];
}

/* Load pre-recorded movements */
void LoadRecordedMovements()
{
	INFO_LOG(WIIMOTE, "LoadRecordedMovements()");

	IniFile file;
	file.Load(FULL_CONFIG_DIR "WiimoteMovement.ini");

	for(int i = 0; i < RECORDING_ROWS; i++)
	{
		//INFO_LOG(WIIMOTE, "Recording%i ", i + 1);

		// Temporary storage
		int iTmp;
		std::string STmp;

		// First clear the list
		VRecording.at(i).Recording.clear();

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
		for (int j = 0, k = 0, l = 0; (u32)j < TmpMovement.length(); j+=13)
		{
			// Skip blank savings
			if (TmpMovement.length() < 3) continue;

			// Avoid going to far, this can only happen with modified ini files, but we check for it anyway
			if (TmpMovement.length() < (u32)j + 12) continue;

			// Skip old style recordings
			if (TmpMovement.substr(j, 1) != "-" && TmpMovement.substr(j, 1) != "+") continue;

			std::string StrX = TmpMovement.substr(j, 4);
			std::string StrY = TmpMovement.substr(j + 4, 4);
			std::string StrZ = TmpMovement.substr(j + 8, 4);		
			Tmp.x = atoi(StrX.c_str());
			Tmp.y = atoi(StrY.c_str());
			Tmp.z = atoi(StrZ.c_str());

			// Go to next set of IR values

			// If there is no IR data saving we fill the array with
			// zeroes. This should only be able to occur from manual ini
			// editing but we check for it anyway
			for(int ii = 0; ii < TmpIRBytes; ii++)
			{
				if(TmpIR.length() < (u32)(k + i + TmpIRBytes)) continue; // Safety check
				std::string TmpStr = TmpIR.substr(k + ii*2, 2);
				u32 TmpU32;
				AsciiToHex(TmpStr.c_str(), TmpU32);
				Tmp.IR[ii] = (u8)TmpU32;
			}
			if (TmpIRBytes == 10) k += (10*2 + 1); else k += (12*2 + 1);

			// Go to next set of time values
			double Time = (double)atoi(TmpTime.substr(l, 5).c_str());
			Tmp.Time = (double)(Time/1000);
			l += 6;

			// Save the values
			VRecording.at(i).Recording.push_back(Tmp);

			// Log results
			/*INFO_LOG(WIIMOTE, "Time:%f", Tmp.Time);
			std::string TmpIRLog = ArrayToString(Tmp.IR, TmpIRBytes, 0, 30);
			INFO_LOG(WIIMOTE, "IR: %s", TmpIRLog.c_str());
			INFO_LOG(WIIMOTE, "");*/
		}

		// Get HotKey
		file.Get(SaveName.c_str(), "HotKeySwitch", &iTmp, 3); VRecording.at(i).HotKeySwitch = iTmp;
		file.Get(SaveName.c_str(), "HotKeyWiimote", &iTmp, 10); VRecording.at(i).HotKeyWiimote = iTmp;
		file.Get(SaveName.c_str(), "HotKeyNunchuck", &iTmp, 10); VRecording.at(i).HotKeyNunchuck = iTmp;
		file.Get(SaveName.c_str(), "HotKeyIR", &iTmp, 10); VRecording.at(i).HotKeyIR = iTmp;

		// Get Recording speed
		int TmpPlaybackSpeed; file.Get(SaveName.c_str(), "PlaybackSpeed", &TmpPlaybackSpeed, -1);
		VRecording.at(i).PlaybackSpeed = TmpPlaybackSpeed;

		// Logging
		/*std::string TmpIRLog;
		if(TmpIRBytes > 0 && VRecording.size() > i)
			TmpIRLog = ArrayToString(VRecording.at(i).Recording.at(0).IR, TmpIRBytes, 0, 30);
		else
			TmpIRLog = "";
		
		INFO_LOG(WIIMOTE, "Size:%i HotKey:%i PlSpeed:%i IR:%s X:%i Y:%i Z:%i",
			VRecording.at(i).Recording.size(), VRecording.at(i).HotKeyWiimote, VRecording.at(i).PlaybackSpeed,
			TmpIRLog.c_str(),
			VRecording.at(i).Recording.at(0).x, VRecording.at(i).Recording.at(0).y, VRecording.at(i).Recording.at(0).z
			);*/
	}
}

/* Calibrate the mouse position to the emulation window. g_WiimoteInitialize.hWnd is the rendering window handle. */
void GetMousePos(float& x, float& y)
{
#ifdef _WIN32
	POINT point;
	// Get the cursor position for the entire screen
	GetCursorPos(&point);
	// Get the cursor position relative to the upper left corner of the rendering window
	ScreenToClient(g_WiimoteInitialize.hWnd, &point);

	// Get the size of the rendering window. (In my case Rect.top and Rect.left was zero.)
	RECT Rect;
	GetClientRect(g_WiimoteInitialize.hWnd, &Rect);
	// Width and height is the size of the rendering window
	float WinWidth = (float)(Rect.right - Rect.left);
	float WinHeight = (float)(Rect.bottom - Rect.top);
	float XOffset = 0, YOffset = 0;
	float PictureWidth = WinWidth, PictureHeight = WinHeight;

	/* Calculate the actual picture size and location */
	//		Output: PictureWidth, PictureHeight, XOffset, YOffset
	if (g_Config.bKeepAR43 || g_Config.bKeepAR169)
	{
		// The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
		float Ratio = WinWidth / WinHeight / (g_Config.bKeepAR43 ? (4.0f / 3.0f) : (16.0f / 9.0f));

		// Check if height or width is the limiting factor. If ratio > 1 the picture is to wide and have to limit the width.
		if (Ratio > 1)
		{
			// Calculate the new width and height for glViewport, this is not the actual size of either the picture or the screen
			PictureWidth = WinWidth / Ratio;

			// Calculate the new X offset

			// Move the left of the picture to the middle of the screen
			XOffset = XOffset + WinWidth / 2.0f;
			// Then remove half the picture height to move it to the horizontal center
			XOffset = XOffset - PictureWidth / 2.0f;
		}
		// The window is to high, we have to limit the height
		else
		{
			// Calculate the new width and height for glViewport, this is not the actual size of either the picture or the screen
			// Invert the ratio to make it > 1
			Ratio = 1.0f / Ratio;
			PictureHeight = WinHeight / Ratio;

			// Calculate the new Y offset
			// Move the top of the picture to the middle of the screen
			YOffset = YOffset + WinHeight / 2.0f;
			// Then remove half the picture height to move it to the vertical center
			YOffset = YOffset - PictureHeight / 2.0f;
		}
		/*
		INFO_LOG(WIIMOTE, "Screen         Width:%4.0f Height:%4.0f Ratio:%1.2f", WinWidth, WinHeight, Ratio);
		INFO_LOG(WIIMOTE, "Picture        Width:%4.1f Height:%4.1f YOffset:%4.0f XOffset:%4.0f", PictureWidth, PictureHeight, YOffset, XOffset);
		*/
	}

	// Crop the picture from 4:3 to 5:4 or from 16:9 to 16:10.
	//		Output: PictureWidth, PictureHeight, XOffset, YOffset
	if ((g_Config.bKeepAR43 || g_Config.bKeepAR169) && g_Config.bCrop)
	{
		float Ratio = g_Config.bKeepAR43 ? ((4.0f / 3.0f) / (5.0f / 4.0f)) : (((16.0f / 9.0f) / (16.0f / 10.0f)));
		
		// The width and height we will add  (calculate this before PictureWidth and PictureHeight is adjusted)
		float IncreasedWidth = (Ratio - 1.0f) * PictureWidth;
		float IncreasedHeight = (Ratio - 1.0f) * PictureHeight;

		// The new width and height
		PictureWidth = PictureWidth * Ratio;
		PictureHeight = PictureHeight * Ratio;

		// Adjust the X and Y offset
		XOffset = float(XOffset - (IncreasedWidth / 2.0));
		YOffset = float(YOffset - (IncreasedHeight / 2.0));

		/*
		INFO_LOG(WIIMOTE, "Crop           Ratio:%1.2f IncrWidth:%3.0f IncrHeight:%3.0f", Ratio, IncreasedWidth, IncreasedHeight);
		INFO_LOG(WIIMOTE, "Picture        Width:%4.1f Height:%4.1f YOffset:%4.0f XOffset:%4.0f", PictureWidth, PictureHeight, YOffset, XOffset);
		*/
	}
	
	// Return the mouse position as a fraction of one, inside the picture, with (0.0, 0.0) being the upper left corner of the picture
	x = ((float)point.x - XOffset) / PictureWidth;
	y = ((float)point.y - YOffset) / PictureHeight;
	
	/*
	INFO_LOG(WIIMOTE, "GetCursorPos:  %i %i", point.x, point.y);
	INFO_LOG(WIIMOTE, "GetClientRect: %i %i  %i %i", Rect.left, Rect.right, Rect.top, Rect.bottom);
	INFO_LOG(WIIMOTE, "Position       X:%1.2f Y:%1.2f", x, y);
	*/
	
#else
    // TODO fix on linux
	x = 0.5f;
	y = 0.5f;
#endif
}

/* This is not needed if we call FreeLibrary() when we stop a game, but if it's
   not called we need to reset these variables. */
void Shutdown() 
{
	INFO_LOG(WIIMOTE, "ShutDown");

	ResetVariables();
	// Close joypads
	Close_Devices();
	// Finally close SDL
	if (SDL_WasInit(0))
		SDL_Quit();
}

// Start emulation
void Initialize()
{
	INFO_LOG(WIIMOTE, "Initialize"); 

	// Reset variables
	ResetVariables();

	/* Populate joyinfo for all attached devices and do g_Config.Load() if the
	   configuration window is not already open, if it's already open we
	   continue with the settings we have */
	if(!g_SearchDeviceDone)
	{
		g_Config.Load();
		Search_Devices(joyinfo, NumPads, NumGoodPads);
		g_SearchDeviceDone = true;
	}

	// Write default Eeprom data to g_Eeprom[], this may be overwritten by
	// WiiMoteReal::Initialize() after this function.
	memset(g_Eeprom, 0, WIIMOTE_EEPROM_SIZE);
	memcpy(g_Eeprom, EepromData_0, sizeof(EepromData_0));
	memcpy(g_Eeprom + 0x16D0, EepromData_16D0, sizeof(EepromData_16D0));
	InitCalibration();

	// Copy extension id and calibration to its register, g_Config.Load() is needed before this
	for (int i = 0; i < MAX_WIIMOTES; i++)
		UpdateExtRegisterBlocks(i);

	// The emulated Wiimote is initialized
	g_EmulatedWiiMoteInitialized = true;

	// Load pre-recorded movements
	LoadRecordedMovements();

	/* The Nuncheck extension ID for homebrew applications that use the zero
	   key. This writes 0x0000 in encrypted form (0xfefe) to 0xfe in the
	   extension register. */
	//WriteCrypted16(g_RegExt, 0xfe, 0x0000); // Fully inserted Nunchuk

	// I forgot what these were for? Is this the zero key encrypted 0xa420?
	//	g_RegExt[0xfd] = 0x1e;
	//	g_RegExt[0xfc] = 0x9a;
}

// Set initial valuesm this done both in Init and Shutdown
void ResetVariables()
{
	g_EmulatedWiiMoteInitialized = false;

	g_ID = 0;
	g_Encryption = false;

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		g_ReportingAuto[i] = false;
		g_ReportingMode[i] = 0;
		g_ReportingChannel[i] = 0;
		WiiMapping[i].Motion.TiltWM.Shake = 0;
		WiiMapping[i].Motion.TiltWM.Roll = 0;
		WiiMapping[i].Motion.TiltWM.Pitch = 0;
		WiiMapping[i].Motion.TiltNC.Shake = 0;
		WiiMapping[i].Motion.TiltNC.Roll = 0;
		WiiMapping[i].Motion.TiltNC.Pitch = 0;
	}

	// Set default recording values
#if defined(HAVE_WX) && HAVE_WX
	for (int i = 0; i < 3; i++)
	{
		g_RecordingPlaying[i] = -1;
		g_RecordingCounter[i] = 0;
		g_RecordingPoint[i] = 0;
		g_RecordingStart[i] = 0;
		g_RecordingCurrentTime[i] = 0;
	}
#endif
}

// Initiate the accelerometer neutral values
void InitCalibration()
{
	g_wm.cal_zero.x = g_Eeprom[22];
	g_wm.cal_zero.y = g_Eeprom[23];
	g_wm.cal_zero.z = g_Eeprom[24];
	g_wm.cal_g.x = g_Eeprom[26] - g_Eeprom[22];
	g_wm.cal_g.y = g_Eeprom[27] - g_Eeprom[23];
	g_wm.cal_g.z = g_Eeprom[28] - g_Eeprom[24];

	g_nu.cal_zero.x = nunchuck_calibration[0x00];
	g_nu.cal_zero.y = nunchuck_calibration[0x01];
	g_nu.cal_zero.z = nunchuck_calibration[0x02];
	g_nu.cal_g.x = nunchuck_calibration[0x04] - nunchuck_calibration[0x00];
	g_nu.cal_g.y = nunchuck_calibration[0x05] - nunchuck_calibration[0x01];
	g_nu.cal_g.z = nunchuck_calibration[0x06] - nunchuck_calibration[0x02];

	g_nu.jx.max = nunchuck_calibration[0x08];
	g_nu.jx.min = nunchuck_calibration[0x09];
	g_nu.jx.center = nunchuck_calibration[0x0a];
	g_nu.jy.max = nunchuck_calibration[0x0b];
	g_nu.jy.min = nunchuck_calibration[0x0c];
	g_nu.jy.center = nunchuck_calibration[0x0d];

	g_ClassicContCalibration.Lx.max = classic_calibration[0x00];
	g_ClassicContCalibration.Lx.min = classic_calibration[0x01];
	g_ClassicContCalibration.Lx.center = classic_calibration[0x02];
	g_ClassicContCalibration.Ly.max = classic_calibration[0x03];
	g_ClassicContCalibration.Ly.min = classic_calibration[0x04];
	g_ClassicContCalibration.Ly.center = classic_calibration[0x05];

	g_ClassicContCalibration.Rx.max = classic_calibration[0x06];
	g_ClassicContCalibration.Rx.min = classic_calibration[0x07];
	g_ClassicContCalibration.Rx.center = classic_calibration[0x08];
	g_ClassicContCalibration.Ry.max = classic_calibration[0x09];
	g_ClassicContCalibration.Ry.min = classic_calibration[0x0a];
	g_ClassicContCalibration.Ry.center = classic_calibration[0x0b];

	g_ClassicContCalibration.Tl.neutral = classic_calibration[0x0c];
	g_ClassicContCalibration.Tr.neutral = classic_calibration[0x0d];

	// TODO get the correct values here
	g_GH3Calibration.Lx.max = classic_calibration[0x00];
	g_GH3Calibration.Lx.min = classic_calibration[0x01];
	g_GH3Calibration.Lx.center = classic_calibration[0x02];
	g_GH3Calibration.Ly.max = classic_calibration[0x03];
	g_GH3Calibration.Ly.min = classic_calibration[0x04];
	g_GH3Calibration.Ly.center = classic_calibration[0x05];
}

// Update the extension calibration values with our default values
void UpdateExtRegisterBlocks(int Slot)
{
	// Copy extension id and calibration to its register	
	if(WiiMapping[Slot].iExtensionConnected == EXT_NUNCHUCK)
	{
		memcpy(g_RegExt[Slot] + 0x20, nunchuck_calibration, sizeof(nunchuck_calibration));
		memcpy(g_RegExt[Slot] + 0x30, nunchuck_calibration, sizeof(nunchuck_calibration));
		memcpy(g_RegExt[Slot] + 0xfa, nunchuck_id, sizeof(nunchuck_id));
	}
	else if(WiiMapping[Slot].iExtensionConnected == EXT_CLASSIC_CONTROLLER)
	{
		memcpy(g_RegExt[Slot] + 0x20, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt[Slot] + 0x30, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt[Slot] + 0xfa, classic_id, sizeof(classic_id));
	}
	else if(WiiMapping[Slot].iExtensionConnected == EXT_GUITARHERO)
	{
		// TODO get the correct values here
		memcpy(g_RegExt[Slot] + 0x20, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt[Slot] + 0x30, classic_calibration, sizeof(classic_calibration));
		memcpy(g_RegExt[Slot] + 0xfa, gh3glp_id, sizeof(gh3glp_id));
	}

	INFO_LOG(WIIMOTE, "UpdateExtRegisterBlocks()");
}

void DoState(PointerWrap &p) 
{
	// TODO: Shorten the list
	p.Do(g_Speaker);
	p.Do(g_SpeakerVoice);
	p.DoArray(g_Eeprom, WIIMOTE_EEPROM_SIZE);
	p.DoArray(g_RegSpeaker, WIIMOTE_REG_SPEAKER_SIZE);
	p.DoArray(&g_RegExt[0][0], WIIMOTE_REG_EXT_SIZE * MAX_WIIMOTES);
	p.DoArray(g_RegMotionPlus, WIIMOTE_REG_EXT_SIZE);
	p.DoArray(g_RegExtTmp, WIIMOTE_REG_EXT_SIZE);
	p.DoArray(g_RegIr, WIIMOTE_REG_IR_SIZE);

	p.Do(g_Encryption);

	//p.Do(NumPads);
	//p.Do(NumGoodPads);
	//p.Do(joyinfo);
	//p.DoArray(PadState, 4);
	//p.DoArray(PadMapping, 4);
	//p.Do(g_Wiimote_kbd);
	//p.Do(g_NunchuckExt);
	//p.Do(g_ClassicContExt);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		p.Do(g_ReportingAuto[i]);
		p.Do(g_ReportingMode[i]);
		p.Do(g_ReportingChannel[i]);
		//p.Do(g_IR[i]);
		p.Do(g_Leds[i]);
		p.Do(g_ExtKey[i]);
	}
	return;
}

/* This function produce Wiimote Input, i.e. reports from the Wiimote in
   response to Output from the Wii. */
void InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size) 
{
	/* Debugging. We have not yet decided how much of 'data' we will use, it's
	   not determined by sizeof(data). We have to determine it by looking at
	   the data cases. */
	//InterruptDebugging(true, (const void*)_pData);

	g_ID = _number;

	hid_packet* hidp = (hid_packet*)_pData;

	INFO_LOG(WIIMOTE, "Emu InterruptChannel (page: %i, type: 0x%02x, param: 0x%02x)", _number, hidp->type, hidp->param);

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

					/* This is the 0x22 answer to all Inputs.*/

					// There are no 0x22 replys to these report from the real
					// wiimote from what I could see Report 0x10 that seems to
					// be only used for rumble, and we don't need to answer
					// that

					// The rumble report still needs more investigation
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

void ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size) 
{
	// Check for custom communication
	if(_channelID == 99 && *(const u8*)_pData == WIIMOTE_DISCONNECT)
	{
		WARN_LOG(WIIMOTE, "Wiimote: #%i Disconnected", _number);
		g_ReportingAuto[_number] = false;
		return;
	}
		
	g_ID = _number;

	hid_packet* hidp = (hid_packet*)_pData;

	INFO_LOG(WIIMOTE, "Emu ControlChannel (page: %i, type: 0x%02x, param: 0x%02x)", _number, hidp->type, hidp->param);

	switch(hidp->type)
	{
	case HID_TYPE_HANDSHAKE:
		PanicAlert("HID_TYPE_HANDSHAKE - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUPUT");
		break;

	case HID_TYPE_SET_REPORT:
		if (hidp->param == HID_PARAM_INPUT)
		{
			PanicAlert("HID_TYPE_SET_REPORT - INPUT"); 
		}
		else
		{
			// AyuanX: My experiment shows Control Channel is never used
			// In case it happens, we will send back a handshake which means report failed/rejected
			// (TO_BE_VERIFIED)
			//
			u8 handshake = HID_HANDSHAKE_SUCCESS;
			g_WiimoteInitialize.pWiimoteInput(g_ID, _channelID, &handshake, 1);

			PanicAlert("HID_TYPE_DATA - OUTPUT: Ambiguous Control Channel Report!");
		}
		break;

	case HID_TYPE_DATA:
		PanicAlert("HID_TYPE_DATA - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUTPUT");
		break;

	default:
		PanicAlert("HidControlChannel: Unknown type %x and param %x", hidp->type, hidp->param);
		break;
	}

}

/* This is called from Wiimote_Update(). See SystemTimers.cpp for a
   documentation. I'm not sure exactly how often this function is called but I
   think it's tied to the frame rate of the game rather than a certain amount
   of times per second. */
void Update(int _number) 
{
	if (g_ReportingAuto[_number] == false)
		return;

	g_ID = _number;

	// Read input or not
	if (WiiMapping[g_ID].Source == 1)
	{
		ReadLinuxKeyboard();

		// Check if the pad state should be updated
		if (NumGoodPads > 0 && joyinfo.size() > (u32)WiiMapping[g_ID].ID)
			UpdatePadState(WiiMapping[g_ID]);
	}

	switch(g_ReportingMode[g_ID])
	{
	case 0:
		break;
	case WM_REPORT_CORE:
		SendReportCore(g_ReportingChannel[g_ID]);
		break;
	case WM_REPORT_CORE_ACCEL:
		SendReportCoreAccel(g_ReportingChannel[g_ID]);
		break;
	case WM_REPORT_CORE_ACCEL_IR12:
		SendReportCoreAccelIr12(g_ReportingChannel[g_ID]);
		break;
	case WM_REPORT_CORE_ACCEL_EXT16:
		SendReportCoreAccelExt16(g_ReportingChannel[g_ID]);
		break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:
		SendReportCoreAccelIr10Ext(g_ReportingChannel[g_ID]);
		break;
	}
}


void ReadLinuxKeyboard()
{
#if defined(HAVE_X11) && HAVE_X11
	XEvent E;
	KeySym key;

	// keyboard input
	int num_events;
	for (num_events = XPending(WMdisplay); num_events > 0; num_events--)
	{
		XNextEvent(WMdisplay, &E);
		switch (E.type)
		{
		case KeyPress:
		{
			key = XLookupKeysym((XKeyEvent*)&E, 0);
			
			if ((key >= XK_F1 && key <= XK_F9) ||
			   key == XK_Shift_L || key == XK_Shift_R ||
			   key == XK_Control_L || key == XK_Control_R) {
				XPutBackEvent(WMdisplay, &E);
				break;
			}

			for (int i = 0; i < LAST_CONSTANT; i++)
			{
				if (key == WiiMapping[g_ID].Button[i])
					KeyStatus[i] = true;
			}
			break;
		}
		case KeyRelease:
		{
			key = XLookupKeysym((XKeyEvent*)&E, 0);
			
			if ((key >= XK_F1 && key <= XK_F9) ||
			   key == XK_Shift_L || key == XK_Shift_R ||
			   key == XK_Control_L || key == XK_Control_R) {
				XPutBackEvent(WMdisplay, &E);
				break;
			}

			for (int i = 0; i < LAST_CONSTANT; i++)
			{
				if (key == WiiMapping[g_ID].Button[i])
					KeyStatus[i] = false;
			}
			break;
		}
		default:
			break;
		}
	}
#endif
}

} // end of namespace
