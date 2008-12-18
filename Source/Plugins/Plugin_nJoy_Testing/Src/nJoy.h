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

// Set this if you want to use the rumble 'hack' for controller one
//#define USE_RUMBLE_DINPUT_HACK

#include <vector>
#include <stdio.h>
#include <time.h>
#include <SDL.h>

#include "Common.h"
#include "pluginspecs_pad.h"
#include "IniFile.h"

#if defined(HAVE_WX) && HAVE_WX
#include "GUI/AboutBox.h"
#include "GUI/ConfigBox.h"
#endif

#ifdef _WIN32
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "comctl32.lib")
#include <tchar.h>
#include <math.h>
#define _CRT_SECURE_NO_WARNINGS
#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN

#ifdef USE_RUMBLE_DINPUT_HACK
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "winmm.lib")
#include <dinput.h>
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

//////////////////////////////////////////////////////////////////////////////////////////
// Define
// ¯¯¯¯¯¯

#define INPUT_VERSION	"0.4"
#define INPUT_STATE		"TESTING"
#define RELDAY			"01"
#define RELMONTH		"12"
#define RELYEAR			"2008"
#define THANKYOU		"`plot`, Absolute0, Aprentice, Bositman, Brice, ChaosCode, CKemu, CoDeX, Dave2001, dn, drk||Raziel, Florin, Gent, Gigaherz, Hacktarux, JegHegy, Linker, Linuzappz, Martin64, Muad, Knuckles, Raziel, Refraction, Rudy_x, Shadowprince, Snake785, Saqib, vEX, yaz0r, Zilmar, Zenogais and ZeZu."

//////////////////////////////////////////////////////////////////////////////////////////
// Structures
// ¯¯¯¯¯¯¯¯¯¯
#define MAX_AXISES 12 // If more than 12 is needed. Up it
class sCalibration
{
	public:
	sCalibration()
	{
		Min = 0;
		Max = 0;
	}
	Sint16 Min;
	Sint16 Max;
}; // Simple Calibration Data
struct CONTROLLER_STATE{	// GC PAD INFO/STATE
	int buttons[16];		// Amount of buttons (A B X Y Z, L-Trigger R-Trigger Start) might need to change the triggers buttons
	int dpad;				// 1 HAT (8 directions + neutral)
	int halfpress;			// ...
	SDL_Joystick *joy;		// SDL joystick device
};

struct CONTROLLER_MAPPING{	// GC PAD MAPPING
	std::string buttons[16];// Amount of buttons (A B X Y Z, L-Trigger R-Trigger Start) might need to change the triggers buttons
	sCalibration sData[MAX_AXISES];	// Calibration Data
	int dpad;				// 1 HAT (8 directions + neutral)
	int enabled;			// Pad attached?
	int deadzone;			// Deadzone... what else?
	int halfpress;			// Halfpress... you know, like not fully pressed ;)...
	int ID;					// SDL joystick device ID
	int controllertype;		// Joystick, Joystick no hat or a keyboard (perhaps a mouse later)
	int eventnum;			// Linux Event Number, Can't be found dynamically yet
};

struct CONTROLLER_INFO{		// CONNECTED WINDOWS DEVICES INFO
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
	CTL_L_SHOULDER = 0,
	CTL_R_SHOULDER,
	CTL_A_BUTTON,
	CTL_B_BUTTON,
	CTL_X_BUTTON,
	CTL_Y_BUTTON,
	CTL_Z_TRIGGER,	
	CTL_START,
	CTL_D_PAD_UP,
	CTL_D_PAD_DOWN,
	CTL_D_PAD_LEFT,
	CTL_D_PAD_RIGHT,
	CTL_MAIN_X,
	CTL_MAIN_Y,
	CTL_SUB_X,
	CTL_SUB_Y,	
	CTRL_END,
};

enum
{
	CTL_TYPE_JOYSTICK = 0,
	CTL_TYPE_JOYSTICK_NO_HAT,
	CTL_TYPE_JOYSTICK_XBOX360,
	CTL_TYPE_KEYBOARD
};
//////////////////////////////////////////////////////////////////////////////////////////
// Custom Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void GetJoyState(int controller);
int Search_Devices();
void DEBUG_INIT();
void DEBUG_QUIT();

void SaveConfig();
void LoadConfig();
