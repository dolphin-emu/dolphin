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

#ifdef _WIN32
#include "XInput.h"
#endif

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

extern void PAD_RumbleClose();

// Close joypads
void Close_Devices()
{
	PAD_RumbleClose();
	// Close all devices carefully. We must check that we are not accessing any
	// undefined vector elements or any bad devices
	if (SDL_WasInit(0))
	{
		for (int i = 0; i < NumPads; i++)
		{
			if (joyinfo.at(i).joy)
			{
				if(SDL_JoystickOpened(i))
				{
					INFO_LOG(WIIMOTE, "Shut down Joypad: %i", i);
					SDL_JoystickClose(joyinfo.at(i).joy);
				}
			}
		}
	}

	for (int i = 0; i < MAX_WIIMOTES; i++)
		WiiMapping[i].joy = NULL;

	// Clear the physical device info
	joyinfo.clear();
	NumPads = 0;
	NumGoodPads = 0;
}

// Fill joyinfo with the current connected devices
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	// Close opened pad first
	Close_Devices();

	bool WasGotten = InputCommon::SearchDevices(_joyinfo, _NumPads, _NumGoodPads);

	if (_NumGoodPads == 0)
		return false;

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (_NumPads > WiiMapping[i].ID)
			if(_joyinfo.at(WiiMapping[i].ID).Good)
			{
				WiiMapping[i].joy = _joyinfo.at(WiiMapping[i].ID).joy;
#ifdef _WIN32
				XINPUT_STATE xstate;
				DWORD xresult = XInputGetState(WiiMapping[i].ID, &xstate);
				if (xresult == ERROR_SUCCESS)
					WiiMapping[i].TriggerType = InputCommon::CTL_TRIGGER_XINPUT;
#endif
			}
	}
	return WasGotten;
}


// Request joystick state
/* Called from: PAD_GetStatus() Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input
   value "controller" is for a virtual controller 0 to 3. */
void GetAxisState(CONTROLLER_MAPPING_WII &_WiiMapping)
{
	// Update the gamepad status
	SDL_JoystickUpdate();

	// Update axis states. It doesn't hurt much if we happen to ask for nonexisting axises here.
	_WiiMapping.AxisState.Lx = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Lx);
	_WiiMapping.AxisState.Ly = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Ly);
	_WiiMapping.AxisState.Rx = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Rx);
	_WiiMapping.AxisState.Ry = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Ry);

	// Update the analog trigger axis values
#ifdef _WIN32
	if (_WiiMapping.TriggerType == InputCommon::CTL_TRIGGER_SDL)
	{
#endif
		// If we are using SDL analog triggers the buttons have to be mapped as 1000 or up, otherwise they are not used
		// We must also check that we are not asking for a negative axis number because SDL_JoystickGetAxis() has
		// no good way of handling that
		if ((_WiiMapping.AxisMapping.Tl - 1000) >= 0)
			_WiiMapping.AxisState.Tl = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Tl - 1000);
		if ((_WiiMapping.AxisMapping.Tr - 1000) >= 0)
			_WiiMapping.AxisState.Tr = SDL_JoystickGetAxis(_WiiMapping.joy, _WiiMapping.AxisMapping.Tr - 1000);
#ifdef _WIN32
	}
	else
	{
		_WiiMapping.AxisState.Tl = XInput::GetXI(_WiiMapping.ID, _WiiMapping.AxisMapping.Tl - 1000);
		_WiiMapping.AxisState.Tr = XInput::GetXI(_WiiMapping.ID, _WiiMapping.AxisMapping.Tr - 1000);
	}
#endif

	/* Debugging 
//	Console::ClearScreen();
	DEBUG_LOG(CONSOLE,
		"Controller and handle: %i %i\n"

		"Triggers:%i  %i %i  %i %i\n"

		"Analog:%06i %06i  \n",

		controller, (int)_PadState.joy,

		_PadMapping.triggertype,
		_PadMapping.Axis.Tl, _PadMapping.Axis.Tr,
		_PadState.Axis.Tl, _PadState.Axis.Tr,

		_PadState.Axis.Lx, _PadState.Axis.Ly
		);*/
}

void UpdatePadState(CONTROLLER_MAPPING_WII &_WiiMapping)
{
	// Return if we have no pads
	if (NumGoodPads == 0) return;

	GetAxisState(_WiiMapping);

	int &Lx = _WiiMapping.AxisState.Lx;
	int &Ly = _WiiMapping.AxisState.Ly;
	int &Rx = _WiiMapping.AxisState.Rx;
	int &Ry = _WiiMapping.AxisState.Ry;
	int &Tl = _WiiMapping.AxisState.Tl;
	int &Tr = _WiiMapping.AxisState.Tr;

	// Check the circle to square option
	if(_WiiMapping.bCircle2Square)
	{
		InputCommon::Square2Circle(Lx, Ly, _WiiMapping.Diagonal, true);
		InputCommon::Square2Circle(Rx, Ry, _WiiMapping.Diagonal, true);
	}

	// Dead zone adjustment
	float DeadZoneLeft = (float)_WiiMapping.DeadZoneL / 100.0f;
	float DeadZoneRight = (float)_WiiMapping.DeadZoneR / 100.0f;
	if (InputCommon::IsDeadZone(DeadZoneLeft, Lx, Ly))
	{
		Lx = 0;
		Ly = 0;
	}
	if (InputCommon::IsDeadZone(DeadZoneRight, Rx, Ry))
	{
		Rx = 0;
		Ry = 0;
	}

	// Downsize the values from 0x8000 to 0x80
	Lx = InputCommon::Pad_Convert(Lx);
	Ly = InputCommon::Pad_Convert(Ly);
	Rx = InputCommon::Pad_Convert(Rx);
	Ry = InputCommon::Pad_Convert(Ry);
	// The XInput range is already 0 to 0x80
	if (_WiiMapping.TriggerType == InputCommon::CTL_TRIGGER_SDL)
	{
		Tl = InputCommon::Pad_Convert(Tl);
		Tr = InputCommon::Pad_Convert(Tr);
	}
}


} // end of namespace WiiMoteEmu
