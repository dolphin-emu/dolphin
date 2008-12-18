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

#include <iostream> // I hope this doesn't break anything
#include <stdio.h>
#include <stdarg.h>

#include "Common.h" // for Common::swap
#include "Globals.h"
#include "gdsp_interpreter.h"


// =======================================================================================
// This is to verbose, it has to be turned on manually for now
// --------------
void DebugLog(const char* _fmt, ...)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
char Msg[512];
    va_list ap;

    va_start( ap, _fmt );
    vsprintf( Msg, _fmt, ap );
    va_end( ap );

	// Only show certain messages
	std::string sMsg = Msg;
	if(sMsg.find("Mail") != -1 || sMsg.find("AX") != -1)
		// no match = -1
	{

#ifdef _WIN32
		OutputDebugString(Msg);
#endif
		g_dspInitialize.pLog(Msg,0);
	}

#endif
}
// =============


void ErrorLog(const char* _fmt, ...)
{
	char Msg[512];
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Msg, _fmt, ap);
	va_end(ap);

	g_dspInitialize.pLog(Msg,0);
#ifdef _WIN32
	::MessageBox(NULL, Msg, "Error", MB_OK);
#endif

	DSP_DebugBreak(); // NOTICE: we also break the emulation if this happens
}


// =======================================================================================
// For PB address detection
// --------------
u32 RAM_MASK = 0x1FFFFFF;


u16 Memory_Read_U16(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap16(*(u16*)&g_dsp.cpu_ram[_uAddress]);
}

u32 Memory_Read_U32(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap32(*(u32*)&g_dsp.cpu_ram[_uAddress]);
}

#if PROFILE

#define PROFILE_MAP_SIZE 0x10000

u64 g_profileMap[PROFILE_MAP_SIZE];
bool g_profile = false;

void ProfilerStart()
{
	g_profile = true;
}

void ProfilerAddDelta(int _addr, int _delta)
{
	if (g_profile)
	{
		g_profileMap[_addr] += _delta;
	}	
}

void ProfilerInit()
{
	memset(g_profileMap, 0, sizeof(g_profileMap));
}

void ProfilerDump(uint64 count)
{
	FILE* pFile = fopen("c:\\_\\DSP_Prof.txt", "wt");
	if (pFile != NULL)
	{
		fprintf(pFile, "Number of DSP steps: %llu\n\n", count);
		for (int i=0; i<PROFILE_MAP_SIZE;i++)
		{
			if (g_profileMap[i] > 0)
			{
				fprintf(pFile, "0x%04X: %llu\n", i, g_profileMap[i]);
			}
		}

		fclose(pFile);
	}
}

#endif
