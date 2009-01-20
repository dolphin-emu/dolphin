//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
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
// Settings
// ¯¯¯¯¯¯¯¯¯¯
// Set this if you want to use the rumble 'hack' for controller one
//#define USE_RUMBLE_DINPUT_HACK
//////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯
#include <vector> // System
#include <cstdio>
#include <ctime>
#include <cmath>
#include <SDL.h>

#include "Common.h" // Common
#include "pluginspecs_pad.h"
#include "IniFile.h"
//#include "ConsoleWindow.h"

#include "Config.h" // Local

#if defined(HAVE_WX) && HAVE_WX
	#include "GUI/AboutBox.h"
	#include "GUI/ConfigBox.h"
	//extern ConfigBox* m_frame;
#endif

#ifdef _WIN32
	#include <tchar.h>
	#define _CRT_SECURE_NO_WARNINGS
	#define DIRECTINPUT_VERSION 0x0800
	#define WIN32_LEAN_AND_MEAN

	#ifdef USE_RUMBLE_DINPUT_HACK
		#pragma comment(lib, "dxguid.lib")
		#pragma comment(lib, "dinput8.lib")
		#pragma comment(lib, "winmm.lib")
		#include <dinput.h>
		VOID FreeDirectInput(); // Needed in both nJoy.cpp and Rumble.cpp
	#endif
#endif // _WIN32

#ifdef _WIN32
	#define SLEEP(x) Sleep(x)
#else
	#include <unistd.h>
	#include <sys/ioctl.h>
	#define SLEEP(x) usleep(x*1000)
#endif

#ifdef __linux__
#include <linux/input.h>
#endif
//////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Define
// ¯¯¯¯¯¯¯¯¯¯

#define INPUT_VERSION	"0.3"
#define INPUT_STATE		"PUBLIC RELEASE"
#define RELDAY			"21"
#define RELMONTH		"07"
#define RELYEAR			"2008"
#define THANKYOU		"`plot`, Absolute0, Aprentice, Bositman, Brice, ChaosCode, CKemu, CoDeX, Dave2001, dn, drk||Raziel, Florin, Gent, Gigaherz, Hacktarux, JegHegy, Linker, Linuzappz, Martin64, Muad, Knuckles, Raziel, Refraction, Rudy_x, Shadowprince, Snake785, Saqib, vEX, yaz0r, Zilmar, Zenogais and ZeZu."


//////////////////////////////////////////////////////////////////////////////////////////
// Structures
/* ¯¯¯¯¯¯¯¯¯¯
	CONTROLLER_STATE buttons (joystate) = 0 or 1
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
	int enabled;			// Pad attached?
	int deadzone;			// Deadzone... what else?
	int ID;					// SDL joystick device ID
	int controllertype;		// Hat: Hat or custom buttons
	int triggertype;		// Triggers range
	int eventnum;			// Linux Event Number, Can't be found dynamically yet
};

struct CONTROLLER_INFO		// CONNECTED WINDOWS DEVICES INFO
{
	int NumAxes;			// Amount of Axes
	int NumButtons;			// Amount of Buttons
	int NumBalls;			// Amount of Balls
	int NumHats;			// Amount of Hats (POV)
	const char *Name;		// Joypad/stickname
	int ID;					// SDL joystick device ID
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
	CTL_TRIGGER_HALF = 0, // XBox 360
	CTL_TRIGGER_WHOLE // Other pads
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


//////////////////////////////////////////////////////////////////////////////////////////
// Input vector. Todo: Save the configured keys here instead of in joystick
// ¯¯¯¯¯¯¯¯¯
/*
#ifndef _CONTROLLER_STATE_H
extern std::vector<u8> Keys;
#endif
*/



//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ¯¯¯¯¯¯¯¯¯
#ifndef _CONTROLLER_STATE_H
	extern FILE *pFile;
	extern CONTROLLER_INFO *joyinfo;
	extern CONTROLLER_STATE joystate[4];
	extern CONTROLLER_MAPPING joysticks[4];
	extern HWND m_hWnd; // Handle to window
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// Custom Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void GetJoyState(int controller);
int Search_Devices();
void DEBUG_INIT();
void DEBUG_QUIT();

void Pad_Use_Rumble(u8 _numPAD, SPADStatus* _pPADStatus); // Rumble
u8 Pad_Convert(int _val, int _type = 1); // Value conversion
std::vector<int> Pad_Square_to_Circle(int _x, int _y, int _pad); // Value conversion

//void SaveConfig();
//void LoadConfig();
