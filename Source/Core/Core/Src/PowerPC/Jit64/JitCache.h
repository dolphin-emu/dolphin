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

#include "../Gekko.h"

namespace Jit64
{

	typedef void (*CompiledCode)();

	void unknown_instruction(UGeckoInstruction _inst);

	//Code pointers are stored separately, they will be accessed much more frequently

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
		const u8 *checkedEntry;
		bool invalid;
	};

	void PrintStats();

	JitBlock *CurBlock();
	JitBlock *GetBlock(int no);

	u32 GetOriginalCode(u32 address);

	void InvalidateCodeRange(u32 address, u32 length);
	int GetBlockNumberFromAddress(u32 address);
	CompiledCode GetCompiledCode(u32 address);
	CompiledCode GetCompiledCodeFromBlock(int blockNumber);

	int GetCodeSize();
	int GetNumBlocks();

	u8 **GetCodePointers();

	u8 *Jit(u32 emaddress);
	void LinkBlocks();
	void ClearCache();

	void EnterFastRun();

	void InitCache();
	void ShutdownCache();
	void ResetCache();
}

#endif

