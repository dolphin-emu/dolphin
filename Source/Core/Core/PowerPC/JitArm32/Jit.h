// Copyright 2014 Dolphin Emulator Project
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

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm32/JitArmCache.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitFPRCache.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#define PPCSTATE_OFF(elem) ((s32)STRUCT_OFF(PowerPC::ppcState, elem) - (s32)STRUCT_OFF(PowerPC::ppcState, spr[0]))
class JitArm : public JitBase, public ArmGen::ARMXCodeBlock
{
private:
	JitArmBlockCache blocks;

	JitArmAsmRoutineManager asm_routines;

	// TODO: Make arm specific versions of these, shouldn't be too hard to
	// make it so we allocate some space at the start(?) of code generation
	// and keep the registers in a cache. Will burn this bridge when we get to
	// it.
	ArmRegCache gpr;
	ArmFPRCache fpr;

	PPCAnalyst::CodeBuffer code_buffer;
	void DoDownCount();

	void PrintDebug(UGeckoInstruction inst, u32 level);

	void Helper_UpdateCR1(ARMReg fpscr, ARMReg temp);

	void SetFPException(ARMReg Reg, u32 Exception);
public:
	JitArm() : code_buffer(32000) {}
	~JitArm() {}

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b);

	JitBaseBlockCache *GetBlockCache() { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx);

	bool IsInCodeSpace(u8 *ptr) { return IsInSpace(ptr); }

	void Trace();

	void ClearCache();

	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;
	}
	CommonAsmRoutinesBase *GetAsmRoutines() {
		return &asm_routines;
	}

	const char *GetName() {
		return "JITARM";
	}
	// Run!

	void Run();
	void SingleStep();

	// Utilities for use by opcodes

	void WriteExit(u32 destination);
	void WriteExitDestInR(ARMReg Reg);
	void WriteRfiExitDestInR(ARMReg Reg);
	void WriteExceptionExit();
	void WriteCallInterpreter(UGeckoInstruction inst);
	void Cleanup();

	void GenerateRC(int cr = 0);
	void ComputeRC(int cr = 0);
	void ComputeRC(s32 value, int cr);

	void ComputeCarry();
	void ComputeCarry(bool Carry);
	void GetCarryAndClear(ARMReg reg);
	void FinalizeCarry(ARMReg reg);

	// TODO: This shouldn't be here
	void UnsafeStoreFromReg(ARMReg dest, ARMReg value, int accessSize, s32 offset);
	void SafeStoreFromReg(bool fastmem, s32 dest, u32 value, s32 offsetReg, int accessSize, s32 offset);

	void UnsafeLoadToReg(ARMReg dest, ARMReg addr, int accessSize, s32 offset);
	void SafeLoadToReg(bool fastmem, u32 dest, s32 addr, s32 offsetReg, int accessSize, s32 offset, bool signExtend, bool reverse);


	// OPCODES
	void unknown_instruction(UGeckoInstruction inst);
	void FallBackToInterpreter(UGeckoInstruction inst);
	void DoNothing(UGeckoInstruction inst);
	void HLEFunction(UGeckoInstruction inst);

	void DynaRunTable4(UGeckoInstruction inst);
	void DynaRunTable19(UGeckoInstruction inst);
	void DynaRunTable31(UGeckoInstruction inst);
	void DynaRunTable59(UGeckoInstruction inst);
	void DynaRunTable63(UGeckoInstruction inst);

	// Breakin shit
	void Break(UGeckoInstruction inst);
	// Branch
	void bx(UGeckoInstruction inst);
	void bcx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction inst);
	void sc(UGeckoInstruction inst);
	void rfi(UGeckoInstruction inst);
	void bcctrx(UGeckoInstruction inst);

	// Integer
	void arith(UGeckoInstruction inst);

	void addex(UGeckoInstruction inst);
	void subfic(UGeckoInstruction inst);
	void cntlzwx(UGeckoInstruction inst);
	void cmp (UGeckoInstruction inst);
	void cmpi(UGeckoInstruction inst);
	void cmpl(UGeckoInstruction inst);
	void cmpli(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void rlwinmx(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void extshx(UGeckoInstruction inst);
	void extsbx(UGeckoInstruction inst);

	// System Registers
	void mtmsr(UGeckoInstruction inst);
	void mfmsr(UGeckoInstruction inst);
	void mtspr(UGeckoInstruction inst);
	void mfspr(UGeckoInstruction inst);
	void mftb(UGeckoInstruction inst);
	void crXXX(UGeckoInstruction inst);
	void mcrf(UGeckoInstruction inst);
	void mfcr(UGeckoInstruction inst);
	void mtcrf(UGeckoInstruction inst);
	void mtsr(UGeckoInstruction inst);
	void mfsr(UGeckoInstruction inst);
	void mcrxr(UGeckoInstruction inst);
	void twx(UGeckoInstruction inst);

	// LoadStore
	void stX(UGeckoInstruction inst);
	void lXX(UGeckoInstruction inst);
	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);

	void icbi(UGeckoInstruction inst);
	void dcbst(UGeckoInstruction inst);

	// Floating point
	void fabsx(UGeckoInstruction inst);
	void fnabsx(UGeckoInstruction inst);
	void fnegx(UGeckoInstruction inst);
	void faddsx(UGeckoInstruction inst);
	void faddx(UGeckoInstruction inst);
	void fsubsx(UGeckoInstruction inst);
	void fsubx(UGeckoInstruction inst);
	void fmulsx(UGeckoInstruction inst);
	void fmulx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);
	void fmaddsx(UGeckoInstruction inst);
	void fmaddx(UGeckoInstruction inst);
	void fctiwx(UGeckoInstruction inst);
	void fctiwzx(UGeckoInstruction inst);
	void fcmpo(UGeckoInstruction inst);
	void fcmpu(UGeckoInstruction inst);
	void fnmaddx(UGeckoInstruction inst);
	void fnmaddsx(UGeckoInstruction inst);
	void fresx(UGeckoInstruction inst);
	void fselx(UGeckoInstruction inst);
	void frsqrtex(UGeckoInstruction inst);

	// Floating point loadStore
	void lfXX(UGeckoInstruction inst);
	void stfXX(UGeckoInstruction inst);
	void stfs(UGeckoInstruction inst);

	// Paired Singles
	void ps_add(UGeckoInstruction inst);
	void ps_div(UGeckoInstruction inst);
	void ps_res(UGeckoInstruction inst);
	void ps_sum0(UGeckoInstruction inst);
	void ps_sum1(UGeckoInstruction inst);
	void ps_madd(UGeckoInstruction inst);
	void ps_nmadd(UGeckoInstruction inst);
	void ps_msub(UGeckoInstruction inst);
	void ps_nmsub(UGeckoInstruction inst);
	void ps_madds0(UGeckoInstruction inst);
	void ps_madds1(UGeckoInstruction inst);
	void ps_sub(UGeckoInstruction inst);
	void ps_mul(UGeckoInstruction inst);
	void ps_muls0(UGeckoInstruction inst);
	void ps_muls1(UGeckoInstruction inst);
	void ps_merge00(UGeckoInstruction inst);
	void ps_merge01(UGeckoInstruction inst);
	void ps_merge10(UGeckoInstruction inst);
	void ps_merge11(UGeckoInstruction inst);
	void ps_mr(UGeckoInstruction inst);
	void ps_neg(UGeckoInstruction inst);
	void ps_abs(UGeckoInstruction inst);
	void ps_nabs(UGeckoInstruction inst);
	void ps_rsqrte(UGeckoInstruction inst);
	void ps_sel(UGeckoInstruction inst);
	void ps_cmpu0(UGeckoInstruction inst);
	void ps_cmpu1(UGeckoInstruction inst);
	void ps_cmpo0(UGeckoInstruction inst);
	void ps_cmpo1(UGeckoInstruction inst);

	// LoadStore paired
	void psq_l(UGeckoInstruction inst);
	void psq_lx(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);
	void psq_stx(UGeckoInstruction inst);
};
