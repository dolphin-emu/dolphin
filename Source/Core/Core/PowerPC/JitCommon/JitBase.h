// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

//#define JIT_LOG_X86     // Enables logging of the generated x86 code
//#define JIT_LOG_GPR     // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR     // Enables logging of the PPC floating point regs

#include <unordered_set>

#include "Common/x64ABI.h"
#include "Common/x64Analyzer.h"
#include "Common/x64Emitter.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitBackpatch.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

// Use these to control the instruction selection
// #define INSTRUCTION_START FallBackToInterpreter(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START

#define JITDISABLE(setting)                     \
	if (Core::g_CoreStartupParameter.bJITOff || \
		Core::g_CoreStartupParameter.setting)   \
	{ FallBackToInterpreter(inst); return; }

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
		UGeckoInstruction next_inst;  // for easy peephole opt.
		int instructionNumber;
		int downcountAmount;
		u32 numLoadStoreInst;
		u32 numFloatingPointInst;

		bool firstFPInstructionFound;
		bool isLastInstruction;
		bool memcheck;
		bool skipnext;

		int fifoBytesThisBlock;

		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeOp *op;
		u8* rewriteStart;

		JitBlock *curBlock;

		std::unordered_set<u32> fifoWriteAddresses;
	};

	PPCAnalyst::CodeBlock code_block;
	PPCAnalyst::PPCAnalyzer analyzer;

public:
	// This should probably be removed from public:
	JitOptions jo;
	JitState js;

	virtual JitBaseBlockCache *GetBlockCache() = 0;

	virtual void Jit(u32 em_address) = 0;

	virtual const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) = 0;

	virtual const CommonAsmRoutinesBase *GetAsmRoutines() = 0;

	virtual bool IsInCodeSpace(u8 *ptr) = 0;
};

class Jitx86Base : public JitBase, public EmuCodeBlock
{
protected:
	JitBlockCache blocks;
	TrampolineCache trampolines;
public:
	JitBlockCache *GetBlockCache() override { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) override;

	bool IsInCodeSpace(u8 *ptr) override { return IsInSpace(ptr); }
};

extern JitBase *jit;

void Jit(u32 em_address);

// Merged routines that should be moved somewhere better
u32 Helper_Mask(u8 mb, u8 me);
void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer *code_buffer, const u8 *normalEntry, JitBlock *b);
