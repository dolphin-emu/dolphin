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

#include "../../../Core/InputCommon/Src/SDL.h" // Core
#include "../../../Core/InputCommon/Src/XInput.h"

#include "Common.h" // Common
#include "LogManager.h"
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

#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigPadDlg.h"
#endif

extern SWiimoteInitialize g_WiimoteInitialize;
extern WiimotePadConfigDialog *m_PadConfigFrame;

namespace WiiMoteEmu
{


bool LocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	//DEBUG_LOG(PAD, "LocalSearchDevices");
	bool bSuccess = InputCommon::SearchDevices(_joyinfo, _NumPads);
	
	DoLocalSearchDevices(_joyinfo, _NumPads);
	
	return bSuccess;
}

bool LocalSearchDevicesReset(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	DEBUG_LOG(CONSOLE, "LocalSearchDevicesReset");
	
	// Turn off device polling while resetting
	EnablePolling(false);
	
	bool bSuccess = InputCommon::SearchDevicesReset(_joyinfo, _NumPads);
	
	EnablePolling(true);
	
	DoLocalSearchDevices(_joyinfo, _NumPads);
	
	return bSuccess;
}

// Fill joyinfo with the current connected devices
bool DoLocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	//DEBUG_LOG(WIIMOTE, "LocalSearchDevices");
	
	// Turn off device polling while searching
	WiiMoteEmu::EnablePolling(false);		
	
	bool bReturn = InputCommon::SearchDevices(_joyinfo, _NumPads);

	// Warn the user if no gamepads are detected
	if (_NumPads == 0 && g_EmulatorRunning)
	{
		//PanicAlert("Wiimote: No Gamepad Detected");
		//return false;
	}

	// Load the first time
	if (!g_Config.Loaded) g_Config.Load();

	// Update the PadState[].joy handle
	// If the saved ID matches, select this device for this slot
	bool Match = false;
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{	
		for (int j = 0; j < joyinfo.size(); j++)
		{
			if (joyinfo.at(j).Name == PadMapping[i].Name)
			{
				PadState[i].joy = joyinfo.at(j).joy;
				Match = true;
				//INFO_LOG(WIIMOTE, "Slot %i: '%s' %06i", i, joyinfo.at(j).Name.c_str(), PadState[i].joy);
			}
		}
		if (!Match) PadState[i].joy = NULL;
	}

	WiiMoteEmu::EnablePolling(true);

	return bReturn;
}

// Is the device connected?
// ----------------
bool IsConnected(std::string Name)
{
	for (int i = 0; i < joyinfo.size(); i++)
	{
		DEBUG_LOG(WIIMOTE, "Pad %i: IsConnected checking '%s' against '%s'", i, joyinfo.at(i).Name.c_str(), Name.c_str());
		if (joyinfo.at(i).Name == Name)
			return true;
	}
}
// See description of the equivalent functions in nJoy.cpp...
// ----------------
bool IsPolling()
{
	return true;
	/*
	if (!SDLPolling || SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE)
		return false;
	else
		return true;
	*/
}
void EnablePolling(bool Enable)
{
	/*
	if (Enable)
	{
		SDLPolling = true;
		SDL_JoystickEventState(SDL_IGNORE);
	}
	else
	{
		SDLPolling = false;
		SDL_JoystickEventState(SDL_ENABLE);
	}
	*/
}

// ID to Name
// ----------------
std::string IDToName(int ID)
{
	for (int i = 0; i < joyinfo.size(); i++)
	{
		//DEBUG_LOG(WIIMOTE, "IDToName: ID %i id %i %s", ID, i, joyinfo.at(i).Name.c_str());
		if (joyinfo.at(i).ID == ID)
			return joyinfo.at(i).Name;
	}
	return "";
}


// Return adjusted input values
void PadStateAdjustments(int &Lx, int &Ly, int &Rx, int &Ry, int &Tl, int &Tr)
{
	// This has to be changed if multiple Wiimotes are to be supported later
	const int Page = 0;

	// Copy all states to a local variable
	Lx = PadState[Page].Axis.Lx;
	Ly = PadState[Page].Axis.Ly;
	Rx = PadState[Page].Axis.Rx;
	Ry = PadState[Page].Axis.Ry;
	Tl = PadState[Page].Axis.Tl;
	Tr = PadState[Page].Axis.Tr;

	// Check the circle to square option
	if(PadMapping[Page].bCircle2Square)
	{
		InputCommon::Square2Circle(Lx, Ly, Page, PadMapping[Page].SDiagonal, true);
	}

	// Dead zone adjustment
	float DeadZoneLeft = (float)PadMapping[Page].DeadZoneL / 100.0f;
	float DeadZoneRight = (float)PadMapping[Page].DeadZoneR / 100.0f;
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
	if (PadMapping[Page].triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
		Tl = InputCommon::Pad_Convert(PadState[Page].Axis.Tl);
		Tr = InputCommon::Pad_Convert(PadState[Page].Axis.Tr);
	}
}


// Request joystick state
/* Called from: PAD_GetStatus() Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input
   value "controller" is for a virtual controller 0 to 3. */

void GetJoyState(InputCommon::CONTROLLER_STATE_NEW &_PadState, InputCommon::CONTROLLER_MAPPING_NEW _PadMapping, int controller)
{
	//DEBUG_LOG(WIIMOTE, "GetJoyState: Polling:%i NumPads:%i", SDLPolling, NumPads);

	// Return if polling is off
	if (!IsPolling) return;
	// Update joyinfo handles. This is in case the Wiimote plugin has restarted SDL after a pad was conencted/disconnected
	// so that the handles are updated. We don't need to run this this often. Once a second would be enough.
	LocalSearchDevices(joyinfo, NumPads);
	// Return if we have no pads
	if (NumPads == 0) return;
	// Read info
	int NumButtons = SDL_JoystickNumButtons(_PadState.joy);
	
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
		// We must also check that we are not asking for a negative axis number because SDL_JoystickGetAxis() has
		// no good way of handling that
		if ((_PadMapping.Axis.Tl - 1000) >= 0) _PadState.Axis.Tl = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Tl - 1000);
		if ((_PadMapping.Axis.Tr - 1000) >= 0) _PadState.Axis.Tr = SDL_JoystickGetAxis(_PadState.joy, _PadMapping.Axis.Tr - 1000);
#ifdef _WIN32
	}
	else
	{
		_PadState.Axis.Tl = XInput::GetXI(0, _PadMapping.Axis.Tl - 1000);
		_PadState.Axis.Tr = XInput::GetXI(0, _PadMapping.Axis.Tr - 1000);
	}
#endif

	/* Debugging
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->ClearScreen();
	Console->CustomLog(StringFromFormat(
		"Controller: %i Handle: %i\n"
		"Triggers: %i  %i %i  %i %i\n"
		"Analog: %06i %06i  \n",

		controller, (int)_PadState.joy,

		_PadMapping.triggertype,
		_PadMapping.Axis.Tl, _PadMapping.Axis.Tr,
		_PadState.Axis.Tl, _PadState.Axis.Tr,

		_PadState.Axis.Lx, _PadState.Axis.Ly
		).c_str());
	*/
}


} // end of namespace WiiMoteEmu
