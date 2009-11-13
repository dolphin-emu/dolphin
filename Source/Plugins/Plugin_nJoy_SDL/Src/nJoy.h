
// Project description
// -------------------
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
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



#ifndef __NJOY_h__
#define __NJOY_h__


// Includes
// ----------
#include <vector> // System
#include <cstdio>
#include <ctime>
#include <cmath>

#include "../../../Core/InputCommon/Src/SDL.h" // Core
#include "../../../Core/InputCommon/Src/XInput.h"

#include "Common.h" // Common
#include "Setup.h"
#include "pluginspecs_pad.h"
#include "IniFile.h"

#include "Config.h" // Local

#if defined(HAVE_WX) && HAVE_WX
	#include "GUI/AboutBox.h"
	#include "GUI/ConfigBox.h"
#endif

// Define
// ----------

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

#define INPUT_STATE		wxT("PUBLIC RELEASE")
#define RELDAY			wxT("21")
#define RELMONTH		wxT("07")
#define RELYEAR			wxT("2008")
#define THANKYOU		wxT("`plot`, Absolute0, Aprentice, Bositman, Brice, ChaosCode, CKemu, CoDeX, Dave2001, dn, drk||Raziel, Florin, Gent, Gigaherz, Hacktarux, JegHegy, Linker, Linuzappz, Martin64, Muad, Knuckles, Raziel, Refraction, Rudy_x, Shadowprince, Snake785, Saqib, vEX, yaz0r, Zilmar, Zenogais and ZeZu.")
#define PLUGIN_VER_STR	wxT("Based on nJoy 0.3 by Falcon4ever\nRelease: ") RELDAY wxT("/") RELMONTH wxT("/") RELYEAR wxT("\nwww.multigesture.net")


// Variables
// ---------
#ifndef _EXCLUDE_MAIN_
	extern SPADInitialize *g_PADInitialize;
	extern FILE *pFile;
	extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
	extern InputCommon::CONTROLLER_STATE PadState[4];
	extern InputCommon::CONTROLLER_MAPPING PadMapping[4];
	#ifdef _WIN32
		extern HWND m_hWnd, m_hConsole; // Handle to window
	#endif
	extern int NumPads, NumDIDevices, LastPad; // Number of pads
	extern bool SDLPolling;
	extern bool LiveUpdates;
#endif


// Custom Functions
// ----------------
bool LocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads);
bool LocalSearchDevicesReset(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads);
bool DoLocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads);
int SearchDIDevices();
bool IsConnected(std::string Name);
bool IsPolling();
void EnablePolling(bool Enable);
std::string IDToName(int ID);
bool IsConnected(std::string Name);
int IDToid(int ID);
void DEBUG_INIT();
void DEBUG_QUIT();
bool IsFocus();
bool ReloadDLL();
void PAD_RumbleClose();
#endif // __NJOY_h__
