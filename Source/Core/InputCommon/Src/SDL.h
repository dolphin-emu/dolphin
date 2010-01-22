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

#ifndef _SDL_h
#define _SDL_h


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

enum EButtonType
{
	CTL_AXIS = 0,
	CTL_HAT,
	CTL_BUTTON,	
	CTL_KEY,
};

enum ETriggerType
{
	CTL_TRIGGER_SDL = 0,
	CTL_TRIGGER_XINPUT,
};

enum EXInputTrigger
{
	XI_TRIGGER_L = 0,
	XI_TRIGGER_R,
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


// General functions
bool SearchDevices(std::vector<CONTROLLER_INFO> &_joyinfo, int &NumPads, int &NumGoodPads);
void GetButton(SDL_Joystick*, int,int,int,int, int&,int&,int&,int&,bool&,bool&, bool,bool,bool,bool,bool,bool);

// Value conversion
float Deg2Rad(float Deg);
float Rad2Deg(float Rad);
int Pad_Convert(int _val);
float SquareDistance(float deg);
bool IsDeadZone(float DeadZone, int x, int y);
void Square2Circle(int &_x, int &_y, int _Diagonal, bool Circle2Square = false);
void RadiusAdjustment(s8 &_x, s8 &_y, int _Radius);
// Input configuration
std::string VKToString(int keycode);


} // InputCommon


#endif // _SDL_h
