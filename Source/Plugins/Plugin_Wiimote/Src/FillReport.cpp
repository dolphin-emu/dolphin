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
	// Check if the recording is on
	if (g_RecordingPlaying[Wm] == -1) return false;

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
		Console::Print("\n\nBegin: %i\n", Wm);
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
	// Or if we are playing back all observations regardless of time
	//g_RecordingPoint[Wm] = g_RecordingCounter[Wm];
	//if (g_RecordingPoint[Wm] >= VRecording.at(g_RecordingPlaying[Wm]).Recording.size())
	{
		g_RecordingCounter[Wm] = 0;
		g_RecordingPlaying[Wm] = -1;
		g_RecordingStart[Wm] = 0;
		g_RecordingCurrentTime[Wm] = 0;
		Console::Print("End: %i\n\n", Wm);
		return false;
	}

	// Update accelerometer values
	_x = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).x;
	_y = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).y;
	_z = VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).z;
	// Update IR values
	if(Wm == WM_RECORDING_IR) memcpy(&_IR, VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).IR, IRBytes);

	if (g_DebugAccelerometer)
	{
		//Console::ClearScreen();
		Console::Print("Current time: [%i / %i]  %f %f\n",
			g_RecordingPoint[Wm], VRecording.at(g_RecordingPlaying[Wm]).Recording.size(),
			VRecording.at(g_RecordingPlaying[Wm]).Recording.at(g_RecordingPoint[Wm]).Time, g_RecordingCurrentTime[Wm]
			);
		Console::Print("Accel x, y, z: %03u %03u %03u\n", _x, _y, _z);
	}
	//Console::Print("Accel x, y, z: %03u %03u %03u\n", _x, _y, _z);

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
	//Console::Print("RecordingCheckKeys: %i\n", Wiimote);

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
				//Console::Print("Match: %i %i\n", i, Key);
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

	// Check the mouse position. Don't allow mouse clicks from outside the window.
	float x, y; GetMousePos(x, y);
	bool InsideScreen = !(x < 0 || x > 1 || y < 0 || y > 1);

	// Allow both mouse buttons and keyboard to press a and b
	if((GetAsyncKeyState(VK_LBUTTON) && InsideScreen) || GetAsyncKeyState(PadMapping[0].Wm.A))
		_core.a = 1;
	if((GetAsyncKeyState(VK_RBUTTON) && InsideScreen) || GetAsyncKeyState(PadMapping[0].Wm.B))
		_core.b = 1;

	_core.one = GetAsyncKeyState(PadMapping[0].Wm.One) ? 1 : 0;
	_core.two = GetAsyncKeyState(PadMapping[0].Wm.Two) ? 1 : 0;
	_core.plus = GetAsyncKeyState(PadMapping[0].Wm.P) ? 1 : 0;
	_core.minus = GetAsyncKeyState(PadMapping[0].Wm.M) ? 1 : 0;
	_core.home = GetAsyncKeyState(PadMapping[0].Wm.H) ? 1 : 0;

	/* Sideways controls (for example for Wario Land) if the Wiimote is intended to be held sideways */
	if(g_Config.bSidewaysDPad)
	{
		_core.left = GetAsyncKeyState(PadMapping[0].Wm.D) ? 1 : 0;
		_core.up = GetAsyncKeyState(PadMapping[0].Wm.L) ? 1 : 0;
		_core.right = GetAsyncKeyState(PadMapping[0].Wm.U) ? 1 : 0;
		_core.down = GetAsyncKeyState(PadMapping[0].Wm.R) ? 1 : 0;
	}
	else
	{
		_core.left = GetAsyncKeyState(PadMapping[0].Wm.L) ? 1 : 0;
		_core.up = GetAsyncKeyState(PadMapping[0].Wm.U) ? 1 : 0;
		_core.right = GetAsyncKeyState(PadMapping[0].Wm.R) ? 1 : 0;
		_core.down = GetAsyncKeyState(PadMapping[0].Wm.D) ? 1 : 0;
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
u8 g_x, g_y, g_z, g_X, g_Y, g_Z;

// For the shake function, 0 = Wiimote, 1 = Nunchuck
int Shake[] = {-1, -1};

// For the tilt function, the size of this list determines how fast Y returns to its neutral value
std::vector<u8> yhist(15, 0); float KbDegree;


// ------------------------------------------
// Single shake of Wiimote while holding it sideways (Wario Land pound ground)
// ---------------
void SingleShake(u8 &_y, u8 &_z, int i)
{
#ifdef _WIN32
	// Shake Wiimote with S, Nunchuck with D
	if((i == 0 && GetAsyncKeyState(PadMapping[0].Wm.Shake)) || (i == 1 && GetAsyncKeyState(PadMapping[0].Nc.Shake)))
	{
		_z = 0;
		_y = 0;
		Shake[i] = 2;
	}
	else if(Shake[i] == 2)
	{
		// This works regardless of calibration, in Wario Land
		_z = g_accel.cal_zero.z - 2;
		_y = 0;
		Shake[i] = 1;
	}
	else if(Shake[i] == 1)
	{
		Shake[i] = -1;
	}
#endif
	//if (Shake[i] > -1) Console::Print("Shake: %i\n", Shake[i]);
}


// ------------------------------------------
/* Tilting Wiimote with gamepad. We can guess that the game will calculate a Wiimote pitch and use it as a
   measure of the tilting of the Wiimote. We are interested in this tilting range
		90° to -90° */
// ---------------
void TiltWiimoteGamepad(float &Roll, float &Pitch)
{
	// Return if we have no pads
	if (NumGoodPads == 0) return;

	// This has to be changed if multiple Wiimotes are to be supported later
	const int Page = 0;

	/* Adjust the pad state values, including a downscaling from the original 0x8000 size values
	   to 0x80. The only reason we do this is that the code below crrently assume that the range
	   is 0 to 255 for all axes. If we lose any precision by doing this we could consider not
	   doing this adjustment. And instead for example upsize the XInput trigger from 0x80 to 0x8000. */
	int _Lx, _Ly, _Rx, _Ry, _Tl, _Tr;
	PadStateAdjustments(_Lx, _Ly, _Rx, _Ry, _Tl, _Tr);
	float Lx = (float)_Lx;
	float Ly = (float)_Ly;
	float Rx = (float)_Rx;
	float Ry = (float)_Ry;
	float Tl = (float)_Tl;
	float Tr = (float)_Tr;

	// Save the Range in degrees, 45° and 90° are good values in some games
	float RollRange = (float)g_Config.Trigger.Range.Roll;
	float PitchRange = (float)g_Config.Trigger.Range.Pitch;

	// The trigger currently only controls pitch
	if (g_Config.Trigger.Type == g_Config.Trigger.TRIGGER)
	{
		// Make the range the same dimension as the analog stick
		Tl = Tl / 2;
		Tr = Tr / 2;
		// Invert
		if (PadMapping[Page].bPitchInvert) { Tl = -Tl; Tr = -Tr; }
		// The final value
		Pitch = Tl * (PitchRange / 128.0)
			- Tr * (PitchRange / 128.0);
	}

	/* For the analog stick roll us by default set to the X-axis, pitch is by default set to the Y-axis.
	   By changing the axis mapping and the invert options this can be altered in any way */
	else if (g_Config.Trigger.Type == g_Config.Trigger.ANALOG1)
	{
		// Adjust the trigger to go between negative and positive values
		Lx = Lx - 128.0;
		Ly = Ly - 128.0;
		// Invert
		if (PadMapping[Page].bRollInvert) Lx = -Lx; // else Tr = -Tr;
		if (PadMapping[Page].bPitchInvert) Ly = -Ly; // else Tr = -Tr;
		// Produce the final value
		Roll = Lx * (RollRange / 128.0);
		Pitch = Ly * (PitchRange / 128.0);
	}
	// Otherwise we are using ANALOG2
	else
	{
		// Adjust the trigger to go between negative and positive values
		Rx = Rx - 128.0;
		Ry = Ry - 128.0;
		// Invert
		if (PadMapping[Page].bRollInvert) Rx = -Rx; // else Tr = -Tr;
		if (PadMapping[Page].bPitchInvert) Ry = -Ry; // else Tr = -Tr;
		// Produce the final value
		Roll = Rx * (RollRange / 128.0);
		Pitch = Ry * (PitchRange / 128.0);
	}

	// Adjustment to prevent a slightly to high angle
	if (Pitch >= PitchRange) Pitch = PitchRange - 0.1;
	if (Roll >= RollRange) Roll = RollRange - 0.1;
}


// ------------------------------------------
// Tilting Wiimote with keyboard
// ---------------
void TiltWiimoteKeyboard(float &Roll, float &Pitch)
{
#ifdef _WIN32
	if(GetAsyncKeyState('3'))
	{
		// Stop at the upper end of the range
		if(KbDegree < g_Config.Trigger.Range.Roll)
			KbDegree += 3; // aim left
	}
	else if(GetAsyncKeyState('4'))
	{
		// Stop at the lower end of the range
		if(KbDegree > -g_Config.Trigger.Range.Roll)
			KbDegree -= 3; // aim right
	}

	// -----------------------------------
	// Check for inactivity in the tilting, the Y value will be reset after ten inactive updates
	// ----------
	// Check for activity
	yhist[yhist.size() - 1] = (
		GetAsyncKeyState('3')
		|| GetAsyncKeyState('4')
		);	

	// Move all items back, and check if any of them are true
	bool ypressed = false;
	for (int i = 1; i < (int)yhist.size(); i++)
	{
		yhist[i-1] = yhist[i];
		if(yhist[i]) ypressed = true;
	}
	// Tilting was not used a single time, reset the angle to zero
	if(!ypressed)
	{
		KbDegree = 0;
	}
	else
	{
		Pitch = KbDegree;
		//Console::Print("Degree: %2.1f\n", KbDegree);
	}
	// --------------------
#endif
}

// ------------------------------------------
// Tilting Wiimote (Wario Land aiming, Mario Kart steering and other things)
// ---------------
void Tilt(u8 &_x, u8 &_y, u8 &_z)
{
	// Ceck if it's on
	if (g_Config.Trigger.Type == g_Config.Trigger.TRIGGER_OFF) return;

	// Set to zero
	float Roll = 0, Pitch = 0;

	// Select input method and return the x, y, x values
	if (g_Config.Trigger.Type == g_Config.Trigger.KEYBOARD)
		TiltWiimoteKeyboard(Roll, Pitch);
	else if (g_Config.Trigger.Type == g_Config.Trigger.TRIGGER || g_Config.Trigger.Type == g_Config.Trigger.ANALOG1 || g_Config.Trigger.Type == g_Config.Trigger.ANALOG2)
		TiltWiimoteGamepad(Roll, Pitch);

	// Adjust angles, it's only needed if both roll and pitch is used together
	if (g_Config.Trigger.Range.Roll != 0 && g_Config.Trigger.Range.Pitch != 0) AdjustAngles(Roll, Pitch);

	// Calculate the accelerometer value from this tilt angle
	//PitchDegreeToAccelerometer(Roll, Pitch, _x, _y, _z, g_Config.Trigger.Roll, g_Config.Trigger.Pitch);
	PitchDegreeToAccelerometer(Roll, Pitch, _x, _y, _z);

	if (g_DebugData)
	{
		//Console::ClearScreen();
		/*Console::Print("L:%2.1f R:%2.1f Lx:%2.1f Range:%2.1f Degree:%2.1f L:%i R:%i\n",
			Tl, Tr, Lx, Range, Degree, PadState[Page].Axis.Tl, PadState[Page].Axis.Tr);*/
		/*Console::Print("Roll:%2.1f Pitch:%2.1f\n", Roll, Pitch);*/
	}
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
	g_X = g_accel.cal_zero.x;
	g_Y = g_accel.cal_zero.y;
	g_Z = g_accel.cal_zero.z + g_accel.cal_g.z;


	// Check that Dolphin is in focus
	if (!IsFocus())
	{
		_acc.x = g_X;
		_acc.y = g_y;
		_acc.z = g_z;
		return;
	}

	// ------------------------------------------------
	// Wiimote to Gamepad translations
	// ------------
	
	// The following functions may or may not update these values
	g_x = g_X;
	g_y = g_Y;
	g_z = g_Z;

	// Shake the Wiimote
	SingleShake(g_y, g_z, 0);

	// Tilt Wiimote, allow the shake function to interrupt it
	if (Shake[0] == -1) Tilt(g_x, g_y, g_z);

	// Write final values
	_acc.x = g_x;
	_acc.y = g_y;
	_acc.z = g_z;
	

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
	// ------------------------------------------
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

	// Shake the Wiimote
	SingleShake(_ext.ay, _ext.az, 1);

	// ------------------------------------
	// The default joystick and button values unless we use them
	// --------------
	_ext.jx = g_nu.jx.center;
	_ext.jy = g_nu.jy.center;
	_ext.bt = 0x03; // 0x03 means no button pressed, the button is zero active
	// ---------------------

#ifdef _WIN32
	// Update the analog stick
	if (g_Config.Nunchuck.Type == g_Config.Nunchuck.KEYBOARD)
	{
		// Set the max values to the current calibration values
		if(GetAsyncKeyState(PadMapping[0].Nc.L)) // x
			_ext.jx = g_nu.jx.min;
		if(GetAsyncKeyState(PadMapping[0].Nc.R))
			_ext.jx = g_nu.jx.max;

		if(GetAsyncKeyState(PadMapping[0].Nc.D)) // y
			_ext.jy = g_nu.jy.min;
		if(GetAsyncKeyState(PadMapping[0].Nc.U))
			_ext.jy = g_nu.jy.max;
	}
	else
	{
		// Get adjusted pad state values
		int _Lx, _Ly, _Rx, _Ry, _Tl, _Tr;
		PadStateAdjustments(_Lx, _Ly, _Rx, _Ry, _Tl, _Tr);
		// The Y-axis is inverted
		_Ly = 0xff - _Ly;
		_Ry = 0xff - _Ry;

		/* This is if we are also using a real Nunchuck that we are sharing the calibration with. It's not
		   needed if we are using our default values. We adjust the values to the configured range, we even
		   allow the center to not be 0x80. */
		if(g_nu.jx.max != 0xff || g_nu.jy.max != 0xff
			|| g_nu.jx.min != 0 || g_nu.jy.min != 0
			|| g_nu.jx.center != 0x80 || g_nu.jy.center != 0x80)
		{
			float Lx = (float)_Lx;
			float Ly = (float)_Ly;
			float Rx = (float)_Rx;
			float Ry = (float)_Ry;
			float Tl = (float)_Tl;
			float Tr = (float)_Tr;

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

		if (g_Config.Nunchuck.Type == g_Config.Nunchuck.ANALOG1)
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

	if(GetAsyncKeyState(PadMapping[0].Nc.C))
		_ext.bt = 0x01;
	if(GetAsyncKeyState(PadMapping[0].Nc.Z))
		_ext.bt = 0x02;	
	if(GetAsyncKeyState(PadMapping[0].Nc.C) && GetAsyncKeyState(PadMapping[0].Nc.Z))
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
