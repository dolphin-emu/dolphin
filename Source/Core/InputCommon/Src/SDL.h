
// Project description
// -------------------
// Name: SDL Input 
// Description: Common SDL Input Functions
//
// Author: Falcon4ever (nJoy@falcon4ever.com, www.multigesture.net), JPeterson etc
// Copyright (C) 2003 Dolphin Project.
//

//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//


#ifndef _SDL_h
#define _SDL_h


// Include
// -------------------
#include <iostream> // System
#include <vector>
#include <cmath>

#ifdef _WIN32
#include <SDL.h> // Externals
#include <SDL_version.h>
#if SDL_VERSION_ATLEAST(1, 3, 0)
	#include <SDL_haptic.h>
#endif
#else
#include <SDL/SDL.h>
#include <SDL/SDL_version.h>
#if SDL_VERSION_ATLEAST(1, 3, 0)
	#include <SDL/SDL_haptic.h>
#endif
#endif

#include "Common.h" // Common



namespace InputCommon
{


// Settings
// ----------
// Show a status window with the detected axes, buttons and so on
//#define SHOW_PAD_STATUS



// Structures
/* -------------------
	CONTROLLER_STATE buttons (PadState) = 0 or 1
	CONTROLLER_MAPPING buttons (joystick) = 0 or 1, 2, 3, 4, a certain joypad button

	Please remember: The axis limit is hardcoded here, if you allow more axises (for
	example for analog A and B buttons) you must first incrase the size of the axis array
	size here
*/
struct CONTROLLER_STATE		// GC PAD INFO/STATE
{
	int buttons[8];			// Amount of buttons (A B X Y Z, L-Trigger R-Trigger Start) might need to change the triggers buttons
	int dpad;				// Automatic SDL D-Pad (8 directions + neutral)
	int dpad2[4];			// D-pad using buttons
	int axis[6];			// 2 x 2 Axes (Main & Sub)
	int halfpress;			// Halfpress... you know, like not fully pressed ;)...
	SDL_Joystick *joy;		// SDL joystick device
};

struct CONTROLLER_MAPPING	// GC PAD MAPPING
{
	int buttons[8];			// (See above)
	int dpad;				// (See above)
	int dpad2[4];			// (See above)
	int axis[6];			// (See above)
	int halfpress;			// (See above)
	int deadzone;			// Deadzone... what else?
	int ID;					// SDL joystick device ID
	int controllertype;		// Hat: Hat or custom buttons
	int triggertype;		// Triggers range
	std::string SRadius, SDiagonal, SRadiusC, SDiagonalC;
	bool bRadiusOnOff, bSquareToCircle, bRadiusOnOffC, bSquareToCircleC;
	bool rumble;
	int eventnum;			// Linux Event Number, Can't be found dynamically yet
};

struct CONTROLLER_INFO		// CONNECTED WINDOWS DEVICES INFO
{
	int NumAxes;			// Amount of Axes
	int NumButtons;			// Amount of Buttons
	int NumBalls;			// Amount of Balls
	int NumHats;			// Amount of Hats (POV)
	std::string Name;		// Joypad/stickname
	int ID;					// SDL joystick device ID
	bool Good;				// Pad is good (it has at least one button or axis)
	SDL_Joystick *joy;		// SDL joystick device
};
enum
{
	// CTL_L_SHOULDER and CTL_R_SHOULDER = 0 and 1
	CTL_MAIN_X = 2,
	CTL_MAIN_Y,
	CTL_SUB_X,
	CTL_SUB_Y
};

enum
{
	CTL_L_SHOULDER = 0,
	CTL_R_SHOULDER,
	CTL_A_BUTTON,
	CTL_B_BUTTON,
	CTL_X_BUTTON,
	CTL_Y_BUTTON,
	CTL_Z_TRIGGER,	
	CTL_START	
};
// DPad Type
enum
{
	CTL_DPAD_HAT = 0, // Automatically use the first hat that SDL finds
	CTL_DPAD_CUSTOM // Custom directional pad settings
};
// Trigger Type
enum
{
	CTL_TRIGGER_SDL = 0, // 
	CTL_TRIGGER_XINPUT // The XBox 360 pad
};
enum
{
	CTL_D_PAD_UP = 0,
	CTL_D_PAD_DOWN,
	CTL_D_PAD_LEFT,
	CTL_D_PAD_RIGHT
};
// Button type for the configuration
enum
{
	CTL_AXIS = 0,
	CTL_HAT,
	CTL_BUTTON,	
	CTL_KEY
};
// XInput buttons
enum
{
	XI_TRIGGER_L = 0,
	XI_TRIGGER_R
};


union PadAxis
{
	int keyForControls[6];
	struct
	{
		int Lx;
		int Ly;
		int Rx;
		int Ry;
		int Tl; // Trigger
		int Tr; // Trigger
	};
};
struct PadWiimote
{
	int keyForControls[16];
	// Order is A, B, 1, 2, +, -, Home
	// L, R, U, D, RollL, RollR, PitchU, PitchD, Shake
};
struct PadNunchuck
{
	int keyForControls[7];
	// Order is Z, C, L, R, U, D, Shake
};
struct PadClassicController
{
 	int keyForControls[23];
	// Order is A, B, X, Y, +, -, Home
	// Tl, Zl, Zr, Tr, Dl, Dr, Du, Dd
	// Ll, Lr, Lu, Ld, Rl, Rr, Ru, Rd
}; 
struct PadGH3Controller
{
	int keyForControls[14];
	// Order is Green, Red, Yellow, Blue, Orange,
	// +, -, Whammy, 
	// Al, Ar, Au, Ad,
	// StrumUp, StrumDown
};
struct CONTROLLER_STATE_NEW		// GC PAD INFO/STATE
{
	PadAxis Axis;			// 6 Axes (Main, Sub, Triggers)
	SDL_Joystick *joy;		// SDL joystick device
};
struct CONTROLLER_MAPPING_NEW	// GC PAD MAPPING
{
	PadAxis Axis;			// (See above)
	PadWiimote Wm;
	PadNunchuck Nc;
	PadClassicController Cc;
	PadGH3Controller GH3c;
	bool enabled;			// Pad attached?
	bool Rumble;
	int RumbleStrength;
	int DeadZoneL;			// Analog 1 Deadzone
	int DeadZoneR;			// Analog 2 Deadzone
	int ID;					// SDL joystick device ID
	int controllertype;		// D-Pad type: Hat or custom buttons
	int triggertype;		// SDL or XInput trigger
	std::string SDiagonal;
	bool bSquareToCircle;
	bool bCircle2Square;
	bool bRollInvert;
	bool bPitchInvert;
};




// Declarations
// ---------

// General functions
bool SearchDevices(std::vector<CONTROLLER_INFO> &_joyinfo, int &NumPads, int &NumGoodPads);
void GetJoyState(CONTROLLER_STATE &_PadState, CONTROLLER_MAPPING _PadMapping, int controller, int NumButtons);
void GetButton(SDL_Joystick*, int,int,int,int, int&,int&,int&,int&,bool&,bool&, bool,bool,bool,bool,bool,bool);

// Value conversion
float Deg2Rad(float Deg);
float Rad2Deg(float Rad);
int Pad_Convert(int _val);
float SquareDistance(float deg);
bool IsDeadZone(float DeadZone, int x, int y);
void Square2Circle(int &_x, int &_y, int _pad, std::string SDiagonal, bool Circle2Square = false);
void RadiusAdjustment(int &_x, int &_y, int _pad, std::string SRadius);

// Input configuration
std::string VKToString(int keycode);

#ifndef _SDL_MAIN_
	extern int g_LastPad;
#endif



} // InputCommon


#endif // _SDL_h
