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
class JitArm : public JitBase, public ArmGen::ARMCodeBlock
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
	void WriteCallInterpreter(UGeckoInstruction _inst);
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

	void UnsafeLoadToReg(ARMReg dest, ARMReg addr, int accessSize, s32 offsetReg, s32 offset);
	void SafeLoadToReg(bool fastmem, u32 dest, s32 addr, s32 offsetReg, int accessSize, s32 offset, bool signExtend, bool reverse);


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

	// Breakin shit
	void Break(UGeckoInstruction _inst);
	// Branch
	void bx(UGeckoInstruction _inst);
	void bcx(UGeckoInstruction _inst);
	void bclrx(UGeckoInstruction _inst);
	void sc(UGeckoInstruction _inst);
	void rfi(UGeckoInstruction _inst);
	void bcctrx(UGeckoInstruction _inst);

	// Integer
	void arith(UGeckoInstruction _inst);

	void addex(UGeckoInstruction _inst);
	void subfic(UGeckoInstruction _inst);
	void cntlzwx(UGeckoInstruction _inst);
	void cmp (UGeckoInstruction _inst);
	void cmpi(UGeckoInstruction _inst);
	void cmpl(UGeckoInstruction _inst);
	void cmpli(UGeckoInstruction _inst);
	void negx(UGeckoInstruction _inst);
	void mulhwux(UGeckoInstruction _inst);
	void rlwimix(UGeckoInstruction _inst);
	void rlwinmx(UGeckoInstruction _inst);
	void rlwnmx(UGeckoInstruction _inst);
	void srawix(UGeckoInstruction _inst);
	void extshx(UGeckoInstruction inst);
	void extsbx(UGeckoInstruction inst);

	// System Registers
	void mtmsr(UGeckoInstruction _inst);
	void mfmsr(UGeckoInstruction _inst);
	void mtspr(UGeckoInstruction _inst);
	void mfspr(UGeckoInstruction _inst);
	void mftb(UGeckoInstruction _inst);
	void crXXX(UGeckoInstruction _inst);
	void mcrf(UGeckoInstruction _inst);
	void mfcr(UGeckoInstruction _inst);
	void mtcrf(UGeckoInstruction _inst);
	void mtsr(UGeckoInstruction _inst);
	void mfsr(UGeckoInstruction _inst);
	void mcrxr(UGeckoInstruction _inst);
	void twx(UGeckoInstruction _inst);

	// LoadStore
	void stX(UGeckoInstruction _inst);
	void lXX(UGeckoInstruction _inst);
	void lmw(UGeckoInstruction _inst);
	void stmw(UGeckoInstruction _inst);

	void icbi(UGeckoInstruction _inst);
	void dcbst(UGeckoInstruction _inst);

	// Floating point
	void fabsx(UGeckoInstruction _inst);
	void fnabsx(UGeckoInstruction _inst);
	void fnegx(UGeckoInstruction _inst);
	void faddsx(UGeckoInstruction _inst);
	void faddx(UGeckoInstruction _inst);
	void fsubsx(UGeckoInstruction _inst);
	void fsubx(UGeckoInstruction _inst);
	void fmulsx(UGeckoInstruction _inst);
	void fmulx(UGeckoInstruction _inst);
	void fmrx(UGeckoInstruction _inst);
	void fmaddsx(UGeckoInstruction _inst);
	void fmaddx(UGeckoInstruction _inst);
	void fctiwx(UGeckoInstruction _inst);
	void fctiwzx(UGeckoInstruction _inst);
	void fcmpo(UGeckoInstruction _inst);
	void fcmpu(UGeckoInstruction _inst);
	void fnmaddx(UGeckoInstruction _inst);
	void fnmaddsx(UGeckoInstruction _inst);
	void fresx(UGeckoInstruction _inst);
	void fselx(UGeckoInstruction _inst);
	void frsqrtex(UGeckoInstruction _inst);

	// Floating point loadStore
	void lfXX(UGeckoInstruction _inst);
	void stfXX(UGeckoInstruction _inst);
	void stfs(UGeckoInstruction _inst);

	// Paired Singles
	void ps_add(UGeckoInstruction _inst);
	void ps_div(UGeckoInstruction _inst);
	void ps_res(UGeckoInstruction _inst);
	void ps_sum0(UGeckoInstruction _inst);
	void ps_sum1(UGeckoInstruction _inst);
	void ps_madd(UGeckoInstruction _inst);
	void ps_nmadd(UGeckoInstruction _inst);
	void ps_msub(UGeckoInstruction _inst);
	void ps_nmsub(UGeckoInstruction _inst);
	void ps_madds0(UGeckoInstruction _inst);
	void ps_madds1(UGeckoInstruction _inst);
	void ps_sub(UGeckoInstruction _inst);
	void ps_mul(UGeckoInstruction _inst);
	void ps_muls0(UGeckoInstruction _inst);
	void ps_muls1(UGeckoInstruction _inst);
	void ps_merge00(UGeckoInstruction _inst);
	void ps_merge01(UGeckoInstruction _inst);
	void ps_merge10(UGeckoInstruction _inst);
	void ps_merge11(UGeckoInstruction _inst);
	void ps_mr(UGeckoInstruction _inst);
	void ps_neg(UGeckoInstruction _inst);
	void ps_abs(UGeckoInstruction _inst);
	void ps_nabs(UGeckoInstruction _inst);
	void ps_rsqrte(UGeckoInstruction _inst);
	void ps_sel(UGeckoInstruction _inst);
	void ps_cmpu0(UGeckoInstruction _inst);
	void ps_cmpu1(UGeckoInstruction _inst);
	void ps_cmpo0(UGeckoInstruction _inst);
	void ps_cmpo1(UGeckoInstruction _inst);

	// LoadStore paired
	void psq_l(UGeckoInstruction _inst);
	void psq_lx(UGeckoInstruction _inst);
	void psq_st(UGeckoInstruction _inst);
	void psq_stx(UGeckoInstruction _inst);
};
