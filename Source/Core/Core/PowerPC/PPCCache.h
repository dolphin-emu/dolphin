// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace PowerPC
{

	const u32 ICACHE_SETS = 128;
	const u32 ICACHE_WAYS = 8;
	// size of an instruction cache block in words
	const u32 ICACHE_BLOCK_SIZE = 8;

	const u32 ICACHE_EXRAM_BIT = 0x10000000;
	const u32 ICACHE_VMEM_BIT = 0x20000000;

	struct InstructionCache
	{
		u32 data[ICACHE_SETS][ICACHE_WAYS][ICACHE_BLOCK_SIZE];
		u32 tags[ICACHE_SETS][ICACHE_WAYS];
		u32 plru[ICACHE_SETS];
		u32 valid[ICACHE_SETS];

		u32 way_from_valid[255];
		u32 way_from_plru[128];

		u8 lookup_table[1<<20];
		u8 lookup_table_ex[1<<21];
		u8 lookup_table_vmem[1<<20];

		InstructionCache();
		u32 ReadInstruction(u32 addr);
		void Invalidate(u32 addr);
		void Init();
		void Reset();
	};

}
