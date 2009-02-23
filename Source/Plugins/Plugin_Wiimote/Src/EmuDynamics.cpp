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
#include "MathUtil.h"
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


//******************************************************************************
// Accelerometer functions
//******************************************************************************


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
/* Angles adjustment for the upside down state when both roll and pitch is used. When the absolute values
   of the angles go over 90° the Wiimote is upside down and these adjustments are needed. */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void AdjustAngles(float &Roll, float &Pitch)
{
	float OldPitch = Pitch;

	if (abs(Roll) > 90)
	{
		if (Pitch >= 0)
			Pitch = 180 - Pitch; // 15 to 165
		else if (Pitch < 0)	
			Pitch = -180 - Pitch; // -15 to -165
	}	

	if (abs(OldPitch) > 90)
	{
		if (Roll >= 0)
			Roll = 180 - Roll; // 15 to 165
		else if (Roll < 0)			
			Roll = -180 - Roll; // -15 to -165
	}	
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
		float x_num = 2 * tanf(0.5f * _Roll) * z;
		float x_den = pow2f(tanf(0.5f * _Roll)) - 1;
		x = - (x_num / x_den);
		float y_num = 2 * tanf(0.5f * _Pitch) * z;
		float y_den = pow2f(tanf(0.5f * _Pitch)) - 1;
		y = - (y_num / y_den);
		// =========================
	}

	// Multiply with the neutral of z and its g
	float xg = g_wm.cal_g.x;
	float yg = g_wm.cal_g.y;
	float zg = g_wm.cal_g.z;
	float x_zero = g_wm.cal_zero.x;
	float y_zero = g_wm.cal_zero.y;
	float z_zero = g_wm.cal_zero.z;
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
float AccelerometerToG(float Current, float Neutral, float G)
{
	float _G = (Current - Neutral) / G;
	return _G;
}

void PitchAccelerometerToDegree(u8 _x, u8 _y, u8 _z, int &_Roll, int &_Pitch, int &_RollAdj, int &_PitchAdj)
{
	// Definitions
	float Roll = 0, Pitch = 0;

	// Calculate how many g we are from the neutral
	float x = AccelerometerToG((float)_x, (float)g_wm.cal_zero.x, (float)g_wm.cal_g.x);
	float y = AccelerometerToG((float)_y, (float)g_wm.cal_zero.y, (float)g_wm.cal_g.y);
	float z = AccelerometerToG((float)_z, (float)g_wm.cal_zero.z, (float)g_wm.cal_g.z);

	// If it is over 1g then it is probably accelerating and may not reliable
	//if (abs(accel->x - ac->cal_zero.x) <= ac->cal_g.x)
	{
		// Calculate the degree
		Roll = InputCommon::Rad2Deg(atan2(x, z));
	}

	//if (abs(_y - g_wm.cal_zero.y) <= g_wm.cal_g.y)
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



//******************************************************************************
// IR data functions
//******************************************************************************



//////////////////////////////////////////////////////////////////////////////////////////
// Calculate dot positions from the basic 10 byte IR data
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void IRData2DotsBasic(u8 *Data)
{
	struct SDot* Dot = g_Wm.IR.Dot;

	Dot[0].Rx = 1023 - (Data[0] | ((Data[2] & 0x30) << 4));
	Dot[0].Ry = Data[1] | ((Data[2] & 0xc0) << 2);

	Dot[1].Rx = 1023 - (Data[3] | ((Data[2] & 0x03) << 8));
	Dot[1].Ry = Data[4] | ((Data[2] & 0x0c) << 6);

	Dot[2].Rx = 1023 - (Data[5] | ((Data[7] & 0x30) << 4));
	Dot[2].Ry = Data[6] | ((Data[7] & 0xc0) << 2);

	Dot[3].Rx = 1023 - (Data[8] | ((Data[7] & 0x03) << 8));
	Dot[3].Ry = Data[9] | ((Data[7] & 0x0c) << 6);

	/* set each IR spot to visible if spot is in range */
	for (int i = 0; i < 4; ++i)
	{
		if (Dot[i].Ry == 1023)
		{
			Dot[i].Visible = 0;
		}
		else
		{
			Dot[i].Visible = 1;
			Dot[i].Size = 0;		/* since we don't know the size, set it as 0 */
		}

		// For now we let our virtual resolution be the same as the default one
		Dot[i].X = Dot[i].Rx; Dot[i].Y = Dot[i].Ry;
	}

	// Calculate the other values
	ReorderIRDots();
	IRData2Distance();
}


//////////////////////////////////////////////////////////////////////////////////////////
// Calculate dot positions from the extented 12 byte IR data
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void IRData2Dots(u8 *Data)
{
	struct SDot* Dot = g_Wm.IR.Dot;

	for (int i = 0; i < 4; ++i)
	{
		//Console::Print("Rx: %i\n", Dot[i].Rx);	

		Dot[i].Rx = 1023 - (Data[3*i] | ((Data[(3*i)+2] & 0x30) << 4));
		Dot[i].Ry = Data[(3*i)+1] | ((Data[(3*i)+2] & 0xc0) << 2);

		Dot[i].Size = Data[(3*i)+2] & 0x0f;

		/* if in range set to visible */
		if (Dot[i].Ry == 1023)
			Dot[i].Visible = false;
		else
			Dot[i].Visible = true;

		//Console::Print("Rx: %i\n", Dot[i].Rx);

		// For now we let our virtual resolution be the same as the default one
		Dot[i].X = Dot[i].Rx; Dot[i].Y = Dot[i].Ry;
	}	

	// Calculate the other values
	ReorderIRDots();
	IRData2Distance();
}
////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Reorder the IR dots according to their x-axis value
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void ReorderIRDots()
{
	// Create a shortcut
	struct SDot* Dot = g_Wm.IR.Dot;

	// Variables
	int i, j, order;

	// Reset the dot ordering to zero
	for (i = 0; i < 4; ++i)
		Dot[i].Order = 0;

	for (order = 1; order < 5; ++order)
	{
		i = 0;

		//
		for (; !Dot[i].Visible || Dot[i].Order; ++i)
			if (i > 4) return;

		//
		for (j = 0; j < 4; ++j)
		{
			if (Dot[j].Visible && !Dot[j].Order && (Dot[j].X < Dot[i].X))
				i = j;
		}

		Dot[i].Order = order;
	}
}
////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Calculate dot positions from the extented 12 byte IR data
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void IRData2Distance()
{
	// Create a shortcut
	struct SDot* Dot = g_Wm.IR.Dot;

	// Make these ones global
	int i1, i2;

	for (i1 = 0; i1 < 4; ++i1)
		if (Dot[i1].Visible) break;

	// Only one dot was visible, we can not calculate the distance
	if (i1 == 4) { g_Wm.IR.Distance = 0; return; }

	// Look at the next dot
	for (i2 = i1 + 1; i2 < 4; ++i2)
		if (Dot[i2].Visible) break;

	// Only one dot was visible, we can not calculate the distance
	if (i2 == 4) { g_Wm.IR.Distance = 0; return; }

	/* For the emulated Wiimote the y distance is always zero so then the distance is the
	   simple distance between the x dots, i.e. the sensor bar width */
	int xd = Dot[i2].X - Dot[i1].X;
	int yd = Dot[i2].Y - Dot[i1].Y;

	// Save the distance
	g_Wm.IR.Distance = (int)sqrt((float)(xd*xd) + (float)(yd*yd));
}
////////////////////////////////


} // WiiMoteEmu