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

// Settings
// ----------
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
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitBackpatch.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class Jit64 : public Jitx86Base
{
private:
	GPRRegCache gpr;
	FPURegCache fpr;

	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;
	Jit64AsmRoutineManager asm_routines;

public:
	Jit64() : code_buffer(32000) {}
	~Jit64() {}

	void Init() override;
	void Shutdown() override;

	// Jit!

	void Jit(u32 em_address) override;
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	u32 RegistersInUse();

	JitBlockCache *GetBlockCache() override { return &blocks; }

	void Trace();

	void ClearCache() override;

	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;
	}
	const CommonAsmRoutines *GetAsmRoutines() override {
		return &asm_routines;
	}

	const char *GetName() override {
#if _M_X86_64
		return "JIT64";
#else
		return "JIT32";
#endif
	}
	// Run!

	void Run() override;
	void SingleStep() override;

	// Utilities for use by opcodes

	void WriteExit(u32 destination);
	void WriteExitDestInEAX();
	void WriteExceptionExit();
	void WriteExternalExceptionExit();
	void WriteRfiExitDestInEAX();
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();

	void GenerateConstantOverflow(bool overflow);
	void GenerateOverflow();
	void FinalizeCarryOverflow(bool oe, bool inv = false);
	void GetCarryEAXAndClear();
	void FinalizeCarryGenerateOverflowEAX(bool oe, bool inv = false);
	void GenerateCarry();
	void GenerateRC();
	void ComputeRC(const Gen::OpArg & arg);

	void tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg));
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool single, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg));

	// OPCODES
	void unknown_instruction(UGeckoInstruction _inst);
	void FallBackToInterpreter(UGeckoInstruction _inst);
	void DoNothing(UGeckoInstruction _inst);
	void HLEFunction(UGeckoInstruction _inst);

	void DynaRunTable4(UGeckoInstruction _inst);
	void DynaRunTable19(UGeckoInstruction _inst);
	void DynaRunTable31(UGeckoInstruction _inst);
	void DynaRunTable59(UGeckoInstruction _inst);
	void DynaRunTable63(UGeckoInstruction _inst);

	void addx(UGeckoInstruction inst);
	void addcx(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void divwx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void srawx(UGeckoInstruction inst);
	void addex(UGeckoInstruction inst);
	void addmex(UGeckoInstruction inst);
	void addzex(UGeckoInstruction inst);

	void extsbx(UGeckoInstruction inst);
	void extshx(UGeckoInstruction inst);

	void sc(UGeckoInstruction _inst);
	void rfi(UGeckoInstruction _inst);

	void bx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction _inst);
	void bcctrx(UGeckoInstruction _inst);
	void bcx(UGeckoInstruction inst);

	void mtspr(UGeckoInstruction inst);
	void mfspr(UGeckoInstruction inst);
	void mtmsr(UGeckoInstruction inst);
	void mfmsr(UGeckoInstruction inst);
	void mftb(UGeckoInstruction inst);
	void mtcrf(UGeckoInstruction inst);
	void mfcr(UGeckoInstruction inst);
	void mcrf(UGeckoInstruction inst);
	void mcrxr(UGeckoInstruction inst);

	void boolX(UGeckoInstruction inst);
	void crXXX(UGeckoInstruction inst);

	void reg_imm(UGeckoInstruction inst);

	void ps_sel(UGeckoInstruction inst);
	void ps_mr(UGeckoInstruction inst);
	void ps_sign(UGeckoInstruction inst); //aggregate
	void ps_arith(UGeckoInstruction inst); //aggregate
	void ps_mergeXX(UGeckoInstruction inst);
	void ps_maddXX(UGeckoInstruction inst);
	void ps_sum(UGeckoInstruction inst);
	void ps_muls(UGeckoInstruction inst);

	void fp_arith(UGeckoInstruction inst);

	void fcmpx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);

	void cmpXX(UGeckoInstruction inst);

	void cntlzwx(UGeckoInstruction inst);

	void lfs(UGeckoInstruction inst);
	void lfd(UGeckoInstruction inst);
	void stfd(UGeckoInstruction inst);
	void stfs(UGeckoInstruction inst);
	void stfsx(UGeckoInstruction inst);
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

	void fmaddXX(UGeckoInstruction inst);
	void fsign(UGeckoInstruction inst);
	void fselx(UGeckoInstruction inst);
	void stX(UGeckoInstruction inst); //stw sth stb
	void rlwinmx(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void slwx(UGeckoInstruction inst);
	void srwx(UGeckoInstruction inst);
	void dcbst(UGeckoInstruction inst);
	void dcbz(UGeckoInstruction inst);
	void lfsx(UGeckoInstruction inst);

	void subfic(UGeckoInstruction inst);
	void subfcx(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void subfex(UGeckoInstruction inst);
	void subfmex(UGeckoInstruction inst);
	void subfzex(UGeckoInstruction inst);

	void twx(UGeckoInstruction inst);

	void lXXx(UGeckoInstruction inst);

	void stXx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);

	void icbi(UGeckoInstruction inst);
};
