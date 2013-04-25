// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _JITBASE_H
#define _JITBASE_H

//#define JIT_LOG_X86     // Enables logging of the generated x86 code
//#define JIT_LOG_GPR     // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR     // Enables logging of the PPC floating point regs

#include "../CPUCoreBase.h"
#include "JitCache.h"
#include "Jit_Util.h"  // for EmuCodeBlock
#include "JitBackpatch.h"  // for EmuCodeBlock
#include "JitAsmCommon.h"

#include <set>

#define JIT_OPCODE 0

class JitBase : public CPUCoreBase
{
protected:
	struct JitOptions
	{
		bool optimizeStack;
		bool enableBlocklink;
		bool fpAccurateFcmp;
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
		u32 numLoadStoreInst;
		u32 numFloatingPointInst;

		bool firstFPInstructionFound;
		bool isLastInstruction;
		bool forceUnsafeLoad;
		bool memcheck;
		bool skipnext;
		bool broken_block;
		int block_flags;

		int fifoBytesThisBlock;

		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeOp *op;
		u8* rewriteStart;

		JitBlock *curBlock;

		std::set<u32> fifoWriteAddresses;
	};

public:
	// This should probably be removed from public:
	JitOptions jo;
	JitState js;
	
	virtual JitBaseBlockCache *GetBlockCache() = 0;

	virtual void Jit(u32 em_address) = 0;

	virtual	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx) = 0;

	virtual const CommonAsmRoutinesBase *GetAsmRoutines() = 0;

	virtual bool IsInCodeSpace(u8 *ptr) = 0;
};

class Jitx86Base : public JitBase, public EmuCodeBlock
{
protected:
	JitBlockCache blocks;
	TrampolineCache trampolines;	
public:
	JitBlockCache *GetBlockCache() { return &blocks; }
	
	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx);

	bool IsInCodeSpace(u8 *ptr) { return IsInSpace(ptr); }
};

extern JitBase *jit;

void Jit(u32 em_address);

// Merged routines that should be moved somewhere better
u32 Helper_Mask(u8 mb, u8 me);
void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer *code_buffer, const u8 *normalEntry, JitBlock *b);

#endif
