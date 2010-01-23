
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




// File description
/* -------------------
   Function: This file will get the status of the analog triggers of any connected XInput device.
   This code was made with the help of SimpleController.cpp in the June 2008 Microsoft DirectX SDK
   Samples.

///////////////////////////////////////////////////// */

#ifdef _WIN32


// Includes
// -------------------
#include <windows.h>
#include <XInput.h> // XInput API

#include "SDL.h" // Local



namespace XInput
{



// Declarations
// -------------------

#define MAX_CONTROLLERS 4  // XInput handles up to 4 controllers 

struct CONTROLER_STATE
{
    XINPUT_STATE state;
    bool bConnected;
};
CONTROLER_STATE g_Controllers[MAX_CONTROLLERS];




// Init
// -------------------
/* Function: Calculate the number of connected XInput devices
   Todo: Implement this to figure out if there are multiple XInput controllers connected,
   we currently only try to connect to XInput device 0 */
void Init()
{
    // Init state
    //ZeroMemory( g_Controllers, sizeof( CONTROLER_STATE ) * MAX_CONTROLLERS );

	// Declaration
	DWORD dwResult;

	// Calculate the number of connected XInput devices
	for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        // Simply get the state of the controller from XInput.
        dwResult = XInputGetState( i, &g_Controllers[i].state );

        if( dwResult == ERROR_SUCCESS )
            g_Controllers[i].bConnected = true;
        else
            g_Controllers[i].bConnected = false;
    }

}	




// Get the trigger status
// -------------------
int GetXI(int Controller, int Button)
{
	// Update the internal status
    DWORD dwResult;
	dwResult = XInputGetState(Controller, &g_Controllers[Controller].state);

	if (dwResult != ERROR_SUCCESS) return -1;

	switch (Button)
	{
	case InputCommon::XI_TRIGGER_L:
		return g_Controllers[Controller].state.Gamepad.bLeftTrigger;

	case InputCommon::XI_TRIGGER_R:
		return g_Controllers[Controller].state.Gamepad.bRightTrigger;

	default:
		return 0;
	}
}




// Check if a certain controller is connected
// -------------------
bool IsConnected(int Controller)
{
	DWORD dwResult = XInputGetState( Controller, &g_Controllers[Controller].state );

	// Update the connected status
	if( dwResult == ERROR_SUCCESS )
		return true;
	else
		return false;
}


} // XInput

#endif