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
#ifndef _JITCACHE_H
#define _JITCACHE_H

#include <map>

#include "../Gekko.h"

#ifdef _WIN32
#include <windows.h>
#endif

enum BlockFlag
{
	BLOCK_USE_GQR0 = 0x1, BLOCK_USE_GQR1 = 0x2, BLOCK_USE_GQR2 = 0x4, BLOCK_USE_GQR3 = 0x8,
	BLOCK_USE_GQR4 = 0x10, BLOCK_USE_GQR5 = 0x20, BLOCK_USE_GQR6 = 0x40, BLOCK_USE_GQR7 = 0x80,
};

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

#ifdef _WIN32
	// we don't really need to save start and stop
	// TODO (mb2): ticStart and ticStop -> "local var" mean "in block" ... low priority ;)
	LARGE_INTEGER ticStart;		// for profiling - time.
	LARGE_INTEGER ticStop;		// for profiling - time.
	LARGE_INTEGER ticCounter;	// for profiling - time.
#endif
	const u8 *checkedEntry;
	bool invalid;
	int flags;
};

class Jit64;

typedef void (*CompiledCode)();

class JitBlockCache
{
	Jit64 *jit;

	u8 **blockCodePointers;
	JitBlock *blocks;
	int numBlocks;
	std::multimap<u32, int> links_to;

	int MAX_NUM_BLOCKS;

public:
	JitBlockCache() {}

	void SetJit(Jit64 *jit_) { jit = jit_; }

	const u8* Jit(u32 emaddress);
	void Clear();
	void Init();
	void Shutdown();
	void Reset();

	bool IsFull() const;
	// Code Cache
	u32 GetOriginalCode(u32 address);
	JitBlock *GetBlock(int no);
	void InvalidateCodeRange(u32 address, u32 length);
	int GetBlockNumberFromAddress(u32 address);
	CompiledCode GetCompiledCode(u32 address);
	CompiledCode GetCompiledCodeFromBlock(int blockNumber);
	int GetNumBlocks() const;
	u8 **GetCodePointers();
	void DestroyBlocksWithFlag(BlockFlag death_flag);
	void LinkBlocks();
	void LinkBlockExits(int i);
	void LinkBlock(int i);
	void DestroyBlock(int blocknum, bool invalidate);
	bool RangeIntersect(int s1, int e1, int s2, int e2) const;
};

#endif
