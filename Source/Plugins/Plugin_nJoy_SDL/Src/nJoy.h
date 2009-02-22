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


#ifndef __NJOY_h__
#define __NJOY_h__


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

#include "../../../Core/InputCommon/Src/SDL.h" // Core
#include "../../../Core/InputCommon/Src/XInput.h"

#include "Common.h" // Common
#include "Setup.h"
#include "pluginspecs_pad.h"
#include "IniFile.h"
#include "ConsoleWindow.h"
//#include "Timer.h"

#include "Config.h" // Local

#if defined(HAVE_WX) && HAVE_WX
	#include "GUI/AboutBox.h"
	#include "GUI/ConfigBox.h"
	//extern ConfigBox* m_frame;
#endif

#ifdef _WIN32
	#include <tchar.h>
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
#ifndef _EXCLUDE_MAIN_
	extern SPADInitialize *g_PADInitialize;
	extern FILE *pFile;
	extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
	extern InputCommon::CONTROLLER_STATE PadState[4];
	extern InputCommon::CONTROLLER_MAPPING PadMapping[4];
	#ifdef _WIN32
		extern HWND m_hWnd, m_hConsole; // Handle to window
	#endif
	extern int NumPads, NumGoodPads, LastPad; // Number of goods pads
#endif
////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Custom Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads);
void DEBUG_INIT();
void DEBUG_QUIT();
bool IsFocus();
bool ReloadDLL();

void Pad_Use_Rumble(u8 _numPAD, SPADStatus* _pPADStatus); // Rumble

//void SaveConfig();
//void LoadConfig();
////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// ReRecording
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef RERECORDING
namespace Recording
{
	void Initialize();
	void DoState(unsigned char **ptr, int mode);
	void ShutDown();
	void Save();
	void Load();
	const SPADStatus& Play();
	void RecordInput(const SPADStatus& _rPADStatus);
}
#endif
////////////////////////////////


#endif // __NJOY_h__
