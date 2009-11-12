
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




// Include
// -------------------
#define _SDL_MAIN_ // Avoid certain declarations in SDL.h
#include "SDL.h" // Local
#include "XInput.h"
#ifdef _WIN32
#include <windows.h>
#include <dinput.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")
#endif




namespace InputCommon
{


// Definitions
// -------------------
int g_LastPad = 0;
int NumDIDevices = 0;


// Reset and search for devices
// -----------------------
bool SearchDevicesReset(std::vector<CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	// This is needed to update SDL_NumJoysticks
	if (SDL_WasInit(0))
		SDL_Quit();
	
	return SearchDevices(_joyinfo, _NumPads);
}

// Initialize if not previously initialized
// -----------------------
bool SearchDevices(std::vector<CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	// Init Joystick + Haptic (force feedback) subsystem on SDL 1.3
	if (!SDL_WasInit(0))
	{		
#if SDL_VERSION_ATLEAST(1, 3, 0) && !defined(_WIN32)
		NOTICE_LOG(PAD, "SDL_Init | HAPTIC");
		if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) < 0)
#else
		//NOTICE_LOG(PAD, "SDL_Init");
		if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
#endif
		{
			PanicAlert("Could not initialize SDL: %s", SDL_GetError());
			return false;
		}
	}

	// Clear joyinfo
	_joyinfo.clear();
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
		if ( !(  Tmp.NumAxes == 0
			&&  Tmp.NumBalls == 0
			&&  Tmp.NumButtons == 0
			&&  Tmp.NumHats == 0)
			)
		{
			_joyinfo.push_back(Tmp);
		}
		else
		{
			//if (SDL_JoystickOpened(i)) SDL_JoystickClose(Tmp.joy);
		}	
	}

	_NumPads = (int)_joyinfo.size();
	return true;
}


// Show the current pad status
// -----------------
std::string ShowStatus(int Slot, int Device, CONTROLLER_MAPPING PadMapping[], CONTROLLER_STATE PadState[],
	std::vector<InputCommon::CONTROLLER_INFO> joyinfo)
{
	CONTROLLER_MAPPING_NEW _PadMapping[4];
	CONTROLLER_STATE_NEW _PadState[4];
	for (int i = 0; i < 4; i++)
	{
		_PadMapping[i].ID = PadMapping[i].ID;
		_PadMapping[i].Name = PadMapping[i].Name;
		_PadState[i].joy = PadState[i].joy;
	}
	return DoShowStatus(Slot, Device, _PadMapping, _PadState, joyinfo);
}
std::string DoShowStatus(int Slot, int Device,
	CONTROLLER_MAPPING_NEW PadMapping[], CONTROLLER_STATE_NEW PadState[],
	std::vector<InputCommon::CONTROLLER_INFO> joyinfo)
{
	// Save the physical device
	int ID = PadMapping[Slot].ID;
	// Make local shortcut
	SDL_Joystick *joy = PadState[Slot].joy;

	// Make shortcuts for all pads
	SDL_Joystick *joy0 = PadState[0].joy;
	SDL_Joystick *joy1 = PadState[1].joy;
	SDL_Joystick *joy2 = PadState[2].joy;
	SDL_Joystick *joy3 = PadState[3].joy;

	// Temporary storage
	std::string
		StrAllHandles, StrAllName,
		StrHandles, StrId, StrName,
		StrAxes, StrHats, StrBut;
	int value;

	// All devices
	int numjoy = SDL_NumJoysticks();
	for (int i = 0; i < numjoy; i++ )
	{
		SDL_Joystick *AllJoy = SDL_JoystickOpen(i);
		StrAllHandles += StringFromFormat(" %i:%06i", i, AllJoy);
		StrAllName += StringFromFormat("Name %i: %s\n", i, SDL_JoystickName(i));
	}

	// Get handles
	for(int i = 0; i < joyinfo.size(); i++)
	{	
		StrHandles += StringFromFormat(" %i:%06i", i, joyinfo.at(i).joy);	
		StrId += StringFromFormat(" %i:%i", i, joyinfo.at(i).ID);
		StrName += StringFromFormat("Name %i:%s\n", i, joyinfo.at(i).Name.c_str());
	}

	// Get status
	int Axes = joyinfo[Device].NumAxes;
	int Balls = joyinfo[Device].NumBalls;
	int Hats = joyinfo[Device].NumHats;
	int Buttons = joyinfo[Device].NumButtons;

	// Update the internal values
	SDL_JoystickUpdate();

	// Go through all axes and read out their values
	for(int i = 0; i < Axes; i++)
	{	
		value = SDL_JoystickGetAxis(joy, i);
		StrAxes += StringFromFormat(" %i:%06i", i, value);
	}
	for(int i = 0;i < Hats; i++)
	{	
		value = SDL_JoystickGetHat(joy, i);
		StrHats += StringFromFormat(" %i:%i", i, value);
	}
	for(int i = 0;i < Buttons; i++)
	{	
		value = SDL_JoystickGetButton(joy, i);
		StrBut += StringFromFormat(" %i:%i", i+1, value);
	}

	return StringFromFormat(
		"All devices:\n"
		"Handles: %s\n"
		"%s"
	
		"\nAll pads:\n"
		"Handles: %s\n"
		"ID: %s\n"
		"%s"
		
		"\nAll slots:\n"
		"ID: %i %i %i %i\n"
		"Name: '%s' '%s' '%s' '%s'\n"
		//"Controllertype: %i %i %i %i\n"
		//"SquareToCircle: %i %i %i %i\n\n"
		#ifdef _WIN32
			"Handles: %i %i %i %i\n"
			//"XInput: %i %i %i\n"
		#endif

		"\nThis pad:\n"
		"Handle: %06i\n"
		"ID: %i\n"
		//"Slot: %i\n"
		"Axes: %s\n"
		"Hats: %s\n"
		"But: %s\n"
		"Device: Ax: %i Balls:%i Hats:%i But:%i",
		StrAllHandles.c_str(), StrAllName.c_str(),
		StrHandles.c_str(), StrId.c_str(), StrName.c_str(),
		
		PadMapping[0].ID, PadMapping[1].ID, PadMapping[2].ID, PadMapping[3].ID,
		PadMapping[0].Name.c_str(), PadMapping[1].Name.c_str(), PadMapping[2].Name.c_str(), PadMapping[3].Name.c_str(),
		//PadMapping[0].controllertype, PadMapping[1].controllertype, PadMapping[2].controllertype, PadMapping[3].controllertype,
		//PadMapping[0].bSquareToCircle, PadMapping[1].bSquareToCircle, PadMapping[2].bSquareToCircle, PadMapping[3].bSquareToCircle,
		#ifdef _WIN32
			joy0, joy1, joy2, joy3,
			//PadState[PadMapping[0].ID].joy, PadState[PadMapping[1].ID].joy, PadState[PadMapping[2].ID].joy, PadState[PadMapping[3].ID].joy,
			//XInput::IsConnected(0), XInput::GetXI(0, InputCommon::XI_TRIGGER_L), XInput::GetXI(0, InputCommon::XI_TRIGGER_R),
			joy,
		#endif
		//Slot,		
		ID,
		StrAxes.c_str(), StrHats.c_str(), StrBut.c_str(),
		Axes, Balls, Hats, Buttons
		);
}



// Supporting functions
// ====================

// Read current joystick status
/* --------------------
	The value PadMapping[].buttons[] is the number of the assigned joypad button,
	PadState[].buttons[] is the status of the button, it becomes 0 (no pressed) or 1 (pressed) */


// Read buttons status. Called from GetJoyState().
// ----------------------
void ReadButton(CONTROLLER_STATE &_PadState, CONTROLLER_MAPPING _PadMapping, int button, int NumButtons)
{
	int ctl_button = _PadMapping.buttons[button];
	if (ctl_button < NumButtons)
	{
		_PadState.buttons[button] = SDL_JoystickGetButton(_PadState.joy, ctl_button);
	}
}

// Request joystick state.
// ----------------------
/* Called from: PAD_GetStatus()
   Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input value "controller" is
   for a virtual controller 0 to 3. */
void GetJoyState(CONTROLLER_STATE &_PadState, CONTROLLER_MAPPING _PadMapping)
{	
	if (SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE) return;
	//NOTICE_LOG(PAD, "SDL_JoystickEventState: %s", (SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE) ? "SDL_ENABLE" : "SDL_IGNORE");

	// Update the gamepad status
	SDL_JoystickUpdate();
	// Read info
	int NumButtons = SDL_JoystickNumButtons(_PadState.joy);

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
		// XInput triggers for Xbox360 pads
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

	// Update Halfpress state, this one is not in the standard _PadState.buttons array
	if (_PadMapping.halfpress < NumButtons && _PadMapping.halfpress >= 0)
		_PadState.halfpress = SDL_JoystickGetButton(_PadState.joy, _PadMapping.halfpress);
	else
		_PadState.halfpress = 0;


	// Check if we have an analog or digital joypad
	if (_PadMapping.controllertype == CTL_DPAD_HAT)
	{
		_PadState.dpad = SDL_JoystickGetHat(_PadState.joy, _PadMapping.dpad);
	}
	else
	{
		// Only do this if the assigned button is in range (to allow for the current way of saving keyboard
		// keys in the same array) 
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
	/*
	// Show the status of all connected pads
	//ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	//if ((g_LastPad == 0 && _PadMapping.ID == 0) || Controller < g_LastPad) Console->ClearScreen();	
	g_LastPad = Controller;
	NOTICE_LOG(CONSOLE, 
		"Pad        | Number:%i Handle:%i\n"
		"Main Stick | X:%03i  Y:%03i\n"
		"C Stick    | X:%03i  Y:%03i\n"
		"Trigger    | Type:%s DigitalL:%i DigitalR:%i AnalogL:%03i AnalogR:%03i HalfPress:%i\n"
		"Buttons    | A:%i X:%i\n"
		"D-Pad      | Type:%s Hat:%i U:%i D:%i\n"
		"======================================================\n",

		_PadMapping.ID, _PadState.joy,

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
		*/
#endif
}






// Configure button mapping
// ----------

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
void GetButton(SDL_Joystick *joy, int ControllerID,
				int &KeyboardKey, int &value, int &type, int &pressed, bool &Succeed, bool &Stop,
				bool LeftRight, bool Axis, bool XInput, bool Button, bool Hat, bool NoTriggerFilter)
{
	// It needs the wxWidgets excape keycode
	static const int WXK_ESCAPE = 27;

	// Save info
	int buttons = SDL_JoystickNumButtons(joy);
	int axes = SDL_JoystickNumAxes(joy);;
	int hats = SDL_JoystickNumHats(joy);;

	// Update the internal status
	SDL_JoystickUpdate();

	// For the triggers we accept both a digital or an analog button
	if (Axis)
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
	if (Hat)
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



// **********************************************



// Search for DirectInput devices
// ----------------
#ifdef _WIN32
BOOL CALLBACK EnumDICallback(const DIDEVICEINSTANCE* pInst, VOID* pContext)
{
	NumDIDevices++;
	return true;
}
#endif
int SearchDIDevices()
{
	#ifdef _WIN32
	LPDIRECTINPUT8			g_pObject;
	LPDIRECTINPUTDEVICE8	g_pDevice;
	DIPROPDWORD dipdw;
	HRESULT hr;
	NumDIDevices = 0;

	// Register with the DirectInput subsystem and get a pointer to a IDirectInput interface we can use.
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pObject, NULL)))
		return 0;

	// Look for a device
	if (FAILED(hr = g_pObject->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDICallback, NULL, DIEDFL_ATTACHEDONLY)))
		return 0;
	return NumDIDevices;
	#else
	return 0;
	#endif
}


} // InputCommon

