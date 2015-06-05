// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/Arm64Emitter.h"

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"
#include "Core/PowerPC/JitArmCommon/BackPatch.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#define PPCSTATE_OFF(elem) (offsetof(PowerPC::PowerPCState, elem))

// Some asserts to make sure we will be able to load everything
static_assert(PPCSTATE_OFF(spr[1023]) <= 16380, "LDR(32bit) can't reach the last SPR");
static_assert((PPCSTATE_OFF(ps[0][0]) % 8) == 0, "LDR(64bit VFP) requires FPRs to be 8 byte aligned");
static_assert(PPCSTATE_OFF(xer_ca) < 4096, "STRB can't store xer_ca!");
static_assert(PPCSTATE_OFF(xer_so_ov) < 4096, "STRB can't store xer_so_ov!");

class JitArm64 : public JitBase, public Arm64Gen::ARM64CodeBlock
{
public:
	JitArm64() : code_buffer(32000), m_float_emit(this) {}
	~JitArm64() {}

	void Init();
	void Shutdown();

	JitBaseBlockCache *GetBlockCache() { return &blocks; }

	bool IsInCodeSpace(u8 *ptr) const { return IsInSpace(ptr); }

	bool HandleFault(uintptr_t access_address, SContext* ctx) override;

	void ClearCache();

	CommonAsmRoutinesBase *GetAsmRoutines()
	{
		return &asm_routines;
	}

	void Run();
	void SingleStep();

	void Jit(u32);

	const char *GetName()
	{
		return "JITARM64";
	}

	// OPCODES
	void FallBackToInterpreter(UGeckoInstruction inst);
	void DoNothing(UGeckoInstruction inst);
	void HLEFunction(UGeckoInstruction inst);

	void DynaRunTable4(UGeckoInstruction inst);
	void DynaRunTable19(UGeckoInstruction inst);
	void DynaRunTable31(UGeckoInstruction inst);
	void DynaRunTable59(UGeckoInstruction inst);
	void DynaRunTable63(UGeckoInstruction inst);

	// Force break
	void Break(UGeckoInstruction inst);

	// Branch
	void sc(UGeckoInstruction inst);
	void rfi(UGeckoInstruction inst);
	void bx(UGeckoInstruction inst);
	void bcx(UGeckoInstruction inst);
	void bcctrx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction inst);

	// Integer
	void arith_imm(UGeckoInstruction inst);
	void boolX(UGeckoInstruction inst);
	void addx(UGeckoInstruction inst);
	void extsXx(UGeckoInstruction inst);
	void cntlzwx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void cmp(UGeckoInstruction inst);
	void cmpl(UGeckoInstruction inst);
	void cmpi(UGeckoInstruction inst);
	void cmpli(UGeckoInstruction inst);
	void rlwinmx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void addic(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void addzex(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void addcx(UGeckoInstruction inst);

	// System Registers
	void mtmsr(UGeckoInstruction inst);
	void mfmsr(UGeckoInstruction inst);
	void mcrf(UGeckoInstruction inst);
	void mfsr(UGeckoInstruction inst);
	void mtsr(UGeckoInstruction inst);
	void mfsrin(UGeckoInstruction inst);
	void mtsrin(UGeckoInstruction inst);
	void twx(UGeckoInstruction inst);
	void mfspr(UGeckoInstruction inst);
	void mftb(UGeckoInstruction inst);
	void mtspr(UGeckoInstruction inst);

	// LoadStore
	void icbi(UGeckoInstruction inst);
	void lXX(UGeckoInstruction inst);
	void stX(UGeckoInstruction inst);

	// LoadStore floating point
	void lfXX(UGeckoInstruction inst);
	void stfXX(UGeckoInstruction inst);

	// Floating point
	void fabsx(UGeckoInstruction inst);
	void faddsx(UGeckoInstruction inst);
	void faddx(UGeckoInstruction inst);
	void fmaddsx(UGeckoInstruction inst);
	void fmaddx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);
	void fmsubsx(UGeckoInstruction inst);
	void fmsubx(UGeckoInstruction inst);
	void fmulsx(UGeckoInstruction inst);
	void fmulx(UGeckoInstruction inst);
	void fnabsx(UGeckoInstruction inst);
	void fnegx(UGeckoInstruction inst);
	void fnmaddsx(UGeckoInstruction inst);
	void fnmaddx(UGeckoInstruction inst);
	void fnmsubsx(UGeckoInstruction inst);
	void fnmsubx(UGeckoInstruction inst);
	void fselx(UGeckoInstruction inst);
	void fsubsx(UGeckoInstruction inst);
	void fsubx(UGeckoInstruction inst);

	// Paired
	void ps_abs(UGeckoInstruction inst);
	void ps_add(UGeckoInstruction inst);
	void ps_div(UGeckoInstruction inst);
	void ps_madd(UGeckoInstruction inst);
	void ps_madds0(UGeckoInstruction inst);
	void ps_madds1(UGeckoInstruction inst);
	void ps_merge00(UGeckoInstruction inst);
	void ps_merge01(UGeckoInstruction inst);
	void ps_merge10(UGeckoInstruction inst);
	void ps_merge11(UGeckoInstruction inst);
	void ps_mr(UGeckoInstruction inst);
	void ps_msub(UGeckoInstruction inst);
	void ps_mul(UGeckoInstruction inst);
	void ps_muls0(UGeckoInstruction inst);
	void ps_muls1(UGeckoInstruction inst);
	void ps_nabs(UGeckoInstruction inst);
	void ps_nmadd(UGeckoInstruction inst);
	void ps_nmsub(UGeckoInstruction inst);
	void ps_neg(UGeckoInstruction inst);
	void ps_res(UGeckoInstruction inst);
	void ps_sel(UGeckoInstruction inst);
	void ps_sub(UGeckoInstruction inst);
	void ps_sum0(UGeckoInstruction inst);
	void ps_sum1(UGeckoInstruction inst);

	// Loadstore paired
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

private:
	Arm64GPRCache gpr;
	Arm64FPRCache fpr;

	JitArm64BlockCache blocks;
	JitArm64AsmRoutineManager asm_routines;

	PPCAnalyst::CodeBuffer code_buffer;

	ARM64FloatEmitter m_float_emit;

	// Dump a memory range of code
	void DumpCode(const u8* start, const u8* end);

	// The key is the backpatch flags
	std::map<u32, BackPatchInfo> m_backpatch_info;

	// Backpatching routines
	bool DisasmLoadStore(const u8* ptr, u32* flags, Arm64Gen::ARM64Reg* reg);
	void InitBackpatch();
	u32 EmitBackpatchRoutine(ARM64XEmitter* emit, u32 flags, bool fastmem, bool do_padding, Arm64Gen::ARM64Reg RS, Arm64Gen::ARM64Reg addr);
	// Loadstore routines
	void SafeLoadToReg(u32 dest, s32 addr, s32 offsetReg, u32 flags, s32 offset, bool update);
	void SafeStoreFromReg(s32 dest, u32 value, s32 regOffset, u32 flags, s32 offset);

	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b);

	void DoDownCount();

	// Profiling
	void BeginTimeProfile(JitBlock* b);
	void EndTimeProfile(JitBlock* b);

	// Exits
	void WriteExit(u32 destination);
	void WriteExceptionExit(Arm64Gen::ARM64Reg dest);
	void WriteExceptionExit();
	void WriteExitDestInR(Arm64Gen::ARM64Reg dest);

	FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set);

	void ComputeRC(Arm64Gen::ARM64Reg reg, int crf = 0);
	void ComputeRC(u32 imm, int crf = 0);
	void ComputeCarry(bool Carry);
	void ComputeCarry();

	typedef u32 (*Operation)(u32, u32);
	void reg_imm(u32 d, u32 a, bool binary, u32 value, Operation do_op, void (ARM64XEmitter::*op)(Arm64Gen::ARM64Reg, Arm64Gen::ARM64Reg, Arm64Gen::ARM64Reg, ArithOption), bool Rc = false);
};

