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
#include <wx/msgdlg.h>

#include <vector>
#include <string>

#include "Common.h" // Common
#include "pluginspecs_wiimote.h"
#include "StringUtil.h" // For ArrayToString

#include "wiimote_hid.h"
#include "main.h"
#include "EmuMain.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Logging.h" // For startConsoleWin, Console::Print, GetConsoleHwnd
#include "Config.h" // For g_Config
//////////////////////////////////

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{


//**************************************************************************************
// Recorded movements
//**************************************************************************************

// ------------------------------------------
// Variables: 0 = Wiimote, 1 = Nunchuck
// ----------------
int g_RecordingPlaying[3]; //g_RecordingPlaying[0] = -1; g_RecordingPlaying[1] = -1;
int g_RecordingCounter[3]; //g_RecordingCounter[0] = 0; g_RecordingCounter[1] = 0;
int g_RecordingPoint[3]; //g_RecordingPoint[0] = 0; g_RecordingPoint[1] = 0;
double g_RecordingStart[3]; //g_RecordingStart[0] = 0; g_RecordingStart[1] = 0;
double g_RecordingCurrentTime[3]; //g_RecordingCurrentTime[0] = 0; g_RecordingCurrentTime[1] = 0;
// --------------------------

template<class IRReportType>
bool RecordingPlayAccIR(u8 &_x, u8 &_y, u8 &_z, IRReportType &_IR, int Wm)
{
	// Return if the list is empty
	if(VRecording.at(g_RecordingPlaying[Wm]).Recording.size() == 0)
	{
		g_RecordingPlaying[Wm] = -1;
		Console::Print("Empty\n\n");
		return false;
	}

	// Return if the playback speed is unset
	if(VRecording.at(g_RecordingPlaying[Wm]).PlaybackSpeed < 0)
	{
		Console::Print("PlaybackSpeed empty: %i\n\n", g_RecordingPlaying[Wm]);
		g_RecordingPlaying[Wm] = -1;
		return false;
	}

	// Get IR bytes
	int IRBytes = VRecording.at(g_RecordingPlaying[Wm]).IRBytes;

	// Return if the IR mode is wrong
	if (Wm == WM_RECORDING_IR
		&& (   (IRBytes == 12 && !(g_ReportingMode == 0x33))
			|| (IRBytes == 10 && !(g_ReportingMode == 0x36 || g_ReportingMode == 0x37))
			)
		)
	{
		Console::Print("Wrong IR mode: %i\n\n", g_RecordingPlaying[Wm]);
		g_RecordingPlaying[Wm] = -1;
		return false;
	}

	// Get starting time
	if(g_RecordingCounter[Wm] == 0)
	{
		Console::Print("\n\nBegin\n");
		g_RecordingStart[Wm] = GetDoubleTime();
	}

	// Get current time
	g_RecordingCurrentTime[Wm] = GetDoubleTime() - g_RecordingStart[Wm];

	// Modify the current time
	g_RecordingCurrentTime[Wm] *= ((25.0 + (double)VRecording.at(g_RecordingPlaying[Wm]).PlaybackSpeed * 25.0) / 100.0);

	// Select reading
	for (int i = 0; i < VRecording.at(g_RecordingPlaying[Wm]).Recording.size(); i++)
		if (VRecording.at(g_RecordingPlaying[Wm]).Recording.at(i).Time > g_RecordingCurrentTime[Wm])
		{
			g_RecordingPoint[Wm] = i;
			break; // Break loop
		}

	// Return if we are at the end of the list
	if(g_RecordingCurrentTime[Wm] >=
		VRecording.at(g_RecordingPlaying[Wm]).Recording.at(
			VRecording.at(g_RecordingPlaying[Wm]).Recording.size() - 1).Time)
	{
		g_RecordingCounter[Wm] = 0;
		g_RecordingPlaying[Wm] = -1;
		g_RecordingStart[Wm] = 0;
		g_RecordingCurrentTime[Wm] = 0;
		Console::Print("End\n\n");
		return false;
	}

	// Update values
	_x = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).x;
	_y = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).y;
	_z = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).z;
	if(Wm == WM_RECORDING_IR) memcpy(&_IR, VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).IR, IRBytes);

	/**/
	if (g_DebugAccelerometer)
	{
		Console::ClearScreen();
		Console::Print("Current time: %f %f  %i %i\n",
			VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).Time, g_RecordingCurrentTime[Wm],
			VRecording.at(g_RecordingPlaying[Wm]).Recording.size(), g_RecordingPoint[Wm]
			);
		Console::Print("Accel x, y, z: %03u %03u %03u\n", _x, _y, _z);
	}
	//Console::Print("Accel x, y, z: %03u %03u %03u\n", _x, _y, _z);
	//Console::Print("Accel x, y, z: %02x %02x %02x\n", _x, _y, _z);

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

// Check if we should start the playback of a recording. Once it has been started it can not currently
// be stopped, it will always run to the end of the recording.
int RecordingCheckKeys(int Wiimote)
{
#ifdef _WIN32
	//Console::Print("RecordingCheckKeys: %i\n", Wiimote);

	// ------------------------------------
	// Don't allow multiple action keys
	// --------------
	// Return if we have both a Shift, Ctrl, and Alt
	if ( GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_MENU) ) return -1;
	// Return if we have both a Shift and Ctrl
	if ( (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_CONTROL)) ) return -1;
	// Return if we have both a Ctrl and Alt
	if ( (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_MENU)) ) return -1;
	// Return if we have both a Shift and Alt
	if ( (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_MENU)) ) return -1;
	// ---------------------

	// Return if we don't have both a Wiimote and Shift
	if ( Wiimote == 0 && !GetAsyncKeyState(VK_SHIFT) ) return -1;
	
	// Return if we don't have both a Nunchuck and Ctrl
	if ( Wiimote == 1  && !GetAsyncKeyState(VK_CONTROL) ) return -1;

	// Return if we don't have both a IR call and Alt
	if ( Wiimote == 2  && !GetAsyncKeyState(VK_MENU) ) return -1;

	// Check if we have exactly one numerical key
	int Keys = 0;
	for(int i = 0; i < 10; i++)
	{
		std::string Key = StringFromFormat("%i", i);
		if(GetAsyncKeyState(Key[0])) Keys++;
	}

	//Console::Print("RecordingCheckKeys: %i\n", Keys);

	// Return if we have less than or more than one
	if (Keys != 1) return -1;

	// Check which key it is
	int Key;
	for(int i = 0; i < 10; i++)
	{
		std::string TmpKey = StringFromFormat("%i", i);
		if(GetAsyncKeyState(TmpKey[0])) { Key = i; break; }
	}
	
	// Check if we have a HotKey match
	bool Match = false;
	for(int i = 0; i < RECORDING_ROWS; i++)
	{
		if (VRecording.at(i).HotKey == Key)
		{
			//Console::Print("Match: %i %i\n", i, Key);
			Match = true;
			Key = i;
			break;
		}
	}

	// Return nothing if we don't have a match
	if (!Match) return -1;

	// Return the match
	return Key;
#else
	return -1;
#endif
}

// check if we have any recording playback key combination
bool CheckKeyCombination()
{
	if (RecordingCheckKeys(0) == -1 && RecordingCheckKeys(1) == -1 && RecordingCheckKeys(2) == -1)
		return false;
	else
		return true; // This will also start a recording
}



//******************************************************************************
// Subroutines
//******************************************************************************


////////////////////////////////////////////////////////////
// Wiimote core buttons
// ---------------
void FillReportInfo(wm_core& _core)
{
	/* This has to be filled with zeroes (and not for example 0xff) because when no buttons are pressed the
	   value is 00 00 */
	memset(&_core, 0x00, sizeof(wm_core));

#ifdef _WIN32
	// Check that Dolphin is in focus
	if (!IsFocus()) return;

	// Don't interrupt a recording
	if (CheckKeyCombination()) return;

	// Check the mouse position. Don't allow mouse clicks from outside the window.
	float x, y; GetMousePos(x, y);
	bool InsideScreen = !(x < 0 || x > 1 || y < 0 || y > 1);

	// Allow both mouse buttons and keyboard to press a and b
	if((GetAsyncKeyState(VK_LBUTTON) && InsideScreen) || GetAsyncKeyState('A') ? 1 : 0)
		_core.a = 1;

	if((GetAsyncKeyState(VK_RBUTTON) && InsideScreen) || GetAsyncKeyState('B') ? 1 : 0)
		_core.b = 1;

	_core.one = GetAsyncKeyState('1') ? 1 : 0;
	_core.two = GetAsyncKeyState('2') ? 1 : 0;
	_core.plus = GetAsyncKeyState('P') ? 1 : 0;
	_core.minus = GetAsyncKeyState('M') ? 1 : 0;
	_core.home = GetAsyncKeyState('H') ? 1 : 0;

	/* Sideways controls (for example for Wario Land) if the Wiimote is intended to be held sideways */
	if(g_Config.bSidewaysDPad)
	{
		_core.left = GetAsyncKeyState(VK_DOWN) ? 1 : 0;
		_core.up = GetAsyncKeyState(VK_LEFT) ? 1 : 0;
		_core.right = GetAsyncKeyState(VK_UP) ? 1 : 0;
		_core.down = GetAsyncKeyState(VK_RIGHT) ? 1 : 0;
	}
	else
	{
		_core.left = GetAsyncKeyState(VK_LEFT) ? 1 : 0;
		_core.up = GetAsyncKeyState(VK_UP) ? 1 : 0;
		_core.right = GetAsyncKeyState(VK_RIGHT) ? 1 : 0;
		_core.down = GetAsyncKeyState(VK_DOWN) ? 1 : 0;
	}
#else 
        // TODO: fill in
#endif
}
//////////////////////////


///////////////////////////////////////////////////////////////////
// Wiimote accelerometer
// ---------------
/* The accelerometer x, y and z values range from 0x00 to 0xff with the default netural values
   being [y = 0x84, x = 0x84, z = 0x9f] according to a source. The extremes are 0x00 for (-)
   and 0xff for (+). It's important that all values are not 0x80, the mouse pointer can disappear
   from the screen permanently then, until z is adjusted back. */


// ----------
// Global declarations for FillReportAcc: These variables are global so they can be changed during debugging
//int A = 0, B = 128, C = 64; // for debugging
//int a = 1, b = 1, c = 2, d = -2; // for debugging
//int consoleDisplay = 0;

// For all functions
u8 x, y, z, X, Y, Z;

// For the shake function
int shake = -1;

// For the tilt function, the size of this list determines how fast Y returns to its neutral value
std::vector<u8> yhist(15, 0); float KbDegree;


// ------------------------------------------
// Single shake of Wiimote while holding it sideways (Wario Land pound ground)
// ---------------
void SingleShake(u8 &_z, u8 &_y)
{
	if(GetAsyncKeyState('S'))
	{
		_z = 0;
		_y = 0;
		shake = 2;
	}
	else if(shake == 2)
	{
		_z = 128;
		_y = 0;
		shake = 1;
	}
	else if(shake == 1)
	{
		_z = Z;
		_y = Y;
		shake = -1;
	}
	else // the default Z if nothing is pressed
	{
		z = Z;
	}
}

// ------------------------------------------
/* Tilting Wiimote with gamepad. We can guess that the game will calculate a Wiimote pitch and use it as a
   measure of the tilting of the Wiimote. We are interested in this tilting range
			90° to -90° */
// ---------------
void TiltWiimoteGamepad(u8 &_y, u8 &_z)
{
	// Update the pad state
	const int Page = 0;
	WiiMoteEmu::GetJoyState(PadState[Page], PadMapping[Page], Page, joyinfo[PadMapping[Page].ID].NumButtons);

	// Convert the big values
	float Lx = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Lx);
	float Ly = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Ly);
	float Rx = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Rx);
	float Ry = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Ry);
	float Tl, Tr;

	if (PadMapping[Page].triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
		Tl = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Tl);
		Tr = (float)InputCommon::Pad_Convert(PadState[Page].Axis.Tr);
	}
	else
	{
		Tl = (float)PadState[Page].Axis.Tl;
		Tr = (float)PadState[Page].Axis.Tr;
	}

	// It's easier to use a float here
	float Degree = 0;
	// Calculate the present room between the neutral and the maximum values
	float RoomAbove = 255.0 - (float)Y;
	float RoomBelow = (float)Y;
	// Save the Range in degrees, 45° and 90° are good values in some games
	float Range = (float)g_Config.Trigger.Range;

	// Trigger
	if (g_Config.Trigger.Type == g_Config.TRIGGER)
	{
		// Make the range the same dimension as the analog stick
		Tl = Tl / 2;
		Tr = Tr / 2;

		Degree = Tl * (Range / 128)
			- Tr * (Range / 128);
	}

	// Analog stick
	else
	{
		// Adjust the trigger to go between negative and positive values
		Lx = Lx - 128;
		// Produce the final value
		Degree = -Lx * (Range / 128);
	}

	// Calculate the acceleometer value from this tilt angle
	PitchDegreeToAccelerometer(Degree, _y, _z);

	//Console::ClearScreen();
	/*Console::Print("L:%2.1f R:%2.1f Lx:%2.1f Range:%2.1f Degree:%2.1f L:%i R:%i\n",
		Tl, Tr, Lx, Range, Degree, PadState[Page].Axis.Tl, PadState[Page].Axis.Tr);*/
	/*Console::Print("Degree:%2.1f\n", Degree);*/
}


// ------------------------------------------
// Tilting Wiimote (Wario Land aiming, Mario Kart steering) : For some reason 150 and 40
// seemed like decent starting values.
// ---------------
void TiltWiimoteKeyboard(u8 &_y, u8 &_z)
{
#ifdef _WIN32
	if(GetAsyncKeyState('3'))
	{
		// Stop at the upper end of the range
		if(KbDegree < g_Config.Trigger.Range)
			KbDegree += 3; // aim left
	}
	else if(GetAsyncKeyState('4'))
	{
		// Stop at the lower end of the range
		if(KbDegree > -g_Config.Trigger.Range)
			KbDegree -= 3; // aim right
	}

	// -----------------------------------
	// Check for inactivity in the tilting, the Y value will be reset after ten inactive updates
	// ----------

	yhist[yhist.size() - 1] = (
		GetAsyncKeyState('3')
		|| GetAsyncKeyState('4')
		|| shake > 0
		);	

	// Move all items back, and check if any of them are true
	bool ypressed = false;
	for (int i = 1; i < (int)yhist.size(); i++)
	{
		yhist[i-1] = yhist[i];
		if(yhist[i]) ypressed = true;
	}
	// Tilting was not used a single time, reset y to its neutral value
	if(!ypressed)
	{
		_y = Y;
	}
	else
	{
		PitchDegreeToAccelerometer(KbDegree, _y, _z);
		//Console::Print("Degree: %2.1f\n", KbDegree);
	}
	// --------------------
#endif
}

void FillReportAcc(wm_accel& _acc)
{
	// ------------------------------------
	// Recorded movements
	// --------------
	// Check for a playback command
	if(g_RecordingPlaying[0] < 0)
	{
		g_RecordingPlaying[0] = RecordingCheckKeys(0);
	}
	else
	{
		// If the recording reached the end or failed somehow we will not return
		if (RecordingPlay(_acc.x, _acc.y, _acc.z, 0)) return;
		//Console::Print("X, Y, Z: %u %u %u\n", _acc.x, _acc.y, _acc.z);
	}
	// ---------------------

	// The default values can change so we need to update them all the time
	X = g_accel.cal_zero.x;
	Y = g_accel.cal_zero.y;
	Z = g_accel.cal_zero.z + g_accel.cal_g.z;


	// Check that Dolphin is in focus
	if (!IsFocus())
	{
		_acc.x = X;
		_acc.y = y;
		_acc.z = z;
		return;
	}

	// ------------------------------------------------
	// Wiimote to Gamepad translations
	// ------------

	// Shake the Wiimote
	SingleShake(z, y);

	// Tilt Wiimote
	if (g_Config.Trigger.Type == g_Config.KEYBOARD)
		TiltWiimoteKeyboard(y, z);
	else if (g_Config.Trigger.Type == g_Config.TRIGGER || g_Config.Trigger.Type == g_Config.ANALOG)
		TiltWiimoteGamepad(y, z);

	// Write values
	_acc.x = X;
	_acc.y = y;
	_acc.z = z;
	

	// ----------------------------
	// Debugging for translating Wiimote to Keyboard (or Gamepad)
	// ----------
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
	Console::Print("x: %03i | y: %03i | z: %03i  |  A:%i B:%i C:%i  a:%i b:%i c:%i d:%i  X:%i Y:%i Z:%i\n",
		_acc.x, _acc.y, _acc.z,
		A, B, C,
		a, b, c, d,
		X, Y, Z
		);
	Console::Print("x: %03i | y: %03i | z: %03i  |  X:%i Y:%i Z:%i  | AX:%i AY:%i AZ:%i \n",
		_acc.x, _acc.y, _acc.z,
		X, Y, Z,
		AX, AY, AZ
		);*/	
}
/////////////////////////


///////////////////////////////////////////////////////////////////
// The extended 12 byte (3 byte per object) reporting
// ---------------
void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1)
{
	// ------------------------------------
	// Recorded movements
	// --------------
	// Check for a playback command
	if(g_RecordingPlaying[2] < 0)
	{
		g_RecordingPlaying[2] = RecordingCheckKeys(2);
	}
	else
	{
		//Console::Print("X, Y, Z: %u %u %u\n", _acc.x, _acc.y, _acc.z);
		if (RecordingPlayIR(_ir0)) return;		
	}
	// ---------------------


	// --------------------------------------
	/* The calibration is controlled by these values, their absolute value and
	   the relative distance between between them control the calibration. WideScreen mode
	   has its own settings. */
	// ----------
	int Top, Left, Right, Bottom, SensorBarRadius;
	if(g_Config.bWideScreen)
	{		
		Top = wTOP; Left = wLEFT; Right = wRIGHT;
		Bottom = wBOTTOM; SensorBarRadius = wSENSOR_BAR_RADIUS;
	}
	else
	{
		Top = TOP; Left = LEFT; Right = RIGHT;
		Bottom = BOTTOM; SensorBarRadius = SENSOR_BAR_RADIUS;		
	}
	// ------------------

	/* Fill with 0xff if empty. The real Wiimote seems to use 0xff when it doesn't see a certain point,
	   at least from how WiiMoteReal::SendEvent() works. */
	memset(&_ir0, 0xff, sizeof(wm_ir_extended));
	memset(&_ir1, 0xff, sizeof(wm_ir_extended));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	// If we are outside the screen leave the values at 0xff
	if(MouseX > 1 || MouseX < 0 || MouseY > 1 || MouseY < 0) return;

	// --------------------------------------
	// Actual position calculation
	// ----------
	int y0 = Top + (MouseY * (Bottom - Top));
	int y1 = Top + (MouseY * (Bottom - Top));

	int x0 = Left + (MouseX * (Right - Left)) - SensorBarRadius;
	int x1 = Left + (MouseX * (Right - Left)) + SensorBarRadius;

	x0 = 1023 - x0;
	_ir0.x = x0 & 0xFF;
	_ir0.y = y0 & 0xFF;
	_ir0.size = 10;
	_ir0.xHi = x0 >> 8;
	_ir0.yHi = y0 >> 8;

	x1 = 1023 - x1;
	_ir1.x = x1 & 0xFF;
	_ir1.y = y1 & 0xFF;
	_ir1.size = 10;
	_ir1.xHi = x1 >> 8;
	_ir1.yHi = y1 >> 8;
	// ------------------

	// ----------------------------
	// Debugging for calibration
	// ----------
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
		Console::Print("x0:%03i x1:%03i  y0:%03i y1:%03i   irx0:%03i y0:%03i  x1:%03i y1:%03i | T:%i L:%i R:%i B:%i S:%i\n",
		x0, x1, y0, y1, _ir0.x, _ir0.y, _ir1.x, _ir1.y, Top, Left, Right, Bottom, SensorBarRadius
		);	
	Console::Print("\n");
	Console::Print("ir0.x:%02x xHi:%02x  ir1.x:%02x xHi:%02x  |  ir0.y:%02x yHi:%02x  ir1.y:%02x yHi:%02x  |  1.s:%02x 2:%02x\n",
		_ir0.x, _ir0.xHi, _ir1.x, _ir1.xHi,
		_ir0.y, _ir0.yHi, _ir1.y, _ir1.yHi,
		_ir0.size, _ir1.size
		);*/
	// ------------------
}

///////////////////////////////////////////////////////////////////
// The 10 byte reporting used when an extension is connected
// ---------------
void FillReportIRBasic(wm_ir_basic& _ir0, wm_ir_basic& _ir1)
{
	// ------------------------------------
	// Recorded movements
	// --------------
	// Check for a playback command
	if(g_RecordingPlaying[2] < 0)
	{
		g_RecordingPlaying[2] = RecordingCheckKeys(2);
	}
	// We are playing back a recording, we don't accept any manual input this time
	else
	{
		//Console::Print("X, Y, Z: %u %u %u\n", _acc.x, _acc.y, _acc.z);
		if (RecordingPlayIR(_ir0)) return;		
	}
	// ---------------------

	// --------------------------------------
	/* See calibration description above */
	// ----------
	int Top, Left, Right, Bottom, SensorBarRadius;

	if(g_Config.bWideScreen)
	{		
		Top = wTOP; Left = wLEFT; Right = wRIGHT;
		Bottom = wBOTTOM; SensorBarRadius = wSENSOR_BAR_RADIUS;
	}
	else
	{
		Top = TOP; Left = LEFT; Right = RIGHT;
		Bottom = BOTTOM; SensorBarRadius = SENSOR_BAR_RADIUS;		
	}
	// ------------------

	// Fill with 0xff if empty
	memset(&_ir0, 0xff, sizeof(wm_ir_basic));
	memset(&_ir1, 0xff, sizeof(wm_ir_basic));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	// If we are outside the screen leave the values at 0xff
	if(MouseX > 1 || MouseX < 0 || MouseY > 1 || MouseY < 0) return;

	int y1 = Top + (MouseY * (Bottom - Top));
	int y2 = Top + (MouseY * (Bottom - Top));

	int x1 = Left + (MouseX * (Right - Left)) - SensorBarRadius;
	int x2 = Left + (MouseX * (Right - Left)) + SensorBarRadius;

	/* As with the extented report we settle with emulating two out of four possible objects */
	x1 = 1023 - x1;
	_ir0.x1 = x1 & 0xff;
	_ir0.y1 = y1 & 0xff;
	_ir0.x1Hi = (x1 >> 8); // we are dealing with 2 bit values here
	_ir0.y1Hi = (y1 >> 8);

	x2 = 1023 - x2;
	_ir0.x2 = x2 & 0xff;
	_ir0.y2 = y2 & 0xff;
	_ir0.x2Hi = (x2 >> 8);
	_ir0.y2Hi = (y2 >> 8);

	// I don't understand't the & 0x03, should we do that?
	//_ir1.x1Hi = (x1 >> 8) & 0x3;
	//_ir1.y1Hi = (y1 >> 8) & 0x3;

	// ------------------------------------
	// Debugging for calibration
	// ----------	
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
		
		Console::Print("x1:%03i x2:%03i  y1:%03i y2:%03i   irx1:%02x y1:%02x  x2:%02x y2:%02x | T:%i L:%i R:%i B:%i S:%i\n",
		x1, x2, y1, y2, _ir0.x1, _ir0.y1, _ir1.x2, _ir1.y2, Top, Left, Right, Bottom, SensorBarRadius
		);
		Console::Print("\n");
		Console::Print("ir0.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir0.y1:%02x y1h:%02x y2:%02x y2h:%02x  |  ir1.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir1.y1:%02x y1h:%02x y2:%02x y2h:%02x\n",
		_ir0.x1, _ir0.x1Hi, _ir0.x2, _ir0.x2Hi,
		_ir0.y1, _ir0.y1Hi, _ir0.y2, _ir0.y2Hi,
		_ir1.x1, _ir1.x1Hi, _ir1.x2, _ir1.x2Hi,
		_ir1.y1, _ir1.y1Hi, _ir1.y2, _ir1.y2Hi
		);*/
	// ------------------
}


//**************************************************************************************
// Extensions
//**************************************************************************************


// ===================================================
/* Generate the 6 byte extension report for the Nunchuck, encrypted. The bytes are JX JY AX AY AZ BT. */
// ----------------
void FillReportExtension(wm_extension& _ext)
{
	// ------------------------------------
	// Recorded movements
	// --------------
	// Check for a playback command
	if(g_RecordingPlaying[1] < 0) g_RecordingPlaying[1] = RecordingCheckKeys(1);

	// We should not play back the accelerometer values
	if (!(g_RecordingPlaying[1] >= 0 && RecordingPlay(_ext.ax, _ext.ay, _ext.az, 1)))
	{
		/* These are the default neutral values for the nunchuck accelerometer according to the calibration
		   data we have in nunchuck_calibration[] */
		_ext.ax = 0x80;
		_ext.ay = 0x80;
		_ext.az = 0xb3;
	}
	// ---------------------

	// ------------------------------------
	// The default joystick and button values unless we use them
	// --------------
	_ext.jx = g_nu.jx.center;
	_ext.jy = g_nu.jy.center;
	_ext.bt = 0x03; // 0x03 means no button pressed, the button is zero active
	// ---------------------

#ifdef _WIN32
	// Set the max values to the current calibration values
	if(GetAsyncKeyState(VK_NUMPAD4)) // x
		_ext.jx = g_nu.jx.min;
	if(GetAsyncKeyState(VK_NUMPAD6))
		_ext.jx = g_nu.jx.max;

	if(GetAsyncKeyState(VK_NUMPAD5)) // y
		_ext.jy = g_nu.jy.min;
	if(GetAsyncKeyState(VK_NUMPAD8))
		_ext.jy = g_nu.jy.max;

	if(GetAsyncKeyState('C'))
		_ext.bt = 0x01;
	if(GetAsyncKeyState('Z'))
		_ext.bt = 0x02;	
	if(GetAsyncKeyState('C') && GetAsyncKeyState('Z'))
		_ext.bt = 0x00;
#else 
        // TODO linux port
#endif

	/* Here we encrypt the report */

	// Create a temporary storage for the data
	u8 Tmp[sizeof(_ext)];
	// Clear the array by copying zeroes to it
	memset(Tmp, 0, sizeof(_ext));
	// Copy the data to it
	memcpy(Tmp, &_ext, sizeof(_ext));
	// Encrypt it
	wiimote_encrypt(&g_ExtKey, Tmp, 0x00, sizeof(_ext));
	// Write it back to the struct
	memcpy(&_ext, Tmp, sizeof(_ext));
}
// =======================


// ===================================================
/* Generate the 6 byte extension report for the Classic Controller, encrypted.
   The bytes are ... */
// ----------------
void FillReportClassicExtension(wm_classic_extension& _ext)
{

	/* These are the default neutral values for the analog triggers and sticks */
	u8 Rx = 0x80, Ry = 0x80, Lx = 0x80, Ly = 0x80, lT = 0x80, rT = 0x80;

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


	// --------------------------------------
	// Check that Dolphin is in focus
	if (!IsFocus()) return;
	// --------------------------------------


	// --------------------------------------
	/* Left and right analog sticks

		u8 Lx : 6; // byte 0
		u8 Rx : 2;
		u8 Ly : 6; // byte 1
		u8 Rx2 : 2;
		u8 Ry : 5; // byte 2
		u8 lT : 2;
		u8 Rx3 : 1;
		u8 rT : 5; // byte 3
		u8 lT2 : 3;
	*/
#ifdef _WIN32
	/* We use a 200 range (28 to 228) for the left analog stick and a 176 range
	   (40 to 216) for the right analog stick to match our calibration values
	   in classic_calibration */
	if(GetAsyncKeyState('J')) // left analog left
		Lx = 0x1c;
	if(GetAsyncKeyState('I')) // up
		Ly = 0xe4;
	if(GetAsyncKeyState('L')) // right
		Lx = 0xe4;
	if(GetAsyncKeyState('K')) // down
		Ly = 0x1c;

	if(GetAsyncKeyState('D')) // right analog left
		Rx = 0x28;
	if(GetAsyncKeyState('R')) // up
		Ry = 0xd8;
	if(GetAsyncKeyState('G')) // right
		Rx = 0xd8;
	if(GetAsyncKeyState('F')) // down
		Ry = 0x28;

#endif
	_ext.Lx = (Lx >> 2);
	_ext.Ly = (Ly >> 2);
	_ext.Rx = (Rx >> 3); // this may be wrong
	_ext.Rx2 = (Rx >> 5);
	_ext.Rx3 = (Rx >> 7);
	_ext.Ry = (Ry >> 2);

	_ext.lT = (Ry >> 2);
	_ext.lT2 = (Ry >> 3);
	_ext.rT = (Ry >> 4);
	// --------------
#ifdef _WIN32



	// --------------------------------------
	/* D-Pad
	
	u8 b1;
		0:
		6: bdD
		7: bdR

	u8 b2;
		0: bdU
		1: bdL	
	*/
	if(GetAsyncKeyState(VK_NUMPAD4)) // left
		_ext.b2.bdL = 0x00;

	if(GetAsyncKeyState(VK_NUMPAD8)) // up
		_ext.b2.bdU = 0x00;

	if(GetAsyncKeyState(VK_NUMPAD6)) // right
		_ext.b1.bdR = 0x00;

	if(GetAsyncKeyState(VK_NUMPAD5)) // down
		_ext.b1.bdD = 0x00;
	// --------------


	// --------------------------------------
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
	if(GetAsyncKeyState('Z'))
		_ext.b2.bA = 0x00;

	if(GetAsyncKeyState('C'))
		_ext.b2.bB = 0x00;

	if(GetAsyncKeyState('Y'))
		_ext.b2.bY = 0x00;

	if(GetAsyncKeyState('X'))
		_ext.b2.bX = 0x00;

	if(GetAsyncKeyState('O')) // O instead of P
		_ext.b1.bP = 0x00;

	if(GetAsyncKeyState('N')) // N instead of M
		_ext.b1.bM = 0x00;

	if(GetAsyncKeyState('U')) // Home button
		_ext.b1.bH = 0x00;

	if(GetAsyncKeyState('7')) // digital left trigger
		_ext.b1.bLT = 0x00;

	if(GetAsyncKeyState('8'))
		_ext.b2.bZL = 0x00;

	if(GetAsyncKeyState('9'))
		_ext.b2.bZR = 0x00;

	if(GetAsyncKeyState('0')) // digital right trigger
		_ext.b1.bRT = 0x00;
	
	// All buttons pressed
	//if(GetAsyncKeyState('C') && GetAsyncKeyState('Z'))
	//	{ _ext.b2.bA = 0x01; _ext.b2.bB = 0x01; }
	// --------------
#else 
        // TODO linux port
#endif

	/* Here we encrypt the report */

	// Create a temporary storage for the data
	u8 Tmp[sizeof(_ext)];
	// Clear the array by copying zeroes to it
	memset(Tmp, 0, sizeof(_ext));
	// Copy the data to it
	memcpy(Tmp, &_ext, sizeof(_ext));
	// Encrypt it
	wiimote_encrypt(&g_ExtKey, Tmp, 0x00, sizeof(_ext));
	// Write it back to the struct
	memcpy(&_ext, Tmp, sizeof(_ext));
}
// =======================


} // end of namespace
