// Copyright (C) 2009 Dolphin Project.

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

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!                                                                        !!
// !!                          THIS CODE IS UNUSED                           !!
// !!                                                                        !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#include "FakeAccelerometer.h"

namespace WiiMoteEmu
{

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

void FakeAccelerometer::StartShake() {
	StartShake(*this);
}

void FakeAccelerometer::StartShake(ShakeData &shakeData) {
	if (shakeData.Shake <= 0) shakeData.Shake = 1;
}

// Single shake step of all three directions
void FakeAccelerometer::SingleShake() {
	SingleShake(this->x, this->y, this->z, *((ShakeData*)this));
}

void FakeAccelerometer::SingleShake(int &_x, int &_y, int &_z, ShakeData &shakeData)
{
// 	if (shakeData.Shake == 0)
// 	{
// 		if((wm == 0 && IsKey(g_Wiimote_kbd.SHAKE)) || (wm == 1 && IsKey(g_NunchuckExt.SHAKE)))
// 			Shake[wm] = 1;
// 	}
	switch(shakeData.Shake)
	{
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
		shakeData.Shake = -1;
		break;
	}
	shakeData.Shake++;
	//if (Shake[wm] != 0) DEBUG_LOG(WIIMOTE, "Shake: %i - 0x%02x, 0x%02x, 0x%02x", Shake[wm], _x, _y, _z);
}


/* Tilting Wiimote with gamepad. We can guess that the game will calculate a
   Wiimote pitch and use it as a measure of the tilting of the Wiimote. We are
   interested in this tilting range 90 to -90*/
void FakeAccelerometer::TiltWiimoteGamepad() {
	TiltWiimoteGamepad(this->Roll, this->Pitch);
}

void FakeAccelerometer::TiltWiimoteGamepad(int &Roll, int &Pitch)
{
	// Return if we have no pads
	if (NumGoodPads == 0) return;

	// This has to be changed if multiple Wiimotes are to be supported later
	const int Page = 0;

	/* Adjust the pad state values, including a downscaling from the original
	   0x8000 size values to 0x80. The only reason we do this is that the code
	   below crrently assume that the range is 0 to 255 for all axes. If we
	   lose any precision by doing this we could consider not doing this
	   adjustment. And instead for example upsize the XInput trigger from 0x80
	   to 0x8000. */
	int Lx, Ly, Rx, Ry, Tl, Tr;
	PadStateAdjustments(Lx, Ly, Rx, Ry, Tl, Tr);

	// Save the Range in degrees, 45 and 90 are good values in some games
	int &RollRange = g_Config.Tilt.Range.Roll;
	int &PitchRange = g_Config.Tilt.Range.Pitch;

	// The trigger currently only controls pitch
	if (g_Config.Tilt.Type == g_Config.Tilt.TRIGGER)
	{
		// Make the range the same dimension as the analog stick
		Tl = Tl / 2;
		Tr = Tr / 2;
		// Invert
		if (PadMapping[Page].bPitchInvert) { Tl = -Tl; Tr = -Tr; }
		// The final value
		Pitch = (float)PitchRange * ((float)(Tl - Tr) / 128.0f);
	}

	/* For the analog stick roll is by default set to the X-axis, pitch is by
	   default set to the Y-axis.  By changing the axis mapping and the invert
	   options this can be altered in any way */
	else if (g_Config.Tilt.Type == g_Config.Tilt.ANALOG1)
	{
		// Adjust the trigger to go between negative and positive values
		Lx = Lx - 0x80;
		Ly = Ly - 0x80;
		// Invert
		if (PadMapping[Page].bRollInvert) Lx = -Lx; // else Tr = -Tr;
		if (PadMapping[Page].bPitchInvert) Ly = -Ly; // else Tr = -Tr;
		// Produce the final value
		Roll = (RollRange) ? (float)RollRange * ((float)Lx / 128.0f) : Lx;
		Pitch = (PitchRange) ? (float)PitchRange * ((float)Ly / 128.0f) : Ly;
	}
	// Otherwise we are using ANALOG2
	else
	{
		// Adjust the trigger to go between negative and positive values
		Rx = Rx - 0x80;
		Ry = Ry - 0x80;
		// Invert
		if (PadMapping[Page].bRollInvert) Rx = -Rx; // else Tr = -Tr;
		if (PadMapping[Page].bPitchInvert) Ry = -Ry; // else Tr = -Tr;
		// Produce the final value
		Roll = (RollRange) ? (float)RollRange * ((float)Rx / 128.0f) : Rx;
		Pitch = (PitchRange) ? (float)PitchRange * ((float)Ry / 128.0f) : Ry;
	}
}


// Tilting Wiimote with keyboard
void FakeAccelerometer::TiltWiimoteKeyboard() {
	TiltWiimoteKeyboard(this->Roll, this->Pitch);
}

void FakeAccelerometer::TiltWiimoteKeyboard(int &Roll, int &Pitch)
{
	// Direct map roll/pitch to swing
	if (g_Config.Tilt.Range.Roll == 0 && g_Config.Tilt.Range.Pitch == 0)
	{
		if (IsKey(g_Wiimote_kbd.ROLL_L))
			Roll = -0x80 / 2;
		else if (IsKey(g_Wiimote_kbd.ROLL_R))
			Roll = 0x80 / 2;
		else
			Roll = 0;
		if (IsKey(g_Wiimote_kbd.PITCH_U))
			Pitch = -0x80 / 2;
		else if (IsKey(g_Wiimote_kbd.PITCH_D))
			Pitch = 0x80 / 2;
		else
			Pitch = 0;
		return;
	}

	// Otherwise do roll/pitch
	if (IsKey(g_Wiimote_kbd.ROLL_L))
	{
		// Stop at the upper end of the range
		if (Roll < g_Config.Tilt.Range.Roll)
			Roll += 3; // aim left
	}
	else if (IsKey(g_Wiimote_kbd.ROLL_R))
	{
		// Stop at the lower end of the range
		if (Roll > -g_Config.Tilt.Range.Roll)
			Roll -= 3; // aim right
	}
	else
	{
		Roll = 0;
	}
	if (IsKey(g_Wiimote_kbd.PITCH_U))
	{
		// Stop at the upper end of the range
		if (Pitch < g_Config.Tilt.Range.Pitch)
			Pitch += 3; // aim up
	}
	else if (IsKey(g_Wiimote_kbd.PITCH_D))
	{
		// Stop at the lower end of the range
		if (Pitch > -g_Config.Tilt.Range.Pitch)
			Pitch -= 3; // aim down
	}
	else
	{
		Pitch = 0;
	}
}

// Tilting Wiimote (Wario Land aiming, Mario Kart steering and other things)
void FakeAccelerometer::Tilt() {
	Tilt(this->x, this->y, this->z);
}

void FakeAccelerometer::Tilt(int &_x, int &_y, int &_z)
{
	// Check if it's on
	if (g_Config.Tilt.Type == g_Config.Tilt.OFF) return;

	// Select input method and return the x, y, x values
	if (g_Config.Tilt.Type == g_Config.Tilt.KEYBOARD)
		TiltWiimoteKeyboard();
	else if (g_Config.Tilt.Type == g_Config.Tilt.TRIGGER || g_Config.Tilt.Type == g_Config.Tilt.ANALOG1 || g_Config.Tilt.Type == g_Config.Tilt.ANALOG2)
		TiltWiimoteGamepad();

	// Adjust angles, it's only needed if both roll and pitch is used together
	if (g_Config.Tilt.Range.Roll != 0 && g_Config.Tilt.Range.Pitch != 0)
		AdjustAngles(Roll, Pitch);

	// Calculate the accelerometer value from this tilt angle
	PitchDegreeToAccelerometer(Roll, Pitch, _x, _y, _z);

	//DEBUG_LOG(WIIMOTE, "Roll:%i, Pitch:%i, _x:%u, _y:%u, _z:%u", Roll, Pitch, _x, _y, _z);
}

void FakeAccelerometer::FillReportAcc(wm_accel& _acc)
{
	// Recorded movements
	// Check for a playback command
	if(g_RecordingPlaying[0] < 0)
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
	x = g_wm.cal_zero.x;
	y = g_wm.cal_zero.y;
	z = g_wm.cal_zero.z;

	if (!g_Config.bUpright)
		z += g_wm.cal_g.z;
	else	// Upright wiimote
		y -= g_wm.cal_g.y;

	// Check that Dolphin is in focus
	if (IsFocus())
	{
		// Check for shake button
		if(IsKey(g_Wiimote_kbd.SHAKE)) StartShake();
		// Step the shake simulation one step
		SingleShake();

		// Tilt Wiimote, allow the shake function to interrupt it
		if (g_Wiimote_kbd.shakeData.Shake == 0)	Tilt();
	
		// Boundary check
		if (x > 0xFF)		x = 0xFF;
		else if (x < 0x00)	x = 0x00;
		if (y > 0xFF)		y = 0xFF;
		else if (y < 0x00)	y = 0x00;
		if (z > 0xFF)		z = 0xFF;
		else if (z < 0x00)	z = 0x00;
	}

	_acc.x = x;
	_acc.y = y;
	_acc.z = z;

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


} // namespace
