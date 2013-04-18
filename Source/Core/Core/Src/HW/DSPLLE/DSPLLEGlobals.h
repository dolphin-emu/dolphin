// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Common.h"
#include "AudioCommon.h"
#include <stdio.h>

// TODO: Get rid of this file.

#define PROFILE					0

#if PROFILE	
	void ProfilerDump(u64 _count);
	void ProfilerInit();
	void ProfilerAddDelta(int _addr, int _delta);
	void ProfilerStart();
#endif

#endif

