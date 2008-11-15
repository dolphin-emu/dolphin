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


#include <wx/msgdlg.h>

#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>
#include "Common.h"
#include "wiimote_hid.h"
#include "EmuDeclarations.h"
#include "EmuDefinitions.h"
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd

extern SWiimoteInitialize g_WiimoteInitialize;
//extern void __Log(int log, const char *format, ...);
//extern void __Log(int log, int v, const char *format, ...);

namespace WiiMoteEmu
{


//******************************************************************************
// Subroutines
//******************************************************************************


bool toggleSideWays = false;
void FillReportInfo(wm_core& _core)
{
	memset(&_core, 0x00, sizeof(wm_core));

#ifdef _WIN32
	// allow both mouse buttons and keyboard to press a and b
	if(GetAsyncKeyState(VK_LBUTTON) ? 1 : 0 || GetAsyncKeyState('A') ? 1 : 0)
		_core.a = 1;

	if(GetAsyncKeyState(VK_RBUTTON) ? 1 : 0 || GetAsyncKeyState('B') ? 1 : 0)
		_core.b = 1;

	_core.one = GetAsyncKeyState('1') ? 1 : 0;
	_core.two = GetAsyncKeyState('2') ? 1 : 0;
	_core.plus = GetAsyncKeyState('P') ? 1 : 0;
	_core.minus = GetAsyncKeyState('M') ? 1 : 0;
	_core.home = GetAsyncKeyState('H') ? 1 : 0;

	if(GetAsyncKeyState('T'))
	{
		wxMessageBox(wxString::Format("You turned %s sideways controls",
			toggleSideWays ? "off" : "on"));
		toggleSideWays = !toggleSideWays;
	}

	/* Sideways controls (for example for Wario Land) was not enabled automatically
	   so I have to use this function. I'm not sure how it works on the actual Wii.
	   */
	if(toggleSideWays)
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

// -----------------------------
// Global declarations for FillReportAcc. The accelerometer x, y and z values range from
// 0x00 to 0xff with [y = 0x80, x = 0x80, z ~ 0xa0] being neutral and 0x00 being (-)
// and 0xff being (+). Or does it not? It's important that all values are not 0x80,
// the the mouse pointer can disappear from the screen permanently, until z is adjusted
// back.
// ----------
// the variables are global so they can be changed during debugging
//int A = 0, B = 128, C = 64; // for debugging
//int a = 1, b = 1, c = 2, d = -2; // for debugging
//int consoleDisplay = 0;


int X = 0x80, Y = 0x80, Z = 160; // neutral values
u8 x = 0x0, y = 0x0, z = 0x00;
int shake = -1, yhistsize = 15; // for the shake function
std::vector<u8> yhist(15); // for the tilt function


void FillReportAcc(wm_accel& _acc)
{
	// -----------------------------
	// Wiimote to Gamepad translations
	// ----------
	// Tilting Wiimote (Wario Land aiming, Mario Kart steering) : For some reason 150 and 40
	// seemed like decent starting values.
	if(GetAsyncKeyState('3'))
	{
		//if(a < 128) // for debugging
		if(y < 250)
		{
			y += 4; // aim left
			//a += c;  // debugging values
			//y = A + a; // aim left
		}
	}
	else if(GetAsyncKeyState('4'))
	{
		// if(b < 128) // for debugging
		if(y > 5)
		{
			y -= 4; // aim right
			//b -= d; // debugging values
			//y = B + b;
		}
	}


	// Single shake of Wiimote while holding it sideways (Wario Land pound ground)
	/*
	if(GetAsyncKeyState('S'))
		z = 0;
	else
		z  = Z;
		*/

	if(GetAsyncKeyState('S'))
	{
		z = 0;
		y = 0;
		shake = 2;
	}
	else if(shake == 2)
	{
		z = 128;
		y = 0;
		shake = 1;
	}
	else if(shake == 1)
	{
		z = Z;
		y = Y;
		shake = -1;
	}
	// ----------


	// -----------------------------
	// For tilting: add new value and move all back
	// ----------
	bool ypressed = false;

	yhist[yhist.size() - 1] = (
		GetAsyncKeyState('3') ? true : false
		|| GetAsyncKeyState('4') ? true : false
		|| shake > 0
		);	
	if(yhistsize > yhist.size()) yhistsize = yhist.size();
	for (int i = 1; i < yhistsize; i++)
	{
		yhist[i-1] = yhist[i];
		if(yhist[i]) ypressed = true;
	}
	
	if(!ypressed) // y was not pressed a single time
	{
		y = Y;
		//a = 0; // for debugging
		//b = 0;
	}
	else if(!GetAsyncKeyState('3') && !GetAsyncKeyState('4'))
	{
		// perhaps start dropping acceleration back?
	}
	// ----------


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
	else if(GetAsyncKeyState(VK_INSERT))
		a-=1;
	else if(GetAsyncKeyState(VK_DELETE))
		a+=1;
	else if(GetAsyncKeyState(VK_HOME))
		b-=1;
	else if(GetAsyncKeyState(VK_END))
		b+=1;
	else if(GetAsyncKeyState(VK_SHIFT))
		c-=1;
	else if(GetAsyncKeyState(VK_CONTROL))
		c+=1;
	else if(GetAsyncKeyState(VK_NUMPAD3))
		d-=1;
	else if(GetAsyncKeyState(VK_NUMPAD6))
		d+=1;
	else if(GetAsyncKeyState(VK_ADD))
		yhistsize-=1;
	else if(GetAsyncKeyState(VK_SUBTRACT))
		yhistsize+=1;


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

	/*
	if(GetAsyncKeyState('S'))
	{
		z = Z + C;
	}
	else
	{
		z = Z;
	}
	

	if(GetAsyncKeyState('D'))
	{
		y = Y + B;
	}
	else
	{
		y = Y;
	}

	if(GetAsyncKeyState('F'))
	{
		z = Z + C;
		y = Y + B;
	}
	else if(!GetAsyncKeyState('S') && !GetAsyncKeyState('D'))
	{
		z = Z;
		y = Y;
	}	
	if(consoleDisplay == 0)
	wprintf("x: %03i | y: %03i | z: %03i  |  A:%i B:%i C:%i  a:%i b:%i c:%i d:%i  X:%i Y:%i Z:%i\n", _acc.x, _acc.y, _acc.z,
		A, B, C,
		a, b, c, d,
		X, Y, Z);
	*/
}

/* DESCRIPTION: The calibration is controlled by these values, their absolute value and
   the relative distance between between them control the calibration. These integers is
   for the debugger so that we can calibrate the best values */
int Top = TOP, Left = LEFT, Right = RIGHT,
Bottom = BOTTOM, SensorBarRadius = SENSOR_BAR_RADIUS;

void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1)
{
	memset(&_ir0, 0xFF, sizeof(wm_ir_extended));
	memset(&_ir1, 0xFF, sizeof(wm_ir_extended));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

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
	_ir1.x = x1;
	_ir1.y = y1 & 0xFF;
	_ir1.size = 10;
	_ir1.xHi = x1 >> 8;
	_ir1.yHi = y1 >> 8;


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
	if(consoleDisplay == 1)
		wprintf("x0:%03i x1:%03i  y0:%03i y1:%03i   irx0:%03i y0:%03i  x1:%03i y1:%03i | T:%i L:%i R:%i B:%i S:%i\n",
		x0, x1, y0, y1, _ir0.x, _ir0.y, _ir1.x, _ir1.y, Top, Left, Right, Bottom, SensorBarRadius
		);
	*/
	
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

	//ClearScreen();
	/*
	wprintf("x0:%03i | y0:%03i  ||  x1:%03i | 0.y: %03i  ||  1.x: %03i | 1.y %03i\n", _ir0.x1, _ir0.y1,
	_ir1.x2, _ir1.y2
	);
	*/
}


} // end of namespace
