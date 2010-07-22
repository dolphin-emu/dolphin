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

#ifndef EXCLUDE_H
//this one is required for Common::Event
#include "Thread.h"
#endif

namespace WiiMoteReal
{

int Initialize();
#ifdef _WIN32
int WiimotePairUp(bool unpair);
#endif

void Allocate();
void DoState(PointerWrap &p);
void Shutdown(void);
void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _WiimoteNumber);

void SendAcc(u8 _ReportID);
void ClearEvents();


#ifndef EXCLUDE_H
	extern wiimote_t**		g_WiiMotesFromWiiUse;
	extern bool				g_Shutdown;
	extern int				g_NumberOfWiiMotes;
	extern int				LastNumberOfWiimotes;
#ifdef _WIN32
	extern Common::Thread*	g_AutoPairUpInvisibleWindow;
	extern Common::Thread*	g_AutoPairUpMonitoring;
	extern Common::Event	g_StartAutopairThread;
	THREAD_RETURN			PairUp_ThreadFunc(void* arg);
	THREAD_RETURN			RunInvisibleMessageWindow_ThreadFunc(void* arg);
#endif
#endif

}; // WiiMoteReal

#endif

