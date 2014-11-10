// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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

#include "Common/x64ABI.h"
#include "Common/x64Analyzer.h"
#include "Common/x64Emitter.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitBackpatch.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitILCommon/IR.h"
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
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	void Trace();

	JitBlockCache *GetBlockCache() override { return &blocks; }

	void ClearCache() override;
	const u8 *GetDispatcher()
	{
		return asm_routines.dispatcher;  // asm_routines.dispatcher
	}

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
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();

	void GenerateCarry(Gen::X64Reg temp_reg);

	void tri_op(int d, int a, int b, bool reversible, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));

	void WriteCode(u32 exitAddress);

	// OPCODES
	void unknown_instruction(UGeckoInstruction _inst) override;
	void FallBackToInterpreter(UGeckoInstruction _inst) override;
	void DoNothing(UGeckoInstruction _inst) override;
	void HLEFunction(UGeckoInstruction _inst) override;

	void DynaRunTable4(UGeckoInstruction _inst) override;
	void DynaRunTable19(UGeckoInstruction _inst) override;
	void DynaRunTable31(UGeckoInstruction _inst) override;
	void DynaRunTable59(UGeckoInstruction _inst) override;
	void DynaRunTable63(UGeckoInstruction _inst) override;

};
