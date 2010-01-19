// Copyright (C) 2010 Dolphin Project.

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

#ifndef _JITBASE_H
#define _JITBASE_H

#include "JitCache.h"
#include "Jit_Util.h"  // for EmuCodeBlock
#include "JitBackpatch.h"  // for EmuCodeBlock
#include "JitAsmCommon.h"

#define JIT_OPCODE 0

// TODO: In the future, inherit this from CPUCoreBase and have Interpreter
// inherit from that too?
class JitBase : public EmuCodeBlock
{
protected:
	JitBlockCache blocks;
	TrampolineCache trampolines;

	struct JitOptions
	{
		bool optimizeStack;
		bool assumeFPLoadFromMem;
		bool enableBlocklink;
		bool fpAccurateFcmp;
		bool enableFastMem;
		bool optimizeGatherPipe;
		bool fastInterrupts;
		bool accurateSinglePrecision;
	};
	struct JitState
	{
		u32 compilerPC;
		u32 next_compilerPC;
		u32 blockStart;
		bool cancel;
		UGeckoInstruction next_inst;  // for easy peephole opt.
		int blockSize;
		int instructionNumber;
		int downcountAmount;

		bool isLastInstruction;
		bool forceUnsafeLoad;

		int fifoBytesThisBlock;

		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeOp *op;
		u8* rewriteStart;

		JitBlock *curBlock;
	};

public:
	// This should probably be removed from public:
	JitOptions jo;
	JitState js;

	JitBlockCache *GetBlockCache() { return &blocks; }

	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual void Jit(u32 em_address) = 0;
	virtual void ClearCache() = 0;
	virtual void Run() = 0;
	virtual void SingleStep() = 0;
	virtual void NotifyBreakpoint(u32 em_address, bool set) = 0;

	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx);

	virtual const CommonAsmRoutines *GetAsmRoutines() = 0;
	virtual const char *GetName() = 0;
};

extern JitBase *jit;

void Jit(u32 em_address);

// Merged routines that should be moved somewhere better
u32 Helper_Mask(u8 mb, u8 me);


#endif
