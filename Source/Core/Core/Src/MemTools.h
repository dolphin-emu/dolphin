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

#ifndef _MEMTOOLS_H
#define _MEMTOOLS_H

#include "Common.h"

typedef u32 EAddr;

namespace EMM
{
	enum WR
	{
		Read  = 1,
		Write = 2,
		Execute = 4
	};

	enum WatchType
	{
		Oneshot,
		Continuous
	};

	enum AccessSize
	{
		Access8,
		Access16,
		Access32,
		Access64,
		Access128
	};

	typedef int WatchID;
	typedef void (*WatchCallback)(EAddr addr, AccessSize size, WR action, WatchID id);

	//Useful to emulate low-used I/O, and caching of memory resources that can change any time
	WatchID AddWatchRegion(EAddr startAddr, EAddr endAddr, WR watchFor, WatchType type, WatchCallback callback, u64 userData);
	void RemoveWatchRegion(WatchID id);
	void ClearWatches();

	//Call this on your main emulator thread, with your mainloop in codeToRun
	
	void InstallExceptionHandler();
}

u8 ReadHandler8(EAddr address);
u16 ReadHandler16(EAddr address);
u32 ReadHandler32(EAddr address);
u64 ReadHandler64(EAddr address);
void WriteHandler8(EAddr address, u8 value);
void WriteHandler16(EAddr address, u16 value);
void WriteHandler32(EAddr address, u32 value);
void WriteHandler64(EAddr address, u64 value);

#endif
