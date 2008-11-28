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
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd
#include "Config.h" // for g_Config

extern SWiimoteInitialize g_WiimoteInitialize;


namespace WiiMoteEmu
{

//******************************************************************************
// Subroutines
//******************************************************************************


// ===================================================
/* Debugging. Read out an u8 array. */
// ----------------
std::string ArrayToString(const u8 *data, u32 size, u32 offset, int line_len)
{
	//const u8* _data = (const u8*)data;
	std::string Temp;
	for (u32 i = 0; i < size; i++)
	{
		char Buffer[128];
		sprintf(Buffer, "%02x ", data[i + offset]);
		if((i + 1) % line_len == 0) Temp.append("\n"); // break long lines
		Temp.append(Buffer);
	}	
	return Temp;
}
// ================


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


	/* Sideways controls (for example for Wario Land) was not enabled automatically
	   so I have to use this function. I'm not sure how it works on the actual Wii.
	   */
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

// -----------------------------
/* Global declarations for FillReportAcc. The accelerometer x, y and z values range from
   0x00 to 0xff with the default netural values being [y = 0x84, x = 0x84, z = 0x9f]
   according to a source. The extremes are 0x00 for (-) and 0xff for (+). It's important
   that all values are not 0x80, the the mouse pointer can disappear from the screen
   permanently then, until z is adjusted back. */
// ----------
// the variables are global so they can be changed during debugging
//int A = 0, B = 128, C = 64; // for debugging
//int a = 1, b = 1, c = 2, d = -2; // for debugging
//int consoleDisplay = 0;

int X = 0x84, Y = 0x84, Z = 0x9f; // neutral values
u8 x = X, y = Y, z = Z;
int shake = -1, yhistsize = 15; // for the shake function
std::vector<u8> yhist(15); // for the tilt function

void FillReportAcc(wm_accel& _acc)
{
#ifdef _WIN32
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


	/* Single shake of Wiimote while holding it sideways (Wario Land pound ground)	
	if(GetAsyncKeyState('S'))
		z = 0;
	else
		z  = Z;*/

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
	else // the default Y and Z if nothing is pressed
	{
		z = Z;
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
		y = Y; // this is the default value that will occur most of the time
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
	wprintf("x: %03i | y: %03i | z: %03i  |  A:%i B:%i C:%i  a:%i b:%i c:%i d:%i  X:%i Y:%i Z:%i\n",
		_acc.x, _acc.y, _acc.z,
		A, B, C,
		a, b, c, d,
		X, Y, Z
		);
	wprintf("x: %03i | y: %03i | z: %03i  |  X:%i Y:%i Z:%i  | AX:%i AY:%i AZ:%i \n",
		_acc.x, _acc.y, _acc.z,
		X, Y, Z,
		AX, AY, AZ
		);*/	
#else 
        // TODO port to linux
#endif
}


void FillReportIR(wm_ir_extended& _ir0, wm_ir_extended& _ir1)
{

/* DESCRIPTION: The calibration is controlled by these values, their absolute value and
   the relative distance between between them control the calibration. WideScreen mode
   has its own settings. */
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

	// Fill with 0xff if empty
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
	_ir1.x = x1 & 0xFF;
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
	//if(consoleDisplay == 1)
		wprintf("x0:%03i x1:%03i  y0:%03i y1:%03i   irx0:%03i y0:%03i  x1:%03i y1:%03i | T:%i L:%i R:%i B:%i S:%i\n",
		x0, x1, y0, y1, _ir0.x, _ir0.y, _ir1.x, _ir1.y, Top, Left, Right, Bottom, SensorBarRadius
		);	
	wprintf("\n");
	wprintf("ir0.x:%02x xHi:%02x  ir1.x:%02x xHi:%02x  |  ir0.y:%02x yHi:%02x  ir1.y:%02x yHi:%02x  |  1.s:%02x 2:%02x\n",
		_ir0.x, _ir0.xHi, _ir1.x, _ir1.xHi,
		_ir0.y, _ir0.yHi, _ir1.y, _ir1.yHi,
		_ir0.size, _ir1.size
		);*/
}


void FillReportIRBasic(wm_ir_basic& _ir0, wm_ir_basic& _ir1)
{
	/* See description above */
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

	// Fill with 0xff if empty
	memset(&_ir0, 0xff, sizeof(wm_ir_basic));
	memset(&_ir1, 0xff, sizeof(wm_ir_basic));

	float MouseX, MouseY;
	GetMousePos(MouseX, MouseY);

	int y1 = Top + (MouseY * (Bottom - Top));
	int y2 = Top + (MouseY * (Bottom - Top));

	int x1 = Left + (MouseX * (Right - Left)) - SensorBarRadius;
	int x2 = Left + (MouseX * (Right - Left)) + SensorBarRadius;

	/* As with the extented report we settle with emulating two out of four
	   possible objects */
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
		
		wprintf("x1:%03i x2:%03i  y1:%03i y2:%03i   irx1:%02x y1:%02x  x2:%02x y2:%02x | T:%i L:%i R:%i B:%i S:%i\n",
		x1, x2, y1, y2, _ir0.x1, _ir0.y1, _ir1.x2, _ir1.y2, Top, Left, Right, Bottom, SensorBarRadius
		);
		wprintf("\n");
		wprintf("ir0.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir0.y1:%02x y1h:%02x y2:%02x y2h:%02x  |  ir1.x1:%02x x1h:%02x x2:%02x x2h:%02x  |  ir1.y1:%02x y1h:%02x y2:%02x y2h:%02x\n",
		_ir0.x1, _ir0.x1Hi, _ir0.x2, _ir0.x2Hi,
		_ir0.y1, _ir0.y1Hi, _ir0.y2, _ir0.y2Hi,
		_ir1.x1, _ir1.x1Hi, _ir1.x2, _ir1.x2Hi,
		_ir1.y1, _ir1.y1Hi, _ir1.y2, _ir1.y2Hi
		);*/
}


// ===================================================
/* Generate the 6 byte extension report, encrypted. The bytes are JX JY AX AY AZ BT. */
// ----------------
void FillReportExtension(wm_extension& _ext)
{

	/* These are the default neutral values for the nunchuck accelerometer according
	   to a source. */
	_ext.ax = 0x80;
	_ext.ay = 0x80;
	_ext.az = 0xb3;


	_ext.jx = 0x80; // these are the default values unless we use them
	_ext.jy = 0x80;
	_ext.bt = 0x03; // 0x03 means no button pressed, the button is zero active
	

#ifdef _WIN32
	/* We use a 192 range (32 to 224) that match our calibration values in
	   nunchuck_calibration */
	if(GetAsyncKeyState(VK_NUMPAD4)) // left
		_ext.jx = 0x20;

	if(GetAsyncKeyState(VK_NUMPAD8))
		_ext.jy = 0xe0;

	if(GetAsyncKeyState(VK_NUMPAD6)) // right
		_ext.jx = 0xe0;

	if(GetAsyncKeyState(VK_NUMPAD5))
		_ext.jy = 0x20;


	if(GetAsyncKeyState('C'))
		_ext.bt = 0x01;

	if(GetAsyncKeyState('Z'))
		_ext.bt = 0x02;
	
	if(GetAsyncKeyState('C') && GetAsyncKeyState('Z'))
		_ext.bt = 0x00;
#else 
        // TODO linux port
#endif

	// Clear g_RegExtTmp by copying zeroes to it
	memset(g_RegExtTmp, 0, sizeof(g_RegExtTmp));

	/* Write the nunchuck inputs to it. We begin writing at 0x08, but it could also be
	   0x00, the important thing is that we begin at an address evenly divisible
	   by 0x08 */
	memcpy(g_RegExtTmp + 0x08, &_ext, sizeof(_ext));

	// Encrypt it
	wiimote_encrypt(&g_ExtKey, &g_RegExtTmp[0x08], 0x08, sizeof(_ext));

	// Write it back
	memcpy(&_ext, &g_RegExtTmp[0x08], sizeof(_ext));
}


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

	if(GetAsyncKeyState('F')) // right analog left
		Rx = 0x28;
	if(GetAsyncKeyState('T')) // up
		Ry = 0xd8;
	if(GetAsyncKeyState('H')) // right
		Rx = 0xd8;
	if(GetAsyncKeyState('G')) // down
		Ry = 0x28;

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


	// Clear g_RegExtTmp by copying zeroes to it
	memset(g_RegExtTmp, 0, sizeof(g_RegExtTmp));

	/* Write the nunchuck inputs to it. We begin writing at 0x08, see comment above. */
	memcpy(g_RegExtTmp + 0x08, &_ext, sizeof(_ext));

	// Encrypt it
	wiimote_encrypt(&g_ExtKey, &g_RegExtTmp[0x08], 0x08, 0x06);

	// Write it back
	memcpy(&_ext, &g_RegExtTmp[0x08], sizeof(_ext));
}


} // end of namespace
