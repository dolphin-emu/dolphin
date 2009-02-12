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


//////////////////////////////////////////////////////////////////////////////////////////
// Test the calculations
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void TiltTest(u8 x, u8 y, u8 z)
{
	int Roll, Pitch, RollAdj, PitchAdj;
	PitchAccelerometerToDegree(x, y, z, Roll, Pitch, RollAdj, PitchAdj);
	std::string From = StringFromFormat("From: X:%i Y:%i Z:%i Roll:%s Pitch:%s", x, y, z,
		(Roll >= 0) ? StringFromFormat(" %03i", Roll).c_str() : StringFromFormat("%04i", Roll).c_str(),
		(Pitch >= 0) ? StringFromFormat(" %03i", Pitch).c_str() : StringFromFormat("%04i", Pitch).c_str());

	float _Roll = (float)Roll, _Pitch = (float)Pitch;
	PitchDegreeToAccelerometer(_Roll, _Pitch, x, y, z);
	std::string To = StringFromFormat("%s\nTo:   X:%i Y:%i Z:%i Roll:%s Pitch:%s", From.c_str(), x, y, z,
		(_Roll >= 0) ? StringFromFormat(" %03i", (int)_Roll).c_str() : StringFromFormat("%04i", (int)_Roll).c_str(),
		(_Pitch >= 0) ? StringFromFormat(" %03i", (int)_Pitch).c_str() : StringFromFormat("%04i", (int)_Pitch).c_str());
	Console::Print("%s\n", To.c_str());
}
////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Angles to accelerometer values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void PitchDegreeToAccelerometer(float _Roll, float _Pitch, u8 &_x, u8 &_y, u8 &_z)
{
	// We need radiands for the math functions
	_Roll = InputCommon::Deg2Rad(_Roll);
	_Pitch = InputCommon::Deg2Rad(_Pitch);
	// We need decimal values
	float x = (float)_x, y = (float)_y, z = (float)_z;

	// In these cases we can use the simple and accurate formula
	if(g_Config.Trigger.Range.Pitch == 0)
	{
		x = sin(_Roll);
		z = cos(_Roll);
	}
	else if (g_Config.Trigger.Range.Roll == 0)
	{

		y = sin(_Pitch);
		z = cos(_Pitch);
	}
	else
	{
		// ====================================================
		/* This seems to always produce the exact same combination of x, y, z and Roll and Pitch that the
		   real Wiimote produce. There is an unlimited amount of x, y, z combinations for any combination of
		   Roll and Pitch. But if we select a Z from the smallest of the absolute value of cos(Roll) and
		   cos (Pitch) we get the right values. */
		// ---------	
		if (abs(cos(_Roll)) < abs(cos(_Pitch))) z = cos(_Roll); else z = cos(_Pitch);
		/* I got these from reversing the calculation in PitchAccelerometerToDegree() in a math program
		   I don't know if we can derive these from some kind of matrix or something */
		float x_num = 2 * tan(0.5 * _Roll) * z;
		float x_den = pow(tan(0.5 * _Roll),2) - 1;
		x = - (x_num / x_den);
		float y_num = 2 * tan(0.5 * _Pitch) * z;
		float y_den = pow(tan(0.5 * _Pitch), 2) - 1;
		y = - (y_num / y_den);
		// =========================
	}

	// Multiply with the neutral of z and its g
	float xg = g_accel.cal_g.x;
	float yg = g_accel.cal_g.y;
	float zg = g_accel.cal_g.z;
	float x_zero = g_accel.cal_zero.x;
	float y_zero = g_accel.cal_zero.y;
	float z_zero = g_accel.cal_zero.z;
	int ix = (int) (x_zero + xg * x);
	int iy = (int) (y_zero + yg * y);
	int iz = (int) (z_zero + zg * z);

	// Boundaries
	if (ix < 0) ix = 0; if (ix > 255) ix = 255;
	if (iy < 0) iy = 0; if (iy > 255) iy = 255;
	if (iz < 0) iz = 0; if (iz > 255) iz = 255;
	if(g_Config.Trigger.Range.Roll != 0) _x = ix;
	if(g_Config.Trigger.Range.Pitch != 0) _y = iy;
	_z = iz;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Accelerometer to roll and pitch angles
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void PitchAccelerometerToDegree(u8 _x, u8 _y, u8 _z, int &_Roll, int &_Pitch, int &_RollAdj, int &_PitchAdj)
{
	/* find out how much it has to move to be 1g */
	float xg = (float)g_accel.cal_g.x;
	float yg = (float)g_accel.cal_g.y;
	float zg = (float)g_accel.cal_g.z;
	float Roll = 0, Pitch = 0;

	// Calculate how many g we are from the neutral
	float x = ((float)_x - (float)g_accel.cal_zero.x) / xg;
	float y = ((float)_y - (float)g_accel.cal_zero.y) / yg;
	float z = ((float)_z - (float)g_accel.cal_zero.z) / zg;

	// If it is over 1g then it is probably accelerating and may not reliable
	//if (abs(accel->x - ac->cal_zero.x) <= ac->cal_g.x)
	{
		// Calculate the degree
		Roll = InputCommon::Rad2Deg(atan2(x, z));
	}

	//if (abs(_y - g_accel.cal_zero.y) <= g_accel.cal_g.y)
	{
		// Calculate the degree
		Pitch = InputCommon::Rad2Deg(atan2(y, z));
	}

	_Roll = (int)Roll;
	_Pitch = (int)Pitch;

	/* Don't allow forces bigger than 1g */
	if (x < -1.0) x = -1.0; else if (x > 1.0) x = 1.0;
	if (y < -1.0) y = -1.0; else if (y > 1.0) y = 1.0;
	if (z < -1.0) z = -1.0; else if (z > 1.0) z = 1.0;
	Roll = InputCommon::Rad2Deg(atan2(x, z));
	Pitch = InputCommon::Rad2Deg(atan2(y, z));

	_RollAdj = (int)Roll;
	_PitchAdj = (int)Pitch;
}

} // WiiMoteEmu