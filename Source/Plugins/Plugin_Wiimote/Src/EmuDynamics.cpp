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
#include "Logging.h" // for startConsoleWin, Console::Print, GetConsoleHwnd
#include "Config.h" // for g_Config
////////////////////////////////////


namespace WiiMoteEmu
{

void PitchDegreeToAccelerometer(float _Roll, float _Pitch, u8 &_x, u8 &_y, u8 &_z, bool RollOn, bool PitchOn)
{
	// Then don't update z either
	if (!RollOn && ! PitchOn) return;

	// Calculate the radian
	float _RollRad = _Roll * M_PI / 180.0;
	float _PitchRad = _Pitch * M_PI / 180.0;

	// Calculate a good set of y and z values for the degree
	float r = 1.0;
	float fx = r * sin(_RollRad); // y
	float fy = r * sin(_PitchRad); // y
	float fz = r * cos(_PitchRad); // x

	// Multiple with the neutral of z and its g
	float xg = g_accel.cal_g.x;
	float yg = g_accel.cal_g.y;
	float zg = g_accel.cal_g.z;
	float x_zero = g_accel.cal_zero.x;
	float y_zero = g_accel.cal_zero.y;
	float z_zero = g_accel.cal_zero.z;
	fx = (int) (x_zero + xg * fx);
	fy = (int) (y_zero + yg * fy);
	fz = (int) (z_zero + zg * fz);

	// Boundaries
	int ix = (int)fx;
	int iy = (int)fy;
	int iz = (int)fz;
	if (ix < 0) ix = 0; if (ix > 255) ix = 255;
	if (iy < 0) iy = 0; if (iy > 255) iy = 255;
	if (iz < 0) iz = 0; if (iz > 255) iz = 255;
	if (RollOn) _x = ix;
	if (PitchOn) _y = iy;
	_z = iz;
}

// The pitch and roll in 360°
void PitchAccelerometerToDegree(u8 _x, u8 _y, u8 _z, int &_Roll, int &_Pitch)
{
	/* find out how much it has to move to be 1g */
	float xg = (float)g_accel.cal_g.x;
	float yg = (float)g_accel.cal_g.y;
	float zg = (float)g_accel.cal_g.z;
	float Pitch = 0, Roll = 0;

	/* find out how much it actually moved and normalize to +/- 1g */
	float x  = ((float)_x - (float)g_accel.cal_zero.x) / xg;
	float y  = ((float)_y - (float)g_accel.cal_zero.y) / yg;
	float z = ((float)_z - (float)g_accel.cal_zero.z) / zg;

	/* make sure x,y,z are between -1 and 1 for the tan function */
	if (x < -1.0)			x = -1.0;
		else if (x > 1.0)		x = 1.0;
	if (y < -1.0)			y = -1.0;
		else if (y > 1.0)		y = 1.0;
	if (z < -1.0)			z = -1.0;
		else if (z > 1.0)		z = 1.0;

	// If it is over 1g then it is probably accelerating and may not reliable
	//if (abs(accel->x - ac->cal_zero.x) <= ac->cal_g.x)
	{
		// Calculate the radian
		Roll = atan2(x, z);
		// Calculate the degree		
		Roll = (Roll * 180.0) / M_PI;
	}

	//if (abs(_y - g_accel.cal_zero.y) <= g_accel.cal_g.y)
	{
		// Calculate the radian
		Pitch = atan2(y, z);
		// Calculate the degree		
		Pitch = (Pitch * 180.0) / M_PI;
	}
	_Roll = Roll;
	_Pitch = Pitch;
}

} // WiiMoteEmu