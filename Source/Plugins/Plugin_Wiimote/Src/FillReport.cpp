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

#include <wx/msgdlg.h>

#include <vector>
#include <string>

#include "Common.h" // Common
#include "Timer.h"
#include "pluginspecs_wiimote.h"
#include "StringUtil.h" // For ArrayToString

#include "wiimote_hid.h"
#include "main.h"
#include "EmuMain.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Config.h" // For g_Config

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

// Recorded movements
// Variables: 0 = Wiimote, 1 = Nunchuck
int g_RecordingPlaying[3]; //g_RecordingPlaying[0] = -1; g_RecordingPlaying[1] = -1;
int g_RecordingCounter[3]; //g_RecordingCounter[0] = 0; g_RecordingCounter[1] = 0;
int g_RecordingPoint[3]; //g_RecordingPoint[0] = 0; g_RecordingPoint[1] = 0;
double g_RecordingStart[3]; //g_RecordingStart[0] = 0; g_RecordingStart[1] = 0;
double g_RecordingCurrentTime[3]; //g_RecordingCurrentTime[0] = 0; g_RecordingCurrentTime[1] = 0;

/* Convert from -350 to -3.5 g. The Nunchuck gravity size is 51 compared to the 26 to 28 for the Wiimote.
   So the maximum g values are higher for the Wiimote. */
int G2Accelerometer(int _G, int XYZ, int Wm)
{
	float G = (float)_G / 100.0;
	float Neutral, OneG, Accelerometer;

	switch(XYZ)
	{
	case 0: // X
		if(Wm == WM_RECORDING_WIIMOTE)
		{
			OneG = (float)g_wm.cal_g.x;
			Neutral = (float)g_wm.cal_zero.x;
		}
		else
		{
			OneG = (float)g_nu.cal_g.x;
			Neutral = (float)g_nu.cal_zero.x;
		}
		break;
	case 1: // Y
		if(Wm == WM_RECORDING_WIIMOTE)
		{
			OneG = (float)g_wm.cal_g.y;
			Neutral = (float)g_wm.cal_zero.y;
		}
		else
		{
			OneG = (float)g_nu.cal_g.y;
			Neutral = (float)g_nu.cal_zero.y;
		}
		break;
	case 2: // Z
		if(Wm == WM_RECORDING_WIIMOTE)
		{
			OneG = (float)g_wm.cal_g.z;
			Neutral = (float)g_wm.cal_zero.z;
		}
		else
		{
			OneG = (float)g_nu.cal_g.z;
			Neutral = (float)g_nu.cal_zero.z;
		}
		break;
	default: PanicAlert("There is a syntax error in a function that is calling G2Accelerometer(%i, %i)", _G, XYZ);
	}

	Accelerometer = Neutral + (OneG * G);
	int Return = (int)Accelerometer;

	// Logging
	//DEBUG_LOG(WIIMOTE, "G2Accelerometer():%f %f %f %f", Neutral, OneG, G, Accelerometer);

	// Boundaries
	if (Return > 255) Return = 255;
	if (Return < 0) Return = 0;

	return Return;
}

template<class IRReportType>
bool RecordingPlayAccIR(u8 &_x, u8 &_y, u8 &_z, IRReportType &_IR, int Wm)
{
	// Check if the recording is on
	if (g_RecordingPlaying[Wm] == -1) return false;

	// Return if the list is empty
	if(VRecording.at(g_RecordingPlaying[Wm]).Recording.size() == 0)
	{
		g_RecordingPlaying[Wm] = -1;
		DEBUG_LOG(WIIMOTE, "Empty");
		return false;
	}

	// Return if the playback speed is unset
	if(VRecording.at(g_RecordingPlaying[Wm]).PlaybackSpeed < 0)
	{
		DEBUG_LOG(WIIMOTE, "PlaybackSpeed empty: %i", g_RecordingPlaying[Wm]);
		g_RecordingPlaying[Wm] = -1;
		return false;
	}

	// Get IR bytes
	int IRBytes = VRecording.at(g_RecordingPlaying[Wm]).IRBytes;

	// Return if the IR mode is wrong
	if (Wm == WM_RECORDING_IR
		&& (   (IRBytes == 12 && !(g_ReportingMode[g_ID] == 0x33))
			|| (IRBytes == 10 && !(g_ReportingMode[g_ID] == 0x36 || g_ReportingMode[g_ID] == 0x37))
			)
		)
	{
		DEBUG_LOG(WIIMOTE, "Wrong IR mode: %i", g_RecordingPlaying[Wm]);
		g_RecordingPlaying[Wm] = -1;
		return false;
	}

	// Get starting time
	if(g_RecordingCounter[Wm] == 0)
	{
		DEBUG_LOG(WIIMOTE, "Begin: %i", Wm);
		g_RecordingStart[Wm] = Common::Timer::GetDoubleTime();
	}

	// Get current time
	g_RecordingCurrentTime[Wm] = Common::Timer::GetDoubleTime() - g_RecordingStart[Wm];

	// Modify the current time
	g_RecordingCurrentTime[Wm] *= ((25.0 + (double)VRecording.at(g_RecordingPlaying[Wm]).PlaybackSpeed * 25.0) / 100.0);

	// Select reading
	for (int i = 0; i < (int)VRecording.at(g_RecordingPlaying[Wm]).Recording.size(); i++)
		if (VRecording.at(g_RecordingPlaying[Wm]).Recording.at(i).Time > g_RecordingCurrentTime[Wm])
		{
			g_RecordingPoint[Wm] = i;
			break; // Break loop
		}

	// Return if we are at the end of the list
	if(g_RecordingCurrentTime[Wm] >=
		VRecording.at(g_RecordingPlaying[Wm]).Recording.at(
			VRecording.at(g_RecordingPlaying[Wm]).Recording.size() - 1).Time)
	// Or if we are playing back all observations regardless of time
	//g_RecordingPoint[Wm] = g_RecordingCounter[Wm];
	//if (g_RecordingPoint[Wm] >= VRecording.at(g_RecordingPlaying[Wm]).Recording.size())
	{
		g_RecordingCounter[Wm] = 0;
		g_RecordingPlaying[Wm] = -1;
		g_RecordingStart[Wm] = 0;
		g_RecordingCurrentTime[Wm] = 0;
		DEBUG_LOG(WIIMOTE, "End: %i", Wm);
		return false;
	}

	// Update accelerometer values
	_x = G2Accelerometer(VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).x, 0, Wm);
	_y = G2Accelerometer(VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).y, 1, Wm);
	_z = G2Accelerometer(VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).z, 2, Wm);
	// Update IR values
	if(Wm == WM_RECORDING_IR) memcpy(&_IR, VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).IR, IRBytes);

	if (g_DebugAccelerometer)
	{
		//Console::ClearScreen();
		DEBUG_LOG(WIIMOTE, "Current time: [%i / %i]  %f %f",
			g_RecordingPoint[Wm], VRecording.at(g_RecordingPlaying[Wm]).Recording.size(),
			VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).Time, g_RecordingCurrentTime[Wm]
			);
		DEBUG_LOG(WIIMOTE, "Accel x, y, z: %03u %03u %03u", _x, _y, _z);
	}
	//DEBUG_LOG(WIIMOTE, "Accel x, y, z: %03u %03u %03u", _x, _y, _z);

	g_RecordingCounter[Wm]++;

	return true;
}

/* Because the playback is neatly controlled by RecordingPlayAccIR() we use these functions to be able to
   use RecordingPlayAccIR() for both accelerometer and IR recordings */
bool RecordingPlay(u8 &_x, u8 &_y, u8 &_z, int Wm)
{
	wm_ir_basic IR;
	return RecordingPlayAccIR(_x, _y, _z, IR, Wm);
}

template<class IRReportType>
bool RecordingPlayIR(IRReportType &_IR)
{
	u8 x, y, z;
	return RecordingPlayAccIR(x, y, z, _IR, 2);
}

// Return true if this particual numerical key is pressed
bool IsNumericalKeyPressed(int _Key)
{
#ifdef _WIN32
	// Check which key it is
	std::string TmpKey = StringFromFormat("%i", _Key);
	if(GetAsyncKeyState(TmpKey[0]))
		return true;
	else
		// That numerical key is pressed
		return false;
#else
	// TODO linux port
	return false;
#endif
}

// Check if a switch is pressed
bool IsSwitchPressed(int _Key)
{
#ifdef _WIN32
	// Check if that switch is pressed
	switch (_Key)
	{
		case 0: if (GetAsyncKeyState(VK_SHIFT)) return true;
		case 1: if (GetAsyncKeyState(VK_CONTROL)) return true;
		case 2: if (GetAsyncKeyState(VK_MENU)) return true;
	}

	// That switch was not pressed
	return false;
#else
	// TODO linux port
	return false;
#endif
}

// Check if we should start the playback of a recording. Once it has been started it can currently
// not be stopped, it will always run to the end of the recording.
int RecordingCheckKeys(int WmNuIr)
{
#ifdef _WIN32
	//DEBUG_LOG(WIIMOTE, "RecordingCheckKeys: %i", Wiimote);

	// Check if we have a HotKey match
	bool Match = false;
	int Recording = -1;
	for(int i = 0; i < RECORDING_ROWS; i++)
	{
		// Check all ten numerical keys
		for(int j = 0; j < 10; j++)
		{
			if ((VRecording.at(i).HotKeyWiimote == j && WmNuIr == 0 && IsNumericalKeyPressed(j)
				|| VRecording.at(i).HotKeyNunchuck == j && WmNuIr == 1 && IsNumericalKeyPressed(j)
				|| VRecording.at(i).HotKeyIR == j && WmNuIr == 2 && IsNumericalKeyPressed(j))
				&& (IsSwitchPressed(VRecording.at(i).HotKeySwitch) || VRecording.at(i).HotKeySwitch == 3))
			{
				//DEBUG_LOG(WIIMOTE, "Match: %i %i", i, Key);
				Match = true;
				Recording = i;
				break;
			}
		}
	}

	// Return nothing if we don't have a match
	if (!Match) return -1;

	// Return the match
	return Recording;
#else
	return -1;
#endif
}


// Multi System Input Status Check
bool IsKey(int Key)
{
	int Ret = NULL;

	if (WiiMapping[g_ID].Source == 1)
	{
		int MapKey = WiiMapping[g_ID].Button[Key];

		if (MapKey < 256)
		{
#ifdef _WIN32
			Ret = GetAsyncKeyState(MapKey);		// Keyboard (Windows)
#else
			Ret = KeyStatus[MapKey];			// Keyboard (Linux)
#endif
		}
		else if (MapKey < 0x1100)
		{
			Ret = SDL_JoystickGetButton(WiiMapping[g_ID].joy, MapKey - 0x1000);	// Pad button
		}
		else	// Pad hat
		{
			u8 HatCode, HatKey;
			HatCode = SDL_JoystickGetHat(WiiMapping[g_ID].joy, (MapKey - 0x1100) / 0x0010);
			HatKey = (MapKey - 0x1100) % 0x0010;

			if (HatCode & HatKey)
				Ret = HatKey;
		}

#ifdef _WIN32
		if ((Key == EWM_SHAKE && GetAsyncKeyState(VK_MBUTTON))
			|| (Key == EWM_A && GetAsyncKeyState(VK_LBUTTON))
			|| (Key == EWM_B && GetAsyncKeyState(VK_RBUTTON)))
		{
			float x, y;
			GetMousePos(x, y);
			Ret = !(x < 0 || x > 1 || y < 0 || y > 1);
		}
#endif

	}

	return (Ret) ? true : false;
}

// Wiimote core buttons
void FillReportInfo(wm_core& _core)
{
	// Check that Dolphin is in focus
	if (!IsFocus()) return;

	// Allow both mouse buttons and keyboard to press a and b
	_core.a = IsKey(EWM_A);
	_core.b = IsKey(EWM_B);
	_core.one = IsKey(EWM_ONE);
	_core.two = IsKey(EWM_TWO);
	_core.plus = IsKey(EWM_P);
	_core.minus = IsKey(EWM_M);
	_core.home = IsKey(EWM_H);

	/* Sideways controls (for example for Wario Land) if the Wiimote is intended to be held sideways */
	if(WiiMapping[g_ID].bSideways)
	{
		_core.left = IsKey(EWM_D);
		_core.up = IsKey(EWM_L);
		_core.right = IsKey(EWM_U);
		_core.down = IsKey(EWM_R);
	}
	else
	{
		_core.left = IsKey(EWM_L);
		_core.up = IsKey(EWM_U);
		_core.right = IsKey(EWM_R);
		_core.down = IsKey(EWM_D);
	}
}

void FillReportAcc(wm_accel& _acc)
{
	// Recorded movements
	// Check for a playback command
	if (g_RecordingPlaying[0] < 0)
	{
		g_RecordingPlaying[0] = RecordingCheckKeys(0);
	}
	else
	{
		// If the recording reached the end or failed somehow we will not return
		if (RecordingPlay(_acc.x, _acc.y, _acc.z, 0))
			return;
		//DEBUG_LOG(WIIMOTE, "X, Y, Z: %u %u %u", _acc.x, _acc.y, _acc.z);
	}

	// Initial value
	_acc.x = g_wm.cal_zero.x;
	_acc.y = g_wm.cal_zero.y;
	_acc.z = g_wm.cal_zero.z;

	// Adjust position, also add some noise to prevent disconnection
	if (!WiiMapping[g_ID].bUpright)
		_acc.z += g_wm.cal_g.z + WiiMapping[g_ID].Motion.TiltWM.FakeNoise;
	else	// Upright wiimote
		_acc.y -= g_wm.cal_g.y + WiiMapping[g_ID].Motion.TiltWM.FakeNoise;

	WiiMapping[g_ID].Motion.TiltWM.FakeNoise = -WiiMapping[g_ID].Motion.TiltWM.FakeNoise;

	if (IsFocus())
	{
		int acc_x = _acc.x;
		int acc_y = _acc.y;
		int acc_z = _acc.z;

		if (IsKey(EWM_SHAKE) && !WiiMapping[g_ID].Motion.TiltWM.Shake)
			WiiMapping[g_ID].Motion.TiltWM.Shake = 1;

		// Step the shake simulation one step
		ShakeToAccelerometer(acc_x, acc_y, acc_z, WiiMapping[g_ID].Motion.TiltWM);

		// Tilt Wiimote, allow the shake function to interrupt it
		if (!WiiMapping[g_ID].Motion.TiltWM.Shake)
			TiltWiimote(acc_x, acc_y, acc_z);

		// Boundary check
		if (acc_x > 0xFF)	acc_x = 0xFF;
		else if (acc_x < 0x00)	acc_x = 0x00;
		if (acc_y > 0xFF)	acc_y = 0xFF;
		else if (acc_y < 0x00)	acc_y = 0x00;
		if (acc_z > 0xFF)	acc_z = 0xFF;
		else if (acc_z < 0x00)	acc_z = 0x00;

		_acc.x = acc_x;
		_acc.y = acc_y;
		_acc.z = acc_z;
	}

	// Debugging for translating Wiimote to Keyboard (or Gamepad)
	/*
	
	// Toogle console display
	if(GetAsyncKeyState('U'))
	{
		if(consoleDisplay < 2)
			consoleDisplay ++;
		else
			consoleDisplay = 0;
	}

	if(GetAsyncKeyState('5'))
		A-=1;
	else if(GetAsyncKeyState('6'))
		A+=1;
	if(GetAsyncKeyState('7'))
		B-=1;
	else if(GetAsyncKeyState('8'))
		B+=1;
	if(GetAsyncKeyState('9'))
		C-=1;
	else if(GetAsyncKeyState('0'))
		C+=1;

	else if(GetAsyncKeyState(VK_NUMPAD3))
		d-=1;
	else if(GetAsyncKeyState(VK_NUMPAD6))
		d+=1;
	else if(GetAsyncKeyState(VK_ADD))
		yhistsize-=1;
	else if(GetAsyncKeyState(VK_SUBTRACT))
		yhistsize+=1;

	
	if(GetAsyncKeyState(VK_INSERT))
		AX-=1;
	else if(GetAsyncKeyState(VK_DELETE))
		AX+=1;
	else if(GetAsyncKeyState(VK_HOME))
		AY-=1;
	else if(GetAsyncKeyState(VK_END))
		AY+=1;
	else if(GetAsyncKeyState(VK_SHIFT))
		AZ-=1;
	else if(GetAsyncKeyState(VK_CONTROL))
		AZ+=1;

	if(GetAsyncKeyState(VK_NUMPAD1))
		X+=1;
	else if(GetAsyncKeyState(VK_NUMPAD2))
		X-=1;
	if(GetAsyncKeyState(VK_NUMPAD4))
		Y+=1;
	else if(GetAsyncKeyState(VK_NUMPAD5))
		Y-=1;
	if(GetAsyncKeyState(VK_NUMPAD7))
		Z+=1;
	else if(GetAsyncKeyState(VK_NUMPAD8))
		Z-=1;

	//if(consoleDisplay == 0)
	DEBUG_LOG(WIIMOTE, "x: %03i | y: %03i | z: %03i  |  A:%i B:%i C:%i  a:%i b:%i c:%i d:%i  X:%i Y:%i Z:%i",
		_acc.x, _acc.y, _acc.z,
		A, B, C,
		a, b, c, d,
		X, Y, Z
		);
	DEBUG_LOG(WIIMOTE, "x: %03i | y: %03i | z: %03i  |  X:%i Y:%i Z:%i  | AX:%i AY:%i AZ:%i ",
		_acc.x, _acc.y, _acc.z,
		X, Y, Z,
		AX, AY, AZ
		);*/	
}

// The extended 12 byte (3 byte per object) reporting
void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1)
{
	// Recorded movements
	// Check for a playback command
	if(g_RecordingPlaying[2] < 0)
	{
		g_RecordingPlaying[2] = RecordingCheckKeys(2);
	}
	else
	{
		//DEBUG_LOG(WIIMOTE, "X, Y, Z: %u %u %u", _acc.x, _acc.y, _acc.z);
		if (RecordingPlayIR(_ir0)) return;		
	}

	/* Fill with 0xff if empty. The real Wiimote seems to use 0xff when it
	   doesn't see a certain point, at least from how WiiMoteReal::SendEvent()
	   works. */
	memset(&_ir0, 0xff, sizeof(wm_ir_extended));
	memset(&_ir1, 0xff, sizeof(wm_ir_extended));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	// If we are outside the screen leave the values at 0xff
	if(MouseX > 1 || MouseX < 0 || MouseY > 1 || MouseY < 0) return;

	// Position calculation
	int y0 = g_Config.iIRTop + g_Config.iIRHeight * MouseY;
	int y1 = y0;

	// The distance between the x positions are two sensor bar radii
	int x0 = 1023 - g_Config.iIRLeft - g_Config.iIRWidth * MouseX - SENSOR_BAR_WIDTH / 2;
	int x1 = x0 + SENSOR_BAR_WIDTH;

	RotateIRDot(x0, y0, WiiMapping[g_ID].Motion.TiltWM);
	RotateIRDot(x1, y1, WiiMapping[g_ID].Motion.TiltWM);

	// Converted to IR data
	_ir0.x = x0 & 0xff; _ir0.xHi = x0 >> 8;
	_ir0.y = y0 & 0xff; _ir0.yHi = y0 >> 8;

	_ir1.x = x1 & 0xff; _ir1.xHi = x1 >> 8;
	_ir1.y = y1 & 0xff; _ir1.yHi = y1 >> 8;

	// The size can be between 0 and 15 and is probably not important
	_ir0.size = 10;
	_ir1.size = 10;

	// Debugging for calibration
	/*
	if(!GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_RIGHT))
		Right +=1;
	else if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_RIGHT))
		Right -=1;
	if(!GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_LEFT))
		Left +=1;
	else if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_LEFT))
		Left -=1;
	if(!GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_UP))
		Top += 1;
	else if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_UP))
		Top -= 1;
	if(!GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_DOWN))
		Bottom += 1;
	else if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_DOWN))
		Bottom -= 1;
	if(!GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_NUMPAD0))
		SensorBarRadius += 1;
	else if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_NUMPAD0))
		SensorBarRadius -= 1;

	//Console::ClearScreen();
	//if(consoleDisplay == 1)
	DEBUG_LOG(WIIMOTE, "x0:%03i x1:%03i  y0:%03i y1:%03i | T:%i L:%i R:%i B:%i S:%i",
		x0, x1, y0, y1, Top, Left, Right, Bottom, SensorBarRadius
		);*/
}

// The 10 byte reporting used when an extension is connected
void FillReportIRBasic(wm_ir_basic& _ir0, wm_ir_basic& _ir1)
{
	// Recorded movements
	// Check for a playback command
	if(g_RecordingPlaying[2] < 0)
	{
		g_RecordingPlaying[2] = RecordingCheckKeys(2);
	}
	// We are playing back a recording, we don't accept any manual input this time
	else
	{
		//DEBUG_LOG(WIIMOTE, "X, Y, Z: %u %u %u", _acc.x, _acc.y, _acc.z);
		if (RecordingPlayIR(_ir0)) return;		
	}

	// Fill with 0xff if empty
	memset(&_ir0, 0xff, sizeof(wm_ir_basic));
	memset(&_ir1, 0xff, sizeof(wm_ir_basic));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	// If we are outside the screen leave the values at 0xff
	if(MouseX > 1 || MouseX < 0 || MouseY > 1 || MouseY < 0) return;

	int y1 = g_Config.iIRTop + g_Config.iIRHeight * MouseY;
	int y2 = y1;

	int x1 = 1023 - g_Config.iIRLeft - g_Config.iIRWidth * MouseX - SENSOR_BAR_WIDTH / 2;
	int x2 = x1 + SENSOR_BAR_WIDTH;

	RotateIRDot(x1, y1, WiiMapping[g_ID].Motion.TiltWM);
	RotateIRDot(x2, y2, WiiMapping[g_ID].Motion.TiltWM);

	/* As with the extented report we settle with emulating two out of four
	   possible objects the only difference is that we don't report any size of
	   the tracked object here */

	_ir0.x1 = x1 & 0xff; _ir0.x1Hi = (x1 >> 8); // we are dealing with 2 bit values here
	_ir0.y1 = y1 & 0xff; _ir0.y1Hi = (y1 >> 8);

	_ir0.x2 = x2 & 0xff; _ir0.x2Hi = (x2 >> 8);
	_ir0.y2 = y2 & 0xff; _ir0.y2Hi = (y2 >> 8);

	// Debugging for calibration
	/*
	if(GetAsyncKeyState(VK_NUMPAD1))
		Right +=1;
	else if(GetAsyncKeyState(VK_NUMPAD2))
		Right -=1;
	if(GetAsyncKeyState(VK_NUMPAD4))
		Left +=1;
	else if(GetAsyncKeyState(VK_NUMPAD5))
		Left -=1;
	if(GetAsyncKeyState(VK_NUMPAD7))
		Top += 1;
	else if(GetAsyncKeyState(VK_NUMPAD8))
		Top -= 1;
	if(GetAsyncKeyState(VK_NUMPAD6))
		Bottom += 1;
	else if(GetAsyncKeyState(VK_NUMPAD3))
		Bottom -= 1;
	if(GetAsyncKeyState(VK_INSERT))
		SensorBarRadius += 1;
	else if(GetAsyncKeyState(VK_DELETE))
		SensorBarRadius -= 1;

	//ClearScreen();
	//if(consoleDisplay == 1)
		
		DEBUG_LOG(WIIMOTE, "x1:%03i x2:%03i  y1:%03i y2:%03i   irx1:%02x y1:%02x  x2:%02x y2:%02x | T:%i L:%i R:%i B:%i S:%i",
		x1, x2, y1, y2, _ir0.x1, _ir0.y1, _ir1.x2, _ir1.y2, Top, Left, Right, Bottom, SensorBarRadius
		);
		DEBUG_LOG(WIIMOTE, "");
		DEBUG_LOG(WIIMOTE, "ir0.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir0.y1:%02x y1h:%02x y2:%02x y2h:%02x  |  ir1.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir1.y1:%02x y1h:%02x y2:%02x y2h:%02x",
		_ir0.x1, _ir0.x1Hi, _ir0.x2, _ir0.x2Hi,
		_ir0.y1, _ir0.y1Hi, _ir0.y2, _ir0.y2Hi,
		_ir1.x1, _ir1.x1Hi, _ir1.x2, _ir1.x2Hi,
		_ir1.y1, _ir1.y1Hi, _ir1.y2, _ir1.y2Hi
		);*/
	// ------------------
}


/* Generate the 6 byte extension report for the Nunchuck, encrypted. The bytes
   are JX JY AX AY AZ BT. */
void FillReportExtension(wm_extension& _ext)
{
	// Recorded movements
	// Check for a playback command
	if(g_RecordingPlaying[1] < 0)
	{
		g_RecordingPlaying[1] = RecordingCheckKeys(1);
	}
	else
	{
		// We should not play back the accelerometer values
		if (RecordingPlay(_ext.ax, _ext.ay, _ext.az, 1))
			return;
	}

	// The default joystick and button values unless we use them
	_ext.jx = g_nu.jx.center;
	_ext.jy = g_nu.jy.center;
	_ext.bt = 0x03; // 0x03 means no button pressed, the button is zero active

	// Use the neutral values
	_ext.ax = g_nu.cal_zero.x;
	_ext.ay = g_nu.cal_zero.y;
	_ext.az = g_nu.cal_zero.z + g_nu.cal_g.z;

	if (IsFocus())
	{
		int acc_x = _ext.ax;
		int acc_y = _ext.ay;
		int acc_z = _ext.az;

		if (IsKey(ENC_SHAKE) && !WiiMapping[g_ID].Motion.TiltNC.Shake)
			WiiMapping[g_ID].Motion.TiltNC.Shake = 1;

		// Step the shake simulation one step
		ShakeToAccelerometer(acc_x, acc_y, acc_z, WiiMapping[g_ID].Motion.TiltNC);

		// Tilt Nunchuck, allow the shake function to interrupt it
		if (!WiiMapping[g_ID].Motion.TiltNC.Shake)
			TiltNunchuck(acc_x, acc_y, acc_z);

		// Boundary check
		if (acc_x > 0xFF)	acc_x = 0xFF;
		else if (acc_x < 0x00)	acc_x = 0x00;
		if (acc_y > 0xFF)	acc_y = 0xFF;
		else if (acc_y < 0x00)	acc_y = 0x00;
		if (acc_z > 0xFF)	acc_z = 0xFF;
		else if (acc_z < 0x00)	acc_z = 0x00;

		_ext.ax = acc_x;
		_ext.ay = acc_y;
		_ext.az = acc_z;

		// Update the analog stick
		if (WiiMapping[g_ID].Stick.NC == FROM_KEYBOARD)
		{
			// Set the max values to the current calibration values
			if(IsKey(ENC_L)) // x
				_ext.jx = g_nu.jx.min;
			if(IsKey(ENC_R))
				_ext.jx = g_nu.jx.max;

			if(IsKey(ENC_D)) // y
				_ext.jy = g_nu.jy.min;
			if(IsKey(ENC_U))
				_ext.jy = g_nu.jy.max;

			// On a real stick, the initialization value of center is 0x80,
			// but after a first time touch, the center value automatically changes to 0x7F
			if(_ext.jx != g_nu.jx.center)
				g_nu.jx.center = 0x7F;
			if(_ext.jy != g_nu.jy.center)
				g_nu.jy.center = 0x7F;
		}
		else
		{
			// Get adjusted pad state values
			int _Lx = WiiMapping[g_ID].AxisState.Lx;
			int _Ly = WiiMapping[g_ID].AxisState.Ly;
			int _Rx = WiiMapping[g_ID].AxisState.Rx;
			int _Ry = WiiMapping[g_ID].AxisState.Ry;

			// The Y-axis is inverted
			_Ly = 0xff - _Ly;
			_Ry = 0xff - _Ry;

			/* This is if we are also using a real Nunchuck that we are sharing the
			calibration with. It's not needed if we are using our default
			values. We adjust the values to the configured range, we even allow
			the center to not be 0x80. */
			if(g_nu.jx.max != 0xff || g_nu.jy.max != 0xff
				|| g_nu.jx.min != 0 || g_nu.jy.min != 0
				|| g_nu.jx.center != 0x80 || g_nu.jy.center != 0x80)
			{
				float Lx = (float)_Lx;
				float Ly = (float)_Ly;
				float Rx = (float)_Rx;
				float Ry = (float)_Ry;
				//float Tl = (float)_Tl;
				//float Tr = (float)_Tr;

				float XRangePos = (float) (g_nu.jx.max - g_nu.jx.center);
				float XRangeNeg = (float) (g_nu.jx.center - g_nu.jx.min);
				float YRangePos = (float) (g_nu.jy.max - g_nu.jy.center);
				float YRangeNeg = (float) (g_nu.jy.center - g_nu.jy.min);
				if (Lx > 0x80) Lx = Lx * (XRangePos / 128.0);
				if (Lx < 0x80) Lx = Lx * (XRangeNeg / 128.0);
				if (Lx == 0x80) Lx = (float)g_nu.jx.center;
				if (Ly > 0x80) Ly = Ly * (YRangePos / 128.0);
				if (Ly < 0x80) Ly = Ly * (YRangeNeg / 128.0);
				if (Ly == 0x80) Lx = (float)g_nu.jy.center;
				// Boundaries
				_Lx = (int)Lx;
				_Ly = (int)Ly;
				_Rx = (int)Rx;
				_Ry = (int)Ry;
				if (_Lx > 0xff) _Lx = 0xff; if (_Lx < 0) _Lx = 0;
				if (_Rx > 0xff) _Rx = 0xff; if (_Rx < 0) _Rx = 0;
				if (_Ly > 0xff) _Ly = 0xff; if (_Ly < 0) _Ly = 0;
				if (_Ry > 0xff) _Ry = 0xff; if (_Ry < 0) _Ry = 0;
			}

			if (WiiMapping[g_ID].Stick.NC == FROM_ANALOG1)
			{
				_ext.jx = _Lx;
				_ext.jy = _Ly;
			}
			else // ANALOG2
			{
				_ext.jx = _Rx;
				_ext.jy = _Ry;
			}
		}

		if(IsKey(ENC_C)) _ext.bt &= ~0x02;
		if(IsKey(ENC_Z)) _ext.bt &= ~0x01;
	}

	/* Here we encrypt the report */

	// Create a temporary storage for the data
	u8 Tmp[sizeof(_ext)];
	// Clear the array by copying zeroes to it
	memset(Tmp, 0, sizeof(_ext));
	// Copy the data to it
	memcpy(Tmp, &_ext, sizeof(_ext));
	// Encrypt it
	wiimote_encrypt(&g_ExtKey[g_ID], Tmp, 0x00, sizeof(_ext));
	// Write it back to the struct
	memcpy(&_ext, Tmp, sizeof(_ext));
}


/* Generate the 6 byte extension report for the Classic Controller, encrypted.
   The bytes are ... */
void FillReportClassicExtension(wm_classic_extension& _ext)
{
	/* These are the default neutral values for the analog triggers and sticks */
	u8	Rx = g_ClassicContCalibration.Rx.center, Ry = g_ClassicContCalibration.Ry.center,
		Lx = g_ClassicContCalibration.Lx.center, Ly = g_ClassicContCalibration.Ly.center,
		lT = g_ClassicContCalibration.Tl.neutral, rT = g_ClassicContCalibration.Tl.neutral;

	_ext.b1.padding = 0x01; // 0x01 means not pressed
	_ext.b1.bRT = 0x01;
	_ext.b1.bP = 0x01;
	_ext.b1.bH = 0x01;
	_ext.b1.bM = 0x01;
	_ext.b1.bLT = 0x01;
	_ext.b1.bdD = 0x01;
	_ext.b1.bdR = 0x01;

	_ext.b2.bdU = 0x01;
	_ext.b2.bdL = 0x01;
	_ext.b2.bZR = 0x01;
	_ext.b2.bX = 0x01;
	_ext.b2.bA = 0x01;
	_ext.b2.bY = 0x01;
	_ext.b2.bB = 0x01;
	_ext.b2.bZL = 0x01;

	// Check that Dolphin is in focus
	if (IsFocus())
	{
		/* Left and right analog sticks and analog triggers

			u8 Lx : 6; // byte 0
			u8 Rx : 2;
			u8 Ly : 6; // byte 1
			u8 Rx2 : 2;
			u8 Ry : 5; // byte 2
			u8 lT : 2;
			u8 Rx3 : 1;
			u8 rT : 5; // byte 3
			u8 lT2 : 3;

		   We use a 200 range (28 to 228) for the left analog stick and a 176 range
		   (40 to 216) for the right analog stick to match our calibration values
		   in classic_calibration
		*/

		// Update the left analog stick
		if (WiiMapping[g_ID].Stick.CCL == FROM_KEYBOARD)
		{
			if(IsKey(ECC_Ll)) // Left analog left
				Lx = g_ClassicContCalibration.Lx.min;
			if(IsKey(ECC_Lr)) // right
				Lx = g_ClassicContCalibration.Lx.max;
			if(IsKey(ECC_Lu)) // up
				Ly = g_ClassicContCalibration.Ly.max;
			if(IsKey(ECC_Ld)) // down
				Ly = g_ClassicContCalibration.Ly.min;

			// On a real stick, the initialization value of center is 0x80,
			// but after a first time touch, the center value automatically changes to 0x7F
			if(Lx != g_ClassicContCalibration.Lx.center)
				g_ClassicContCalibration.Lx.center = 0x7F;
			if(Ly != g_ClassicContCalibration.Ly.center)
				g_ClassicContCalibration.Ly.center = 0x7F;
		}
		else
		{
			// Get adjusted pad state values
			int _Lx = WiiMapping[g_ID].AxisState.Lx;
			int _Ly = WiiMapping[g_ID].AxisState.Ly;
			int _Rx = WiiMapping[g_ID].AxisState.Rx;
			int _Ry = WiiMapping[g_ID].AxisState.Ry;

			// The Y-axis is inverted
			_Ly = 0xff - _Ly;
			_Ry = 0xff - _Ry;

			/* This is if we are also using a real Classic Controller that we
			   are sharing the calibration with.  It's not needed if we are
			   using our default values. We adjust the values to the configured
			   range.
			   
			   Status: Not added, we are not currently sharing the calibration
			   with the real Classic Controller
			   */

			if (WiiMapping[g_ID].Stick.CCL == FROM_ANALOG1)
			{
				Lx = _Lx;
				Ly = _Ly;
			}
			else // ANALOG2
			{
				Lx = _Rx;
				Ly = _Ry;
			}
		}

		// Update the right analog stick
		if (WiiMapping[g_ID].Stick.CCR == FROM_KEYBOARD)
		{
			if(IsKey(ECC_Rl)) // Right analog left
				Rx = g_ClassicContCalibration.Rx.min;
			if(IsKey(ECC_Rr)) // right
				Rx = g_ClassicContCalibration.Rx.max;
			if(IsKey(ECC_Ru)) // up
				Ry = g_ClassicContCalibration.Ry.max;
			if(IsKey(ECC_Rd)) // down
				Ry = g_ClassicContCalibration.Ry.min;

			// On a real stick, the initialization value of center is 0x80,
			// but after a first time touch, the center value automatically changes to 0x7F
			if(Rx != g_ClassicContCalibration.Rx.center)
				g_ClassicContCalibration.Rx.center = 0x7F;
			if(Ry != g_ClassicContCalibration.Ry.center)
				g_ClassicContCalibration.Ry.center = 0x7F;
		}
		else
		{
			// Get adjusted pad state values
			int _Lx = WiiMapping[g_ID].AxisState.Lx;
			int _Ly = WiiMapping[g_ID].AxisState.Ly;
			int _Rx = WiiMapping[g_ID].AxisState.Rx;
			int _Ry = WiiMapping[g_ID].AxisState.Ry;

			// The Y-axis is inverted
			_Ly = 0xff - _Ly;
			_Ry = 0xff - _Ry;

			/* This is if we are also using a real Classic Controller that we
			   are sharing the calibration with.  It's not needed if we are
			   using our default values. We adjust the values to the configured
			   range.
			   
			   Status: Not added, we are not currently sharing the calibration
			   with the real Classic Controller
			   */

			if (WiiMapping[g_ID].Stick.CCR == FROM_ANALOG1)
			{
				Rx = _Lx;
				Ry = _Ly;
			}
			else // ANALOG2
			{
				Rx = _Rx;
				Ry = _Ry;
			}
		}

		// Update the left and right analog triggers
		if (WiiMapping[g_ID].Stick.CCT == FROM_KEYBOARD)
		{
			if(IsKey(ECC_Tl)) // analog left trigger
				{ _ext.b1.bLT = 0x00; lT = 0x1f; }
			if(IsKey(ECC_Tr)) // analog right trigger
				{ _ext.b1.bRT = 0x00; rT = 0x1f; }
		}
		else // g_Config.ClassicController.TRIGGER
		{
			// Get adjusted pad state values
			int _Tl = WiiMapping[g_ID].AxisState.Tl;
			int _Tr = WiiMapping[g_ID].AxisState.Tr;

			/* This is if we are also using a real Classic Controller that we
			   are sharing the calibration with.  It's not needed if we are
			   using our default values. We adjust the values to the configured
			   range.
			   
			   Status: Not added, we are not currently sharing the calibration
			   with the real Classic Controller
			   */

			// Check if the trigger is fully pressed, then update the digital
			// trigger values to
			if (_Tl == 0xff)  _ext.b1.bLT = 0x00;
			if (_Tr == 0xff)  _ext.b1.bRT = 0x00;

			// These can be copied directly, the bitshift further down fix this
			// value to
			lT = _Tl;
			rT = _Tr;
		}

		/* D-Pad
		
		u8 b1;
			0:
			6: bdD
			7: bdR

		u8 b2;
			0: bdU
			1: bdL	
		*/
		if(IsKey(ECC_Dl)) _ext.b2.bdL = 0x00; // Digital left
		if(IsKey(ECC_Du)) _ext.b2.bdU = 0x00; // Up
		if(IsKey(ECC_Dr)) _ext.b1.bdR = 0x00; // Right
		if(IsKey(ECC_Dd)) _ext.b1.bdD = 0x00; // Down		

		/* Buttons
			u8 b1;
				0:
				6: -
				7: -

			u8 b2;
				0: -
				1: -
				2: bZr
				3: bX
				4: bA
				5: bY
				6: bB
				7: bZl
		*/
		if(IsKey(ECC_A)) _ext.b2.bA = 0x00;
		if(IsKey(ECC_B)) _ext.b2.bB = 0x00;
		if(IsKey(ECC_Y)) _ext.b2.bY = 0x00;
		if(IsKey(ECC_X)) _ext.b2.bX = 0x00;
		if(IsKey(ECC_P)) _ext.b1.bP = 0x00;
		if(IsKey(ECC_M)) _ext.b1.bM = 0x00;
		if(IsKey(ECC_H)) _ext.b1.bH = 0x00;
		if(IsKey(ECC_Zl)) _ext.b2.bZL = 0x00;
		if(IsKey(ECC_Zr)) _ext.b2.bZR = 0x00;
		
		// All buttons pressed
		//if(GetAsyncKeyState('C') && GetAsyncKeyState('Z'))
		//	{ _ext.b2.bA = 0x01; _ext.b2.bB = 0x01; }
	}

	// Convert data for reporting
	_ext.Lx = (Lx >> 2);
	_ext.Ly = (Ly >> 2);
	// 5 bit to 1 bit
	_ext.Rx = (Rx >> 3) & 0x01;
	// 5 bit to the next 2 bit
	_ext.Rx2 = ((Rx >> 3) >> 1) & 0x03;
	// 5 bit to the next 2 bit
	_ext.Rx3 = ((Rx >> 3) >> 3) & 0x03;
	_ext.Ry = (Ry >> 3);
	// 5 bit to 3 bit
	_ext.lT = (lT >> 3) & 0x07;
	// 5 bit to the highest two bits
	_ext.lT2 = (lT >> 3) >> 3;
	_ext.rT = (rT >> 3);

	/* Here we encrypt the report */
	// Create a temporary storage for the data
	u8 Tmp[sizeof(_ext)];
	// Clear the array by copying zeroes to it
	memset(Tmp, 0, sizeof(_ext));
	// Copy the data to it
	memcpy(Tmp, &_ext, sizeof(_ext));
	// Encrypt it
	wiimote_encrypt(&g_ExtKey[g_ID], Tmp, 0x00, sizeof(_ext));
	// Write it back to the struct
	memcpy(&_ext, Tmp, sizeof(_ext));
}

/* Generate the 6 byte extension report for the GH3 Controller, encrypted.
   The bytes are ... */
void FillReportGuitarHero3Extension(wm_GH3_extension& _ext)
{
	//	u8 TB   : 5; // not used in GH3
	//	u8 WB   : 5;
	u8 SX = g_GH3Calibration.Lx.center, SY = g_GH3Calibration.Ly.center;

	_ext.pad1 = 3;
	_ext.pad2 = 3;
	_ext.pad3 = 0;
	_ext.pad4 = 0;
	_ext.pad5 = 3;
	_ext.pad6 = 1;
	_ext.pad7 = 1;
	_ext.pad8 = 1;	
	_ext.pad9 = 3;

	_ext.Plus = 1;
	_ext.Minus = 1;
	_ext.StrumDown = 1;
	_ext.StrumUp = 1;
	_ext.Yellow = 1;
	_ext.Green = 1;
	_ext.Blue = 1;
	_ext.Red = 1;
	_ext.Orange = 1;

	// Check that Dolphin is in focus
	if (IsFocus())
	{
		// Update the left analog stick
		if (WiiMapping[g_ID].Stick.GH == FROM_KEYBOARD)
		{
			if(IsKey(EGH_Al)) // Left analog left
				_ext.SX = g_GH3Calibration.Lx.min;
			if(IsKey(EGH_Ar)) // right
				_ext.SX = g_GH3Calibration.Lx.max;
			if(IsKey(EGH_Au)) // up
				_ext.SY = g_GH3Calibration.Ly.max;
			if(IsKey(EGH_Ad)) // down
				_ext.SY = g_GH3Calibration.Ly.min;
		}
		else
		{
			// Get adjusted pad state values
			int _Lx = WiiMapping[g_ID].AxisState.Lx;
			int _Ly = WiiMapping[g_ID].AxisState.Ly;
			int _Rx = WiiMapping[g_ID].AxisState.Rx;
			int _Ry = WiiMapping[g_ID].AxisState.Ry;

			// The Y-axis is inverted
			_Ly = 0xff - _Ly;
			_Ry = 0xff - _Ry;

			if (WiiMapping[g_ID].Stick.GH == FROM_ANALOG1)
			{
				SX = _Lx;
				SY = _Ly;
			}
			else // ANALOG2
			{
				SX = _Rx;
				SX = _Ry;
			}
		}

		if(IsKey(EGH_Yellow)) _ext.Yellow = 0;
		if(IsKey(EGH_Green)) _ext.Green = 0;
		if(IsKey(EGH_Blue)) _ext.Blue = 0;
		if(IsKey(EGH_Red)) _ext.Red = 0;
		if(IsKey(EGH_Orange)) _ext.Orange = 0;
		if(IsKey(EGH_Plus)) _ext.Plus = 0;
		if(IsKey(EGH_Minus)) _ext.Minus = 0;
		if(IsKey(EGH_StrumUp)) _ext.StrumUp = 0; // Strum Up
		if(IsKey(EGH_StrumDown)) _ext.StrumDown= 0; // Strum Down
	}

	// Convert data for reporting
	_ext.SX = (SX >> 2);
	_ext.SY = (SY >> 2);

	/* Here we encrypt the report */
	// Create a temporary storage for the data
	u8 Tmp[sizeof(_ext)];
	// Clear the array by copying zeroes to it
	memset(Tmp, 0, sizeof(_ext));
	// Copy the data to it
	memcpy(Tmp, &_ext, sizeof(_ext));
	// Encrypt it
	wiimote_encrypt(&g_ExtKey[g_ID], Tmp, 0x00, sizeof(_ext));
	// Write it back to the struct
	memcpy(&_ext, Tmp, sizeof(_ext));
}

} // end of namespace
