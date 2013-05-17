// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
