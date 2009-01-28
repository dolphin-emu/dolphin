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


#ifndef WIIMOTE_REAL_H
#define WIIMOTE_REAL_H


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "wiiuse.h"
///////////////////////////////////


namespace WiiMoteReal
{

#define MAX_WIIMOTES 1

int Initialize();
void DoState(void* ptr, int mode);
void Shutdown(void);
void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(u16 _channelID, const void* _pData, u32 _Size);
void Update();
void ReadWiimote();

#ifndef EXCLUDE_H
	extern wiimote_t**		g_WiiMotesFromWiiUse;
	extern int				g_NumberOfWiiMotes;
	extern bool				g_MotionSensing;
#endif

}; // WiiMoteReal

#endif