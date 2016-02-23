// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ========================
// See comments in Jit.cpp.
// ========================

// Mystery: Capcom vs SNK 800aa278

// CR flags approach:
//   * Store that "N+Z flag contains CR0" or "S+Z flag contains CR3".
//   * All flag altering instructions flush this
//   * A flush simply does a conditional write to the appropriate CRx.
//   * If flag available, branch code can become absolutely trivial.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

class JitIL : public JitILBase
{
public:
	Jit64AsmRoutineManager asm_routines;

	JitIL() {}
	~JitIL() {}

	// Initialization, etc

	void Init() override;

	void EnableBlockLink();

	void Shutdown() override;

	// Jit!

	void Jit(u32 em_address) override;
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b, u32 nextPC);

	void Trace();

	JitBlockCache *GetBlockCache() override { return &blocks; }

	void ClearCache() override;

	const CommonAsmRoutines *GetAsmRoutines() override
	{
		return &asm_routines;
	}

	const char *GetName() override
	{
		return "JIT64IL";
	}

	// Run!
	void Run() override;
	void SingleStep() override;

	// Utilities for use by opcodes

	void WriteExit(u32 destination);
	void WriteExitDestInOpArg(const Gen::OpArg& arg);
	void WriteExceptionExit();
	void WriteRfiExitDestInOpArg(const Gen::OpArg& arg);
	void Cleanup();

	void WriteCode(u32 exitAddress);

	// OPCODES
	using Instruction = void (JitIL::*)(UGeckoInstruction instCode);
	void FallBackToInterpreter(UGeckoInstruction _inst) override;
	void DoNothing(UGeckoInstruction _inst) override;
	void HLEFunction(UGeckoInstruction _inst) override;

	void DynaRunTable4(UGeckoInstruction _inst) override;
	void DynaRunTable19(UGeckoInstruction _inst) override;
	void DynaRunTable31(UGeckoInstruction _inst) override;
	void DynaRunTable59(UGeckoInstruction _inst) override;
	void DynaRunTable63(UGeckoInstruction _inst) override;
};
