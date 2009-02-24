//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: SDL Input 
// Description: Common SDL Input Functions
//
// Author: Falcon4ever (nJoy@falcon4ever.com, www.multigesture.net), JPeterson etc
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#define _SDL_MAIN_ // Avoid certain declarations in SDL.h
#include "SDL.h" // Local
#include "XInput.h"
////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int g_LastPad = 0;
////////////////////////////////////


namespace InputCommon
{

//////////////////////////////////////////////////////////////////////////////////////////
// Definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Search attached devices. Populate joyinfo for all attached physical devices.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
bool SearchDevices(std::vector<CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	/* SDL 1.3 use DirectInput instead of the old Microsoft Multimedia API, and with this we need 
	   the SDL_INIT_VIDEO flag to */
	if (!SDL_WasInit(0))
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
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
		if (SDL_JoystickOpened(i)) SDL_JoystickClose(_joyinfo[i].joy);
	}

	_NumPads = (int)_joyinfo.size();

	return true;
}
////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Supporting functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Read current joystick status
/* ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
	The value PadMapping[].buttons[] is the number of the assigned joypad button,
	PadState[].buttons[] is the status of the button, it becomes 0 (no pressed) or 1 (pressed) */


// Read buttons status. Called from GetJoyState().
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ReadButton(CONTROLLER_STATE &_PadState, CONTROLLER_MAPPING _PadMapping, int button, int NumButtons)
{
	int ctl_button = _PadMapping.buttons[button];
	if (ctl_button < NumButtons)
	{
		_PadState.buttons[button] = SDL_JoystickGetButton(_PadState.joy, ctl_button);
	}
}

// Request joystick state.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Called from: PAD_GetStatus()
   Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input value "controller" is
   for a virtual controller 0 to 3. */
void GetJoyState(CONTROLLER_STATE &_PadState, CONTROLLER_MAPPING _PadMapping, int Controller, int NumButtons)
{
	// Update the gamepad status
	SDL_JoystickUpdate();

	// Update axis states. It doesn't hurt much if we happen to ask for nonexisting axises here.
	_PadState.axis[CTL_MAIN_X] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.axis[CTL_MAIN_X]);
	_PadState.axis[CTL_MAIN_Y] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.axis[CTL_MAIN_Y]);
	_PadState.axis[CTL_SUB_X] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.axis[CTL_SUB_X]);
	_PadState.axis[CTL_SUB_Y] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.axis[CTL_SUB_Y]);

	// Update the analog trigger axis values
#ifdef _WIN32
	if (_PadMapping.triggertype == CTL_TRIGGER_SDL)
	{
#endif
		// If we are using SDL analog triggers the buttons have to be mapped as 1000 or up, otherwise they are not used
		if(_PadMapping.buttons[CTL_L_SHOULDER] >= 1000) _PadState.axis[CTL_L_SHOULDER] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.buttons[CTL_L_SHOULDER] - 1000); else _PadState.axis[CTL_L_SHOULDER] = 0;
		if(_PadMapping.buttons[CTL_R_SHOULDER] >= 1000) _PadState.axis[CTL_R_SHOULDER] = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.buttons[CTL_R_SHOULDER] - 1000); else _PadState.axis[CTL_R_SHOULDER] = 0;
#ifdef _WIN32
	}
	else
	{
		_PadState.axis[CTL_L_SHOULDER] = XInput::GetXI(0, _PadMapping.buttons[CTL_L_SHOULDER] - 1000);
		_PadState.axis[CTL_R_SHOULDER] = XInput::GetXI(0, _PadMapping.buttons[CTL_R_SHOULDER] - 1000);
	}
#endif

	// Update button states to on or off
	ReadButton(_PadState, _PadMapping, CTL_L_SHOULDER, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_R_SHOULDER, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_A_BUTTON, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_B_BUTTON, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_X_BUTTON, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_Y_BUTTON, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_Z_TRIGGER, NumButtons);
	ReadButton(_PadState, _PadMapping, CTL_START, NumButtons);

	//
	if (_PadMapping.halfpress < NumButtons)
		_PadState.halfpress = SDL_JoystickGetButton(_PadState.joy, _PadMapping.halfpress);

	// Check if we have an analog or digital joypad
	if (_PadMapping.controllertype == CTL_DPAD_HAT)
	{
		_PadState.dpad = SDL_JoystickGetHat(_PadState.joy, _PadMapping.dpad);
	}
	else
	{
		/* Only do this if the assigned button is in range (to allow for the current way of saving keyboard
		   keys in the same array) */
		if(_PadMapping.dpad2[CTL_D_PAD_UP] <= NumButtons)
			_PadState.dpad2[CTL_D_PAD_UP] = SDL_JoystickGetButton(_PadState.joy, _PadMapping.dpad2[CTL_D_PAD_UP]);
		if(_PadMapping.dpad2[CTL_D_PAD_DOWN] <= NumButtons)
			_PadState.dpad2[CTL_D_PAD_DOWN] = SDL_JoystickGetButton(_PadState.joy, _PadMapping.dpad2[CTL_D_PAD_DOWN]);
		if(_PadMapping.dpad2[CTL_D_PAD_LEFT] <= NumButtons)
			_PadState.dpad2[CTL_D_PAD_LEFT] = SDL_JoystickGetButton(_PadState.joy, _PadMapping.dpad2[CTL_D_PAD_LEFT]);
		if(_PadMapping.dpad2[CTL_D_PAD_RIGHT] <= NumButtons)
			_PadState.dpad2[CTL_D_PAD_RIGHT] = SDL_JoystickGetButton(_PadState.joy, _PadMapping.dpad2[CTL_D_PAD_RIGHT]);
	}

	#ifdef SHOW_PAD_STATUS
	// Show the status of all connected pads
	if ((g_LastPad == 0 && Controller == 0) || Controller < g_LastPad) Console::ClearScreen();	
	g_LastPad = Controller;
	Console::Print(
		"Pad        | Number:%i Enabled:%i Handle:%i\n"
		"Main Stick | X:%03i  Y:%03i\n"
		"C Stick    | X:%03i  Y:%03i\n"
		"Trigger    | Type:%s DigitalL:%i DigitalR:%i AnalogL:%03i AnalogR:%03i HalfPress:%i\n"
		"Buttons    | A:%i X:%i\n"
		"D-Pad      | Type:%s Hat:%i U:%i D:%i\n"
		"======================================================\n",

		Controller, _PadMapping.enabled, _PadState.joy,

		_PadState.axis[InputCommon::CTL_MAIN_X], _PadState.axis[InputCommon::CTL_MAIN_Y],
		_PadState.axis[InputCommon::CTL_SUB_X], _PadState.axis[InputCommon::CTL_SUB_Y],

		(_PadMapping.triggertype ? "CTL_TRIGGER_XINPUT" : "CTL_TRIGGER_SDL"),
		_PadState.buttons[InputCommon::CTL_L_SHOULDER], _PadState.buttons[InputCommon::CTL_R_SHOULDER],
		_PadState.axis[InputCommon::CTL_L_SHOULDER], _PadState.axis[InputCommon::CTL_R_SHOULDER],
		_PadState.halfpress,

		_PadState.buttons[InputCommon::CTL_A_BUTTON], _PadState.buttons[InputCommon::CTL_X_BUTTON],

		(_PadMapping.controllertype ? "CTL_DPAD_CUSTOM" : "CTL_DPAD_HAT"),
			_PadState.dpad,
			_PadState.dpad2[InputCommon::CTL_D_PAD_UP], _PadState.dpad2[InputCommon::CTL_D_PAD_DOWN]
		);
	#endif
}
//////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////
// Configure button mapping
// ¯¯¯¯¯¯¯¯¯¯

// Avoid extreme axis values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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
			if(SDL_JoystickGetHat(joy, i))
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
		if(XInput)
		{
			for(int i = 0; i <= InputCommon::XI_TRIGGER_R; i++)
			{			
				if(XInput::GetXI(0, i))
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
/////////////////////////////////////////////////////////// Configure button mapping



} // InputCommon

