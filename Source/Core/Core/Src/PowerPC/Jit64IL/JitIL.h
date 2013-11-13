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

#ifndef _JITIL_H
#define _JITIL_H

#include "JitILAsm.h"
#include "x64Emitter.h"
#include "x64ABI.h"
#include "x64Analyzer.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"
#include "../JitCommon/JitBase.h"
#include "../JitCommon/JitCache.h"
#include "../JitCommon/JitBackpatch.h"
#include "../JitCommon/Jit_Util.h"
#include "../JitILCommon/JitILBase.h"
#include "../JitILCommon/IR.h"
#include "../../ConfigManager.h"
#include "../../Core.h"
#include "../../CoreTiming.h"
#include "../../HW/Memmap.h"
#include "../../HW/GPFifo.h"

// #define INSTRUCTION_START Default(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START

#define JITDISABLE(setting) \
	if (Core::g_CoreStartupParameter.bJITOff || \
		Core::g_CoreStartupParameter.setting) \
		{Default(inst); return;}

#ifdef _M_X64
#define DISABLE64 \
	{Default(inst); return;}
#else
#define DISABLE64
#endif

class JitIL : public JitILBase, public EmuCodeBlock
{
private:
	JitBlockCache blocks;
	TrampolineCache trampolines;

public:
	JitILAsmRoutineManager asm_routines;

	JitIL() {}
	~JitIL() {}

	// Initialization, etc

	void Init() override;
	void Shutdown() override;

	// Jit!

	void Jit(u32 em_address) override;
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	void Trace();

	JitBlockCache *GetBlockCache() override { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) override { return NULL; };

	bool IsInCodeSpace(u8 *ptr) override { return IsInSpace(ptr); }

	void ClearCache() override;
	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;  // asm_routines.dispatcher
	}
	const CommonAsmRoutines *GetAsmRoutines() override {
		return &asm_routines;
	}

	const char *GetName() override {
#ifdef _M_X64
		return "JIT64IL";
#else
		return "JIT32IL";
#endif
	}

	// Run!

	void Run() override;
	void SingleStep() override;

	// Utilities for use by opcodes

	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInOpArg(const Gen::OpArg& arg);
	void WriteExceptionExit();
	void WriteRfiExitDestInOpArg(const Gen::OpArg& arg);
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();

	void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void GenerateCarry(Gen::X64Reg temp_reg);

	void tri_op(int d, int a, int b, bool reversible, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));

	void WriteCode();

	// OPCODES
	void unknown_instruction(UGeckoInstruction _inst) override;
	void Default(UGeckoInstruction _inst) override;
	void DoNothing(UGeckoInstruction _inst) override;
	void HLEFunction(UGeckoInstruction _inst) override;

	void DynaRunTable4(UGeckoInstruction _inst) override;
	void DynaRunTable19(UGeckoInstruction _inst) override;
	void DynaRunTable31(UGeckoInstruction _inst) override;
	void DynaRunTable59(UGeckoInstruction _inst) override;
	void DynaRunTable63(UGeckoInstruction _inst) override;
};

#endif  // _JITIL_H
