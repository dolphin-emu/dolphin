// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Common.h"
#include "AudioCommon.h"
#include <stdio.h>

#define PROFILE					0

void DSP_DebugBreak();

u16 Memory_Read_U16(u32 _uAddress); // For PB address detection
u32 Memory_Read_U32(u32 _uAddress);

#if PROFILE	
	void ProfilerDump(u64 _count);
	void ProfilerInit();
	void ProfilerAddDelta(int _addr, int _delta);
	void ProfilerStart();
#endif


#endif

