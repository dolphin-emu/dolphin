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


#ifndef _PLUGIN_GCPAD_H
#define _PLUGIN_GCPAD_H


#include <vector> // System
#include <cstdio>
#include "../../../Core/InputCommon/Src/SDL.h" // Core
#ifdef _WIN32
   #include "../../../Core/InputCommon/Src/XInput.h"
#elif defined(HAVE_X11) && HAVE_X11
   #include <X11/Xlib.h>
   #include <X11/Xutil.h>
   #include <X11/keysym.h>
   #include <X11/XKBlib.h>
//no need for Cocoa yet, but I guess ayuanx isn't done yet.
//#elif defined(HAVE_COCOA) && HAVE_COCOA
//   #include <Cocoa/Cocoa.h>
#endif
#include "pluginspecs_pad.h"


// SDL Haptic fails on windows, it just doesn't work (even the sample doesn't work)
// So until i can make it work, this is all disabled >:(
#if SDL_VERSION_ATLEAST(1, 3, 0) && !defined(_WIN32)
	#define SDL_RUMBLE
#else
	#ifdef _WIN32
		#define RUMBLE_HACK
		#define DIRECTINPUT_VERSION 0x0800
		#define WIN32_LEAN_AND_MEAN

		#pragma comment(lib, "dxguid.lib")
		#pragma comment(lib, "dinput8.lib")
		#pragma comment(lib, "winmm.lib")
		#include <dinput.h>
	#endif
#endif

#define DEF_BUTTON_FULL			255
#define DEF_STICK_FULL			100
#define DEF_TRIGGER_FULL		255
#define DEF_TRIGGER_THRESHOLD	230

// GC Pad Buttons
enum EGCPad
{
	EGC_A = 0,
	EGC_B,
	EGC_X,
	EGC_Y,
	EGC_Z,
	EGC_START,

	EGC_DPAD_UP,
	EGC_DPAD_DOWN,
	EGC_DPAD_LEFT,
	EGC_DPAD_RIGHT,

	EGC_STICK_UP,
	EGC_STICK_DOWN,
	EGC_STICK_LEFT,
	EGC_STICK_RIGHT,

	EGC_CSTICK_UP,
	EGC_CSTICK_DOWN,
	EGC_CSTICK_LEFT,
	EGC_CSTICK_RIGHT,

	EGC_TGR_L,
	EGC_TGR_R,
	EGC_TGR_SEMI_L,
	EGC_TGR_SEMI_R,

	LAST_CONSTANT,
};

enum EInputType
{
	FROM_KEYBOARD,
	FROM_ANALOG1,
	FROM_ANALOG2,
	FROM_TRIGGER,
};

union UAxis
{
	int Code[6];
	struct
	{
		int Lx;
		int Ly;
		int Rx;
		int Ry;
		int Tl; // Trigger
		int Tr; // Trigger
	};
};

struct SStickMapping
{
	int Main;
	int Sub;
	int Shoulder; //Trigger
};

struct CONTROLLER_MAPPING_GC	// PAD MAPPING GC
{
	int ID;					// SDL joystick device ID
	SDL_Joystick *joy;		// SDL joystick device
	UAxis AxisState;
	UAxis AxisMapping;		// 6 Axes (Main, Sub, Triggers)
	int TriggerType;		// SDL or XInput trigger
	bool Rumble;
	int RumbleStrength;
	int DeadZoneL;			// Analog 1 Deadzone
	int DeadZoneR;			// Analog 2 Deadzone
	bool bSquare2Circle;
	int Diagonal;

	SStickMapping Stick;
	int Button[LAST_CONSTANT];
};

extern CONTROLLER_MAPPING_GC GCMapping[4];
extern int NumPads, NumGoodPads, g_ID;

extern SPADInitialize *g_PADInitialize;
extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
#ifdef _WIN32
extern HWND m_hWnd; // Handle to window
#endif
#if defined(HAVE_X11) && HAVE_X11
extern Display* WMdisplay;
#endif


// Custom Functions
// ----------------
void EmulateAnalogStick(unsigned char &stickX, unsigned char &stickY, bool buttonUp, bool buttonDown, bool buttonLeft, bool buttonRight, int magnitude);
void EmulateAnalogTrigger(unsigned char &trL, unsigned char &trR);
void Close_Devices();
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads);
void GetAxisState(CONTROLLER_MAPPING_GC &_GCMapping);
void UpdatePadState(CONTROLLER_MAPPING_GC &_GCMapping);
bool IsKey(int Key);
void ReadLinuxKeyboard();
bool IsFocus();
bool ReloadDLL();
void PAD_RumbleClose();

#endif // _PLUGIN_GCPAD_H
