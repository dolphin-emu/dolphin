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

void PitchDegreeToAccelerometer(float _Degree, u8 &_y, u8 &_z)
{
	// Calculate the radian
	float d_rad = _Degree * M_PI / 180.0;

	// Calculate a good set of y and z values for the degree
	float r = 1.0;
	float fy = r * sin(d_rad); // y
	float fz = r * cos(d_rad); // x

	// Multiple with the neutral of z and its g
	float yg = g_accel.cal_g.y;
	float zg = g_accel.cal_g.z;
	float y_zero = g_accel.cal_zero.y;
	float z_zero = g_accel.cal_zero.z;
	fy = (int) (y_zero + yg * fy);
	fz = (int) (z_zero + zg * fz);

	// Boundaries
	int iy = (int)fy;
	int iz = (int)fz;
	if (iy < 0) iy = 0;
	if (iy > 255) iy = 255;
	_y = iy;
	_z = iz;
}


int PitchAccelerometerToDegree(u8 _y, u8 _z)
{
	/* find out how much it has to move to be 1g */
	float yg = (float)g_accel.cal_g.y;
	float zg = (float)g_accel.cal_g.z;
	float d = 0;

	/* find out how much it actually moved and normalize to +/- 1g */
	float y  = ((float)_y - (float)g_accel.cal_zero.y) / yg;
	float z = ((float)_z - (float)g_accel.cal_zero.z) / zg;

	/* make sure x,y,z are between -1 and 1 for the tan function */
	if (y < -1.0)			y = -1.0;
		else if (y > 1.0)		y = 1.0;
	if (z < -1.0)			z = -1.0;
		else if (z > 1.0)		z = 1.0;

	//if (abs(_y - g_accel.cal_zero.y) <= g_accel.cal_g.y)
	{
		// Calculate the radian
		d = atan2(y, z);
		// Calculate the degree		
		d = (d * 180.0) / M_PI;
	}
	return (int)d;
}

}