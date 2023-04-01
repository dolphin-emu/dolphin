// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <tuple>

#include "Common/Arm64Emitter.h"

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"
#include "Core/PowerPC/JitArmCommon/BackPatch.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

class JitArm64 : public JitBase, public Arm64Gen::ARM64CodeBlock, public CommonAsmRoutinesBase
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

	CommonAsmRoutinesBase *GetAsmRoutines() override
	{
		return this;
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
	void addix(UGeckoInstruction inst);
	void extsXx(UGeckoInstruction inst);
	void cntlzwx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void cmp(UGeckoInstruction inst);
	void cmpl(UGeckoInstruction inst);
	void cmpi(UGeckoInstruction inst);
	void cmpli(UGeckoInstruction inst);
	void rlwinmx(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void mulhwx(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void addic(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void addzex(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void addcx(UGeckoInstruction inst);
	void slwx(UGeckoInstruction inst);
	void srwx(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void subfex(UGeckoInstruction inst);
	void subfcx(UGeckoInstruction inst);
	void subfic(UGeckoInstruction inst);
	void addex(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);

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
	void crXXX(UGeckoInstruction inst);
	void mfcr(UGeckoInstruction inst);
	void mtcrf(UGeckoInstruction inst);

	// LoadStore
	void lXX(UGeckoInstruction inst);
	void stX(UGeckoInstruction inst);
	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);
	void dcbx(UGeckoInstruction inst);
	void dcbt(UGeckoInstruction inst);
	void dcbz(UGeckoInstruction inst);

	// LoadStore floating point
	void lfXX(UGeckoInstruction inst);
	void stfXX(UGeckoInstruction inst);

	// Floating point
	void fp_arith(UGeckoInstruction inst);
	void fp_logic(UGeckoInstruction inst);
	void fselx(UGeckoInstruction inst);
	void fcmpX(UGeckoInstruction inst);
	void frspx(UGeckoInstruction inst);
	void fctiwzx(UGeckoInstruction inst);

	// Paired
	void ps_maddXX(UGeckoInstruction inst);
	void ps_mergeXX(UGeckoInstruction inst);
	void ps_mulsX(UGeckoInstruction inst);
	void ps_res(UGeckoInstruction inst);
	void ps_sel(UGeckoInstruction inst);
	void ps_sumX(UGeckoInstruction inst);

	// Loadstore paired
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

private:
	struct SlowmemHandler
	{
		ARM64Reg dest_reg;
		ARM64Reg addr_reg;
		BitSet32 gprs;
		BitSet32 fprs;
		u32 flags;

		bool operator<(const SlowmemHandler& rhs) const
		{
			return std::tie(dest_reg, addr_reg, gprs, fprs, flags) <
			       std::tie(rhs.dest_reg, rhs.addr_reg, rhs.gprs, rhs.fprs, rhs.flags);
		}
	};

	struct FastmemArea
	{
		u32 length;
		const u8* slowmem_code;
	};

	// <Fastmem fault location, slowmem handler location>
	std::map<const u8*, FastmemArea> m_fault_to_handler;
	std::map<SlowmemHandler, const u8*> m_handler_to_loc;
	Arm64GPRCache gpr;
	Arm64FPRCache fpr;

	JitArm64BlockCache blocks;

	PPCAnalyst::CodeBuffer code_buffer;

	ARM64FloatEmitter m_float_emit;

	Arm64Gen::ARM64CodeBlock farcode;
	u8* nearcode; // Backed up when we switch to far code.

	// Do we support cycle counter profiling?
	bool m_supports_cycle_counter;

	void EmitResetCycleCounters();
	void EmitGetCycles(Arm64Gen::ARM64Reg reg);

	// Simple functions to switch between near and far code emitting
	void SwitchToFarCode()
	{
		nearcode = GetWritableCodePtr();
		SetCodePtrUnsafe(farcode.GetWritableCodePtr());
		AlignCode16();
	}

	void SwitchToNearCode()
	{
		farcode.SetCodePtrUnsafe(GetWritableCodePtr());
		SetCodePtrUnsafe(nearcode);
	}

	// Dump a memory range of code
	void DumpCode(const u8* start, const u8* end);

	// Backpatching routines
	bool DisasmLoadStore(const u8* ptr, u32* flags, Arm64Gen::ARM64Reg* reg);
	void EmitBackpatchRoutine(u32 flags, bool fastmem, bool do_farcode,
		Arm64Gen::ARM64Reg RS, Arm64Gen::ARM64Reg addr,
		BitSet32 gprs_to_push = BitSet32(0), BitSet32 fprs_to_push = BitSet32(0));
	// Loadstore routines
	void SafeLoadToReg(u32 dest, s32 addr, s32 offsetReg, u32 flags, s32 offset, bool update);
	void SafeStoreFromReg(s32 dest, u32 value, s32 regOffset, u32 flags, s32 offset);

	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b, u32 nextPC);

	void DoDownCount();
	void Cleanup();

	// AsmRoutines
	void GenerateAsm();
	void GenerateCommonAsm();
	void GenMfcr();

	// Profiling
	void BeginTimeProfile(JitBlock* b);
	void EndTimeProfile(JitBlock* b);

	// Exits
	void WriteExit(u32 destination);
	void WriteExit(Arm64Gen::ARM64Reg dest);
	void WriteExceptionExit(u32 destination, bool only_external = false);
	void WriteExceptionExit(Arm64Gen::ARM64Reg dest, bool only_external = false);

	FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set);

	void ComputeRC(Arm64Gen::ARM64Reg reg, int crf = 0, bool needs_sext = true);
	void ComputeRC(u64 imm, int crf = 0, bool needs_sext = true);
	void ComputeCarry(bool Carry);
	void ComputeCarry();

	typedef u32 (*Operation)(u32, u32);
	void reg_imm(u32 d, u32 a, u32 value, Operation do_op, void (ARM64XEmitter::*op)(Arm64Gen::ARM64Reg, Arm64Gen::ARM64Reg, Arm64Gen::ARM64Reg, ArithOption), bool Rc = false);
};

