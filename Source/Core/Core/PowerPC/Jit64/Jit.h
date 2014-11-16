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

#ifdef _WIN32
#include <winnt.h>
#endif

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
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class Jit64 : public Jitx86Base
{
private:
	void AllocStack();
	void FreeStack();

	GPRRegCache gpr;
	FPURegCache fpr;

	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;
	Jit64AsmRoutineManager asm_routines;

	bool m_enable_blr_optimization;
	bool m_clear_cache_asap;
	u8* m_stack;

public:
	Jit64() : code_buffer(32000) {}
	~Jit64() {}

	void Init() override;

	void EnableOptimization();

	void EnableBlockLink();

	void Shutdown() override;

	bool HandleFault(uintptr_t access_address, SContext* ctx) override;

	// Jit!

	void Jit(u32 em_address) override;
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	BitSet32 CallerSavedRegistersInUse();

	JitBlockCache *GetBlockCache() override { return &blocks; }

	void Trace();

	void ClearCache() override;

	const u8 *GetDispatcher()
	{
		return asm_routines.dispatcher;
	}

	const CommonAsmRoutines *GetAsmRoutines() override
	{
		return &asm_routines;
	}

	const char *GetName() override
	{
		return "JIT64";
	}

	// Run!
	void Run() override;
	void SingleStep() override;

	// Utilities for use by opcodes

	void WriteExit(u32 destination, bool bl = false, u32 after = 0);
	void JustWriteExit(u32 destination, bool bl, u32 after);
	void WriteExitDestInRSCRATCH(bool bl = false, u32 after = 0);
	void WriteBLRExit();
	void WriteExceptionExit();
	void WriteExternalExceptionExit();
	void WriteRfiExitDestInRSCRATCH();
	void WriteCallInterpreter(UGeckoInstruction _inst);
	bool Cleanup();

	void GenerateConstantOverflow(bool overflow);
	void GenerateConstantOverflow(s64 val);
	void GenerateOverflow();
	void FinalizeCarryOverflow(bool oe, bool inv = false);
	void FinalizeCarry(Gen::CCFlags cond);
	void FinalizeCarry(bool ca);
	void ComputeRC(const Gen::OpArg & arg, bool needs_test = true, bool needs_sext = true);

	// Use to extract bytes from a register using the regcache. offset is in bytes.
	Gen::OpArg ExtractFromReg(int reg, int offset);
	void AndWithMask(Gen::X64Reg reg, u32 mask);
	bool CheckMergedBranch(int crf);
	void DoMergedBranch();
	void DoMergedBranchCondition();
	void DoMergedBranchImmediate(s64 val);

	// Reads a given bit of a given CR register part.
	void GetCRFieldBit(int field, int bit, Gen::X64Reg out, bool negate = false);
	// Clobbers RDX.
	void SetCRFieldBit(int field, int bit, Gen::X64Reg in);
	void ClearCRFieldBit(int field, int bit);

	// Generates a branch that will check if a given bit of a CR register part
	// is set or not.
	Gen::FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set = true);
	void SetFPRFIfNeeded(UGeckoInstruction inst, Gen::X64Reg xmm);

	void MultiplyImmediate(u32 imm, int a, int d, bool overflow);

	void tri_op(int d, int a, int b, bool reversible, void (XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, Gen::OpArg),
	            void (Gen::XEmitter::*sseOp)(Gen::X64Reg, Gen::OpArg), UGeckoInstruction inst, bool roundRHS = false);
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
		          bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool single, void (Gen::XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, Gen::OpArg),
	               void (Gen::XEmitter::*sseOp)(Gen::X64Reg, Gen::OpArg), UGeckoInstruction inst, bool roundRHS = false);
	void FloatCompare(UGeckoInstruction inst, bool upper = false);

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
	void arithcx(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwXx(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void divwx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void srawx(UGeckoInstruction inst);
	void arithXex(UGeckoInstruction inst);

	void extsXx(UGeckoInstruction inst);

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
	void ps_res(UGeckoInstruction inst);
	void ps_rsqrte(UGeckoInstruction inst);
	void ps_sum(UGeckoInstruction inst);
	void ps_muls(UGeckoInstruction inst);
	void ps_cmpXX(UGeckoInstruction inst);

	void fp_arith(UGeckoInstruction inst);

	void fcmpx(UGeckoInstruction inst);
	void fctiwx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);
	void frspx(UGeckoInstruction inst);
	void frsqrtex(UGeckoInstruction inst);
	void fresx(UGeckoInstruction inst);

	void cmpXX(UGeckoInstruction inst);

	void cntlzwx(UGeckoInstruction inst);

	void lfXXX(UGeckoInstruction inst);
	void stfXXX(UGeckoInstruction inst);
	void stfiwx(UGeckoInstruction inst);
	void psq_lXX(UGeckoInstruction inst);
	void psq_stXX(UGeckoInstruction inst);

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

	void subfic(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);

	void twx(UGeckoInstruction inst);

	void lXXx(UGeckoInstruction inst);

	void stXx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);

	void icbi(UGeckoInstruction inst);
};
