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

#ifndef _JITCACHE_H
#define _JITCACHE_H

#include <map>
#include <vector>

#include "../Gekko.h"
#include "../PPCAnalyst.h"

#ifdef _WIN32
#include <windows.h>
#endif

// emulate CPU with unlimited instruction cache
// the only way to invalidate a region is the "icbi" instruction
#define JIT_UNLIMITED_ICACHE

#define JIT_ICACHE_SIZE 0x2000000
#define JIT_ICACHE_MASK 0x1ffffff
#define JIT_ICACHEEX_SIZE 0x4000000
#define JIT_ICACHEEX_MASK 0x3ffffff
#define JIT_ICACHE_EXRAM_BIT 0x10000000
#define JIT_ICACHE_VMEM_BIT 0x20000000
// this corresponds to opcode 5 which is invalid in PowerPC
#define JIT_ICACHE_INVALID_BYTE 0x14
#define JIT_ICACHE_INVALID_WORD 0x14141414

// TODO(ector) - optimize this struct for size
struct JitBlock
{
	u32 exitAddress[2];  // 0xFFFFFFFF == unknown
	u8 *exitPtrs[2];     // to be able to rewrite the exit jump
	bool linkStatus[2];

	u32 originalAddress;
	u32 originalFirstOpcode; //to be able to restore
	u32 codeSize; 
	u32 originalSize;
	int runCount;  // for profiling.
	int blockNum;

#ifdef _WIN32
	// we don't really need to save start and stop
	// TODO (mb2): ticStart and ticStop -> "local var" mean "in block" ... low priority ;)
	LARGE_INTEGER ticStart;		// for profiling - time.
	LARGE_INTEGER ticStop;		// for profiling - time.
	LARGE_INTEGER ticCounter;	// for profiling - time.
#endif
	const u8 *checkedEntry;
	const u8 *normalEntry;
	bool invalid;
	int flags;

	bool ContainsAddress(u32 em_address);
};

typedef void (*CompiledCode)();

class JitBlockCache
{
	const u8 **blockCodePointers;
	JitBlock *blocks;
	int num_blocks;
	std::multimap<u32, int> links_to;
	std::map<std::pair<u32,u32>, u32> block_map; // (end_addr, start_addr) -> number
#ifdef JIT_UNLIMITED_ICACHE
	u8 *iCache;
	u8 *iCacheEx;
	u8 *iCacheVMEM;
#endif
	int MAX_NUM_BLOCKS;

	bool RangeIntersect(int s1, int e1, int s2, int e2) const;
	void LinkBlockExits(int i);
	void LinkBlock(int i);

public:
	JitBlockCache() {}

	int AllocateBlock(u32 em_address);
	void FinalizeBlock(int block_num, bool block_link, const u8 *code_ptr);

	void Clear();
	void Init();
	void Shutdown();
	void Reset();

	bool IsFull() const;

	// Code Cache
	JitBlock *GetBlock(int block_num);
	int GetNumBlocks() const;
	const u8 **GetCodePointers();
#ifdef JIT_UNLIMITED_ICACHE
	u8 *GetICache();
	u8 *GetICacheEx();
	u8 *GetICacheVMEM();
#endif

	// Fast way to get a block. Only works on the first ppc instruction of a block.
	int GetBlockNumberFromStartAddress(u32 em_address);

    // slower, but can get numbers from within blocks, not just the first instruction.
	// WARNING! WILL NOT WORK WITH INLINING ENABLED (not yet a feature but will be soon)
	// Returns a list of block numbers - only one block can start at a particular address, but they CAN overlap.
	// This one is slow so should only be used for one-shots from the debugger UI, not for anything during runtime.
	void GetBlockNumbersFromAddress(u32 em_address, std::vector<int> *block_numbers);

	u32 GetOriginalFirstOp(int block_num);
	CompiledCode GetCompiledCodeFromBlock(int block_num);

	// DOES NOT WORK CORRECTLY WITH INLINING
	void InvalidateICache(u32 em_address);
	void DestroyBlock(int block_num, bool invalidate);

	// Not currently used
	//void DestroyBlocksWithFlag(BlockFlag death_flag);
};

#endif
