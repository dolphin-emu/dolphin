// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Arm64Emitter.h"

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#define PPCSTATE_OFF(elem) ((s64)&PowerPC::ppcState.elem - (s64)&PowerPC::ppcState)

// Some asserts to make sure we will be able to load everything
static_assert(PPCSTATE_OFF(spr[1023]) <= 16380, "LDR(32bit) can't reach the last SPR");
static_assert((PPCSTATE_OFF(ps[0][0]) % 8) == 0, "LDR(64bit VFP) requires FPRs to be 8 byte aligned");

using namespace Arm64Gen;
class JitArm64 : public JitBase, public Arm64Gen::ARM64CodeBlock
{
public:
	JitArm64() : code_buffer(32000) {}
	~JitArm64() {}

	void Init();
	void Shutdown();

	JitBaseBlockCache *GetBlockCache() { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) { return NULL; }

	bool IsInCodeSpace(u8 *ptr) { return IsInSpace(ptr); }

	bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }

	void ClearCache();

	CommonAsmRoutinesBase *GetAsmRoutines()
	{
		return &asm_routines;
	}

	void Run();
	void SingleStep();

	void Jit(u32 em_address);

	const char *GetName()
	{
		return "JITARM64";
	}

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
	void extsXx(UGeckoInstruction inst);
	void cntlzwx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);

	// System Registers
	void mtmsr(UGeckoInstruction inst);
	void mcrf(UGeckoInstruction inst);
	void mfsr(UGeckoInstruction inst);
	void mtsr(UGeckoInstruction inst);

	// LoadStore
	void icbi(UGeckoInstruction inst);

private:
	Arm64GPRCache gpr;
	Arm64FPRCache fpr;

	JitArm64BlockCache blocks;
	JitArm64AsmRoutineManager asm_routines;

	PPCAnalyst::CodeBuffer code_buffer;

	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b);

	void DoDownCount();

	// Exits
	void WriteExit(u32 destination);
	void WriteExceptionExit(ARM64Reg dest);
	void WriteExitDestInR(ARM64Reg dest);

	FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set);

	void ComputeRC(u32 d);

	typedef u32 (*Operation)(u32, u32);
	void reg_imm(u32 d, u32 a, bool binary, u32 value, Operation do_op, void (ARM64XEmitter::*op)(ARM64Reg, ARM64Reg, ARM64Reg, ArithOption), bool Rc = false);
};

