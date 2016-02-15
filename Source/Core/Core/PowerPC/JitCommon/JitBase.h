// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

//#define JIT_LOG_X86     // Enables logging of the generated x86 code
//#define JIT_LOG_GPR     // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR     // Enables logging of the PPC floating point regs

#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitCommon/TrampolineCache.h"

// TODO: find a better place for x86-specific stuff
// The following register assignments are common to Jit64 and Jit64IL:
// RSCRATCH and RSCRATCH2 are always scratch registers and can be used without
// limitation.
#define RSCRATCH RAX
#define RSCRATCH2 RDX
// RSCRATCH_EXTRA may be in the allocation order, so it has to be flushed
// before use.
#define RSCRATCH_EXTRA RCX
// RMEM points to the start of emulated memory.
#define RMEM RBX
// RPPCSTATE points to ppcState + 0x80.  It's offset because we want to be able
// to address as much as possible in a one-byte offset form.
#define RPPCSTATE RBP

// Use these to control the instruction selection
// #define INSTRUCTION_START FallBackToInterpreter(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START

#define FALLBACK_IF(cond) do { if (cond) { FallBackToInterpreter(inst); return; } } while (0)

#define JITDISABLE(setting) FALLBACK_IF(SConfig::GetInstance().bJITOff || \
                                        SConfig::GetInstance().setting)

class JitBase : public CPUCoreBase
{
protected:
	struct JitOptions
	{
		bool enableBlocklink;
		bool optimizeGatherPipe;
		bool accurateSinglePrecision;
		bool fastmem;
		bool memcheck;
		bool alwaysUseMemFuncs;
	};
	struct JitState
	{
		u32 compilerPC;
		u32 blockStart;
		int instructionNumber;
		int instructionsLeft;
		int downcountAmount;
		u32 numLoadStoreInst;
		u32 numFloatingPointInst;
		// If this is set, we need to generate an exception handler for the fastmem load.
		u8* fastmemLoadStore;
		// If this is set, a load or store already prepared a jump to the exception handler for us,
		// so just fixup that branch instead of testing for a DSI again.
		bool fixupExceptionHandler;
		Gen::FixupBranch exceptionHandler;
		// If these are set, we've stored the old value of a register which will be loaded in revertLoad,
		// which lets us revert it on the exception path.
		int revertGprLoad;
		int revertFprLoad;

		bool assumeNoPairedQuantize;
		bool firstFPInstructionFound;
		bool isLastInstruction;
		int skipInstructions;
		bool carryFlagSet;
		bool carryFlagInverted;

		int fifoBytesThisBlock;

		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeOp* op;
		u8* rewriteStart;

		JitBlock *curBlock;

		std::unordered_set<u32> fifoWriteAddresses;
		std::unordered_set<u32> pairedQuantizeAddresses;
	};

	PPCAnalyst::CodeBlock code_block;
	PPCAnalyst::PPCAnalyzer analyzer;

	bool MergeAllowedNextInstructions(int count);

	void UpdateMemoryOptions();

public:
	// This should probably be removed from public:
	JitOptions jo;
	JitState js;

	virtual JitBaseBlockCache *GetBlockCache() = 0;

	virtual void Jit(u32 em_address) = 0;

	virtual const CommonAsmRoutinesBase *GetAsmRoutines() = 0;

	virtual bool HandleFault(uintptr_t access_address, SContext* ctx) = 0;
	virtual bool HandleStackFault() { return false; }
};

class Jitx86Base : public JitBase, public EmuCodeBlock
{
protected:
	bool BackPatch(u32 emAddress, SContext* ctx);
	JitBlockCache blocks;
	TrampolineCache trampolines;
public:
	JitBlockCache *GetBlockCache() override { return &blocks; }
	bool HandleFault(uintptr_t access_address, SContext* ctx) override;
};

extern JitBase *jit;

void Jit(u32 em_address);

// Merged routines that should be moved somewhere better
u32 Helper_Mask(u8 mb, u8 me);
void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer *code_buffer, const u8 *normalEntry, JitBlock *b);
