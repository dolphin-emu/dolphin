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

#include "AudioCommon.h"
#include <stdio.h>

#define WITH_DSP_ON_THREAD		1
#define DUMP_DSP_IMEM			0
#define PROFILE					0


void DSP_DebugBreak();


typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef unsigned int uint;

typedef signed char sint8;
typedef signed short sint16;
typedef signed int sint32;
typedef signed long long sint64;

typedef const uint32 cuint32;

u16 Memory_Read_U16(u32 _uAddress); // For PB address detection
u32 Memory_Read_U32(u32 _uAddress);

#if PROFILE	
	void ProfilerDump(uint64 _count);
	void ProfilerInit();
	void ProfilerAddDelta(int _addr, int _delta);
	void ProfilerStart();
#endif


#endif

