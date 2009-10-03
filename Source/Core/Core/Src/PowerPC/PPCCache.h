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

#ifndef _PPCCACHE_H
#define _PPCCACHE_H

#include "Common.h"

#define FAST_ICACHE

namespace PowerPC
{

	const u32 ICACHE_SETS = 128;	
	const u32 ICACHE_WAYS = 8;
	// size of an instruction cache block in words
	const u32 ICACHE_BLOCK_SIZE = 8;

	struct InstructionCache
	{
		u32 data[ICACHE_SETS][ICACHE_WAYS][ICACHE_BLOCK_SIZE];
		u32 tags[ICACHE_SETS][ICACHE_WAYS];
		u32 plru[ICACHE_SETS];
		u32 valid[ICACHE_SETS];

		u32 way_from_valid[255];
		u32 way_from_plru[128];

#ifdef FAST_ICACHE
		u8 lookup_table[1<<20];
#endif

		InstructionCache();
		void Reset();
		u32 ReadInstruction(u32 addr);
		void Invalidate(u32 addr);
	};

}

#endif