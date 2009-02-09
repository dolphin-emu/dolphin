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

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

// ===================================================
// Fill joyinfo with the current connected devices
// ----------------
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	bool Success = InputCommon::SearchDevices(_joyinfo, _NumPads, _NumGoodPads);

	// Warn the user if no gamepads are detected
	if (_NumGoodPads == 0 && g_EmulatorRunning)
	{
		//PanicAlert("nJoy: No Gamepad Detected");
		//return false;
	}

	// Load PadMapping[] etc
	g_Config.Load();

	// Update the PadState[].joy handle
	for (int i = 0; i < 1; i++)
	{
		if (PadMapping[i].enabled && joyinfo.size() > PadMapping[i].ID)
			if(joyinfo.at(PadMapping[i].ID).Good)
				PadState[i].joy = SDL_JoystickOpen(PadMapping[i].ID);
	}

	return Success;
}
// ===========================


// Request joystick state.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Called from: PAD_GetStatus()
   Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input value "controller" is
   for a virtual controller 0 to 3. */

void GetJoyState(InputCommon::CONTROLLER_STATE_NEW &_PadState, InputCommon::CONTROLLER_MAPPING_NEW _PadMapping, int controller, int NumButtons)
{
	// Update the gamepad status
	SDL_JoystickUpdate();

	// Update axis states. It doesn't hurt much if we happen to ask for nonexisting axises here.
	_PadState.Axis.Lx = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Lx);
	_PadState.Axis.Ly = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Ly);
	_PadState.Axis.Rx = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Rx);
	_PadState.Axis.Ry = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Ry);

	// Update the analog trigger axis values
#ifdef _WIN32
	if (_PadMapping.triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
#endif
		// If we are using SDL analog triggers the buttons have to be mapped as 1000 or up, otherwise they are not used
		_PadState.Axis.Tl = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Tl - 1000);
		_PadState.Axis.Tr = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Tr - 1000);
#ifdef _WIN32
	}
	else
	{
		_PadState.Axis.Tl = XInput::GetXI(0, _PadMapping.Axis.Tl - 1000);
		_PadState.Axis.Tr = XInput::GetXI(0, _PadMapping.Axis.Tr - 1000);
	}
#endif

	/* Debugging 
	Console::ClearScreen();
	Console::Print(
		"Controller and handle: %i %i\n"

		"Triggers:%i  %i %i  %i %i\n",

		controller, (int)_PadState.joy,

		_PadMapping.triggertype,
		_PadMapping.Axis.Tl, _PadMapping.Axis.Tr,
		_PadState.Axis.Tl, _PadState.Axis.Tr
		);*/
}


} // end of namespace WiiMoteEmu