
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


#define _SDL_MAIN_ // Avoid certain declarations in SDL.h
#include "InputCommon.h"
#include "SDL_Util.h" // Local
#ifdef _WIN32
#include "XInput_Util.h"
#endif

namespace InputCommon
{


// Search attached devices. Populate joyinfo for all attached physical devices.
// -----------------------
bool SearchDevices(std::vector<CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	if (!SDL_WasInit(0))
#if SDL_VERSION_ATLEAST(1, 3, 0)
		if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) < 0)
#else
		if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
#endif
		{
			PanicAlert("Could not initialize SDL: %s", SDL_GetError());
			return false;
		}

	// Get device status
	int numjoy = SDL_NumJoysticks();
	for (int i = 0; i < numjoy; i++ )
	{
		CONTROLLER_INFO Tmp;

		Tmp.joy			= SDL_JoystickOpen(i);
		Tmp.ID			= i;
		Tmp.NumAxes		= SDL_JoystickNumAxes(Tmp.joy);
		Tmp.NumButtons	= SDL_JoystickNumButtons(Tmp.joy);
		Tmp.NumBalls	= SDL_JoystickNumBalls(Tmp.joy);
		Tmp.NumHats		= SDL_JoystickNumHats(Tmp.joy);
		Tmp.Name		= SDL_JoystickName(i);

		// Check if the device is okay
		if (   Tmp.NumAxes == 0
			&&  Tmp.NumBalls == 0
			&&  Tmp.NumButtons == 0
			&&  Tmp.NumHats == 0
			)
		{
			Tmp.Good = false;
		}
		else
		{
			_NumGoodPads++;
			Tmp.Good = true;
		}

		_joyinfo.push_back(Tmp);

		// We have now read the values we need so we close the device
//		if (SDL_JoystickOpened(i)) SDL_JoystickClose(_joyinfo[i].joy);
	}

	_NumPads = (int)_joyinfo.size();

	return true;
}


// Avoid extreme axis values
// ---------------------
/* Function: We have to avoid very big values to becuse some triggers are -0x8000 in the
   unpressed state (and then go from -0x8000 to 0x8000 as they are fully pressed) */
bool AvoidValues(int value, bool NoTriggerFilter)
{
	// Avoid detecting very small or very big (for triggers) values
	if(    (value > -0x2000 && value < 0x2000) // Small values
		|| ((value < -0x6000 || value > 0x6000) && !NoTriggerFilter)) // Big values
		return true; // Avoid
	else
		return false; // Keep	
}


// Detect a pressed button
// ---------------------
void GetButton(SDL_Joystick *joy, int ControllerID, int buttons, int axes, int hats,
				int &KeyboardKey, int &value, int &type, int &pressed, bool &Succeed, bool &Stop,
				bool LeftRight, bool Axis, bool XInput, bool Button, bool Hat, bool NoTriggerFilter)
{
	// It needs the wxWidgets excape keycode
	static const int WXK_ESCAPE = 27;

	// Update the internal status
	SDL_JoystickUpdate();

	// For the triggers we accept both a digital or an analog button
	if(Axis)
	{
		for(int i = 0; i < axes; i++)
		{
			value = SDL_JoystickGetAxis(joy, i);

			if(AvoidValues(value, NoTriggerFilter)) continue; // Avoid values

			pressed = i + (LeftRight ? 1000 : 0); // Identify the analog triggers
			type = InputCommon::CTL_AXIS;
			Succeed = true;
		}
	}

	// Check for a hat
	if(Hat)
	{
		for(int i = 0; i < hats; i++)
		{	
			value = SDL_JoystickGetHat(joy, i);
			if(value)
			{
				pressed = i;
				type = InputCommon::CTL_HAT;
				Succeed = true;
			}			
		}
	}

	// Check for a button
	if(Button)
	{
		for(int i = 0; i < buttons; i++)
		{		
			// Some kind of bug in SDL 1.3 would give button 9 and 10 (nonexistent) the value 48 on the 360 pad
			if (SDL_JoystickGetButton(joy, i) > 1) continue;

			if(SDL_JoystickGetButton(joy, i))
			{
				pressed = i;
				type = InputCommon::CTL_BUTTON;
				Succeed = true;
			}
		}
	}

	// Check for a XInput trigger
	#ifdef _WIN32
		if(XInput && LeftRight)
		{
			for(int i = 0; i <= InputCommon::XI_TRIGGER_R; i++)
			{			
				if(XInput::GetXI(ControllerID, i))
				{
					pressed = i + 1000;
					type = InputCommon::CTL_AXIS;
					Succeed = true;
				}
			}
		}
	#endif

	// Check for keyboard action
	if (KeyboardKey)
	{
		if(Button)
		{
			// Todo: Add a separate keyboard vector to remove this restriction
			if(KeyboardKey >= buttons)
			{
				pressed = KeyboardKey;
				type = InputCommon::CTL_BUTTON;
				Succeed = true;
				KeyboardKey = 0;
				if(pressed == WXK_ESCAPE) pressed = -1; // Check for the escape key
			}
			// Else show the error message
			else
			{
				pressed = KeyboardKey;
				KeyboardKey = -1;
				Stop = true;
			}
		}
		// Only accept the escape key
		else if (KeyboardKey == WXK_ESCAPE)
		{	
			Succeed = true;
			KeyboardKey = 0;
			pressed = -1;
		}
	}
}


} // InputCommon

