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
#include "Config.h" // for g_Config


namespace WiiMoteEmu
{

//******************************************************************************
// Accelerometer functions
//******************************************************************************

// Wiimote accelerometer
/* The accelerometer x, y and z values range from 0x00 to 0xff with the default
   netural values being [y = 0x84, x = 0x84, z = 0x9f] according to a
   source. The extremes are 0x00 for (-) and 0xff for (+). It's important that
   all values are not 0x80, the mouse pointer can disappear from the screen
   permanently then, until z is adjusted back. This is because the game detects
   a steep pitch of the Wiimote then.

Wiimote Accelerometer Axes

+ (- -- X -- +)
|      ___  
|     |   |\  -
|     | + ||   \ 
      | . ||    \
Y     |. .||     Z
      | . ||      \
|     | . ||       \
|     |___||        +
-       ---

*/

// Single shake step of all three directions
void ShakeToAccelerometer(int &_x, int &_y, int &_z, STiltData &_TiltData)
{
	switch(_TiltData.Shake)
	{
	case 0:
		_TiltData.Shake = -1;
		break;
	case 1:
	case 3:
		_x = g_wm.cal_zero.x / 2;
		_y = g_wm.cal_zero.y / 2;
		_z = g_wm.cal_zero.z / 2;
		break;
	case 5:
	case 7:
		_x = (0xFF - g_wm.cal_zero.x ) / 2;
		_y = (0xFF - g_wm.cal_zero.y ) / 2;
		_z = (0xFF - g_wm.cal_zero.z ) / 2;
		break;
	case 2:
		_x = 0x00;
		_y = 0x00;
		_z = 0x00;
		break;
	case 6:
		_x = 0xFF;
		_y = 0xFF;
		_z = 0xFF;
		break;
	case 4:
		_x = 0x80;
		_y = 0x80;
		_z = 0x80;
		break;
	default:
		_TiltData.Shake = -1;
		break;
	}
	_TiltData.Shake++;
	
	if (_TiltData.Shake != 0)
	{
		DEBUG_LOG(WIIMOTE, "Shake: %i - 0x%02x, 0x%02x, 0x%02x", _TiltData.Shake, _x, _y, _z);
	}
}

/* Tilting by gamepad. We can guess that the game will calculate
   roll and pitch and use them as measures of the tilting. We are
   interested in this tilting range 90 to -90*/
void TiltByGamepad(STiltData &_TiltData, int Type)
{
	// Return if we have no pads
	if (NumGoodPads == 0) return;

	/* Adjust the pad state values, including a downscaling from the original
	   0x8000 size values to 0x80. The only reason we do this is that the code
	   below crrently assume that the range is 0 to 255 for all axes. If we
	   lose any precision by doing this we could consider not doing this
	   adjustment. And instead for example upsize the XInput trigger from 0x80
	   to 0x8000. */
	int Lx = WiiMapping[g_ID].AxisState.Lx;
	int Ly = WiiMapping[g_ID].AxisState.Ly;
	int Rx = WiiMapping[g_ID].AxisState.Rx;
	int Ry = WiiMapping[g_ID].AxisState.Ry;
	int Tl = WiiMapping[g_ID].AxisState.Tl;
	int Tr = WiiMapping[g_ID].AxisState.Tr;

	// Save the Range in degrees, 45 and 90 are good values in some games
	int RollRange = WiiMapping[g_ID].Tilt.RollRange;
	int PitchRange = WiiMapping[g_ID].Tilt.PitchRange;

	// The trigger currently only controls pitch, no roll, no free swing
	if (Type == FROM_TRIGGER)
	{
		// Make the range the same dimension as the analog stick
		Tl = Tl / 2;
		Tr = Tr / 2;
		// Invert
		if (WiiMapping[g_ID].Tilt.PitchInvert) { Tl = -Tl; Tr = -Tr; }
		// The final value
		_TiltData.Pitch = (int)((float)PitchRange * ((float)(Tl - Tr) / 128.0f));
	}

	/* For the analog stick roll is by default set to the X-axis, pitch is by
	   default set to the Y-axis.  By changing the axis mapping and the invert
	   options this can be altered in any way */
	else if (Type == FROM_ANALOG1)
	{
		// Adjust the trigger to go between negative and positive values
		Lx = Lx - 0x80;
		Ly = Ly - 0x80;
		// Invert
		if (WiiMapping[g_ID].Tilt.RollInvert) Lx = -Lx; // else Tr = -Tr;
		if (WiiMapping[g_ID].Tilt.PitchInvert) Ly = -Ly; // else Tr = -Tr;
		// Produce the final value
		_TiltData.Roll = (int)((RollRange) ? (float)RollRange * ((float)Lx / 128.0f) : Lx);
		_TiltData.Pitch = (int)((PitchRange) ? (float)PitchRange * ((float)Ly / 128.0f) : Ly);
	}
	// Otherwise we are using ANALOG2
	else
	{
		// Adjust the trigger to go between negative and positive values
		Rx = Rx - 0x80;
		Ry = Ry - 0x80;
		// Invert
		if (WiiMapping[g_ID].Tilt.RollInvert) Rx = -Rx; // else Tr = -Tr;
		if (WiiMapping[g_ID].Tilt.PitchInvert) Ry = -Ry; // else Tr = -Tr;
		// Produce the final value
		_TiltData.Roll = (int)((RollRange) ? (float)RollRange * ((float)Rx / 128.0f) : Rx);
		_TiltData.Pitch = (int)((PitchRange) ? (float)PitchRange * ((float)Ry / 128.0f) : Ry);
	}
}

// Tilting by keyboard
void TiltByKeyboard(STiltData &_TiltData, int Type)
{
	int _ROLL_LEFT_ = (Type) ? ENC_ROLL_L : EWM_ROLL_L;
	int _ROLL_RIGHT_ = (Type) ? ENC_ROLL_R : EWM_ROLL_R;
	int _PITCH_UP_ = (Type) ? ENC_PITCH_U : EWM_PITCH_U;
	int _PITCH_DOWN_ = (Type) ? ENC_PITCH_D : EWM_PITCH_D;

	// Do roll/pitch or free swing
#if defined(HAVE_WX) && HAVE_WX

	if (IsKey(_ROLL_LEFT_))
	{
		if (WiiMapping[g_ID].Tilt.RollRange)
		{
			// Stop at the lower end of the range
			if (_TiltData.Roll > -WiiMapping[g_ID].Tilt.RollRange)
				_TiltData.Roll -= 3; // aim left
		}
		else // Free swing
		{
			_TiltData.Roll = -0x80 / 2;
		}
	}
	else if (IsKey(_ROLL_RIGHT_))
	{
		if (WiiMapping[g_ID].Tilt.RollRange)
		{
			// Stop at the upper end of the range
			if (_TiltData.Roll < WiiMapping[g_ID].Tilt.RollRange)
				_TiltData.Roll += 3; // aim right
		}
		else // Free swing
		{
			_TiltData.Roll = 0x80 / 2;
		}
	}
	else
	{
		_TiltData.Roll = 0;
	}
	if (IsKey(_PITCH_UP_))
	{
		if (WiiMapping[g_ID].Tilt.PitchRange)
		{
			// Stop at the lower end of the range
			if (_TiltData.Pitch > -WiiMapping[g_ID].Tilt.PitchRange)
				_TiltData.Pitch -= 3; // aim down
		}
		else // Free swing
		{
			_TiltData.Pitch = -0x80 / 2;
		}
	}
	else if (IsKey(_PITCH_DOWN_))
	{
		if (WiiMapping[g_ID].Tilt.PitchRange)
		{
			// Stop at the upper end of the range
			if (_TiltData.Pitch < WiiMapping[g_ID].Tilt.PitchRange)
				_TiltData.Pitch += 3; // aim up
		}
		else // Free swing
		{
			_TiltData.Pitch = 0x80 / 2;
		}
	}
	else
	{
		_TiltData.Pitch = 0;
	}
#endif
}

// Tilting Wiimote (Wario Land aiming, Mario Kart steering and other things)
void TiltWiimote(int &_x, int &_y, int &_z)
{
	// Select input method and return the x, y, x values
	if ((WiiMapping[g_ID].UDPWM.instance)&&(WiiMapping[g_ID].UDPWM.enableAccel))
		WiiMapping[g_ID].UDPWM.instance->getAccel(_x,_y,_z);
	else
	{
		if (WiiMapping[g_ID].Tilt.InputWM == FROM_KEYBOARD)
			TiltByKeyboard(WiiMapping[g_ID].Motion.TiltWM, 0);
		else
			TiltByGamepad(WiiMapping[g_ID].Motion.TiltWM, WiiMapping[g_ID].Tilt.InputWM);

		// Adjust angles, it's only needed if both roll and pitch is used together
		if (WiiMapping[g_ID].Tilt.RollRange && WiiMapping[g_ID].Tilt.PitchRange)
			AdjustAngles(WiiMapping[g_ID].Motion.TiltWM.Roll, WiiMapping[g_ID].Motion.TiltWM.Pitch);

		// Calculate the accelerometer value from this tilt angle
		TiltToAccelerometer(_x, _y, _z,WiiMapping[g_ID].Motion.TiltWM);
	}
	//DEBUG_LOG(WIIMOTE, "Roll:%i, Pitch:%i, _x:%u, _y:%u, _z:%u", g_Wiimote_kbd.TiltData.Roll, g_Wiimote_kbd.TiltData.Pitch, _x, _y, _z);
}

// Tilting Nunchuck (Mad World, Dead Space and other things)
void TiltNunchuck(int &_x, int &_y, int &_z)
{
	// Select input method and return the x, y, x values
	if (WiiMapping[g_ID].Tilt.InputNC == FROM_KEYBOARD)
		TiltByKeyboard(WiiMapping[g_ID].Motion.TiltNC, 1);
	else
		TiltByGamepad(WiiMapping[g_ID].Motion.TiltNC, WiiMapping[g_ID].Tilt.InputNC);

	// Adjust angles, it's only needed if both roll and pitch is used together
	if (WiiMapping[g_ID].Tilt.RollRange && WiiMapping[g_ID].Tilt.PitchRange)
		AdjustAngles(WiiMapping[g_ID].Motion.TiltNC.Roll, WiiMapping[g_ID].Motion.TiltNC.Pitch);

	// Calculate the accelerometer value from this tilt angle
	TiltToAccelerometer(_x, _y, _z, WiiMapping[g_ID].Motion.TiltNC);

	//DEBUG_LOG(WIIMOTE, "Roll:%i, Pitch:%i, _x:%u, _y:%u, _z:%u", g_NunchuckExt.TiltData.Roll, g_NunchuckExt.TiltData.Pitch, _x, _y, _z);
}

/* Angles adjustment for the upside down state when both roll and pitch is
   used. When the absolute values of the angles go over 90 the Wiimote is
   upside down and these adjustments are needed. */
void AdjustAngles(int &Roll, int &Pitch)
{
	int OldPitch = Pitch;

	if (abs(Roll) > 90)
	{
		if (Pitch >= 0)
			Pitch = 180 - Pitch; // 15 to 165
		else
			Pitch = -180 - Pitch; // -15 to -165
	}	

	if (abs(OldPitch) > 90)
	{
		if (Roll >= 0)
			Roll = 180 - Roll; // 15 to 165
		else		
			Roll = -180 - Roll; // -15 to -165
	}	
}


// Angles to accelerometer values
void TiltToAccelerometer(int &_x, int &_y, int &_z, STiltData &_TiltData)
{
	if (_TiltData.Roll == 0 && _TiltData.Pitch == 0)
		return;

	// We need radiands for the math functions
	float Roll = InputCommon::Deg2Rad((float)_TiltData.Roll);
	float Pitch = InputCommon::Deg2Rad((float)_TiltData.Pitch);
	// We need float values
	float x = 0.0f, y = 0.0f, z = 1.0f; // Gravity

	// In these cases we can use the simple and accurate formula
	if(WiiMapping[g_ID].Tilt.RollRange && !WiiMapping[g_ID].Tilt.PitchRange)
	{
		x = sin(Roll);
		z = cos(Roll);
	}
	else if (WiiMapping[g_ID].Tilt.PitchRange && !WiiMapping[g_ID].Tilt.RollRange)
	{
		y = sin(Pitch);
		z = cos(Pitch);
	}
	else if(WiiMapping[g_ID].Tilt.RollRange && WiiMapping[g_ID].Tilt.PitchRange)
	{
		// ====================================================
		/* This seems to always produce the exact same combination of x, y, z
		   and Roll and Pitch that the real Wiimote produce. There is an
		   unlimited amount of x, y, z combinations for any combination of Roll
		   and Pitch. But if we select a Z from the smallest of the absolute
		   value of cos(Roll) and cos (Pitch) we get the right values. */
		// ---------	
		if (abs(cos(Roll)) < abs(cos(Pitch)))
			z = cos(Roll);
		else
			z = cos(Pitch);
		/* I got these from reversing the calculation in
		   PitchAccelerometerToDegree() in a math program. */
		float x_num = 2 * tanf(0.5f * Roll) * z;
		float x_den = pow2f(tanf(0.5f * Roll)) - 1;
		x = - (x_num / x_den);
		float y_num = 2 * tanf(0.5f * Pitch) * z;
		float y_den = pow2f(tanf(0.5f * Pitch)) - 1;
		y = - (y_num / y_den);
	}

	// Multiply with neutral value and its g
	float xg = g_wm.cal_g.x;
	float yg = g_wm.cal_g.y;
	float zg = g_wm.cal_g.z;
	int ix = g_wm.cal_zero.x + (int)(xg * x);
	int iy = g_wm.cal_zero.y + (int)(yg * y);
	int iz = g_wm.cal_zero.z + (int)(zg * z);

	if (!WiiMapping[g_ID].bUpright)
	{
		if(WiiMapping[g_ID].Tilt.RollRange) _x = ix;
		if(WiiMapping[g_ID].Tilt.PitchRange) _y = iy;
		_z = iz;
	}
	else	// Upright wiimote
	{
		if(WiiMapping[g_ID].Tilt.RollRange) _x = ix;
		if(WiiMapping[g_ID].Tilt.PitchRange) _z = iy;
		_y = 0xFF - iz;
	}

	// Direct mapping for swing, from analog stick to accelerometer
	if (!WiiMapping[g_ID].Tilt.RollRange)
	{
		_x -= _TiltData.Roll;
	}
	if (!WiiMapping[g_ID].Tilt.PitchRange)
	{
		if (!WiiMapping[g_ID].bUpright)
			_z -= _TiltData.Pitch;
		else	// Upright wiimote
			_y += _TiltData.Pitch;
	}
}

// Rotate IR dot when rolling Wiimote
void RotateIRDot(int &_x, int &_y, STiltData &_TiltData)
{
	if (!WiiMapping[g_ID].Tilt.RollRange || !_TiltData.Roll)
		return;

	// The IR camera resolution is 1023x767
	float dot_x = _x - 1023.0f / 2;
	float dot_y = _y - 767.0f / 2;

	float radius = sqrt(pow(dot_x, 2) + pow(dot_y, 2));
	float radian = atan2(dot_y, dot_x);

	_x = (int)(radius * cos(radian + InputCommon::Deg2Rad((float)_TiltData.Roll)) + 1023.0f / 2);
	_y = (int)(radius * sin(radian + InputCommon::Deg2Rad((float)_TiltData.Roll)) + 767.0f / 2);

	// Out of sight check
	if (_x < 0 || _x > 1023) _x = 0xFFFF;
	if (_y < 0 || _y > 767) _y = 0xFFFF;
}

} // WiiMoteEmu
