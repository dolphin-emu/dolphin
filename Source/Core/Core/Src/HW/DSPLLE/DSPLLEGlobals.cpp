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

#include "Common.h" // for Common::swap
#include "FileUtil.h"
#include "DSP/DSPCore.h"
#include "DSPLLEGlobals.h"

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

void ProfilerDump(u64 count)
{
	File::IOFile pFile("DSP_Prof.txt", "wt");
	if (pFile)
	{
		fprintf(pFile.GetHandle(), "Number of DSP steps: %llu\n\n", count);
		for (int i=0; i<PROFILE_MAP_SIZE;i++)
		{
			if (g_profileMap[i] > 0)
			{
				fprintf(pFile.GetHandle(), "0x%04X: %llu\n", i, g_profileMap[i]);
			}
		}
	}
}

#endif
