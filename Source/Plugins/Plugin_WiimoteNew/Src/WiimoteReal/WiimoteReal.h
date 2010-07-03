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


#ifndef WIIMOTE_REAL_H
#define WIIMOTE_REAL_H

#include "wiiuse.h"
#include "ChunkFile.h"

#include "../../InputCommon/Src/InputConfig.h"

#define MAX_WIIMOTES	4

extern unsigned int g_wiimote_sources[MAX_WIIMOTES];
extern InputPlugin g_plugin;
extern SWiimoteInitialize g_WiimoteInitialize;

enum
{
	WIIMOTE_SRC_NONE = 0,
	WIIMOTE_SRC_EMU,
	WIIMOTE_SRC_REAL,
};

namespace WiimoteReal
{

unsigned int Initialize();
void Shutdown();
void Refresh();
void LoadSettings();

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _WiimoteNumber);

void DoState(PointerWrap &p);
void StateChange(PLUGIN_EMUSTATE newState);

}; // WiiMoteReal

#endif
