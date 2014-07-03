// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ArmEmitter.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm32/JitArmCache.h"
#include "Core/PowerPC/JitArmIL/JitILAsm.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitILCommon/IR.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

#define PPCSTATE_OFF(elem) ((s32)STRUCT_OFF(PowerPC::ppcState, elem) - (s32)STRUCT_OFF(PowerPC::ppcState, spr[0]))
class JitArmIL : public JitILBase, public ArmGen::ARMCodeBlock
{
private:
	JitArmBlockCache blocks;
	JitArmILAsmRoutineManager asm_routines;

	void PrintDebug(UGeckoInstruction inst, u32 level);
	void DoDownCount();

public:
	// Initialization, etc
	JitArmIL() {}
	~JitArmIL() {}

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	JitBaseBlockCache *GetBlockCache() { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) { return nullptr; }

	bool IsInCodeSpace(u8 *ptr) { return IsInSpace(ptr); }

	void ClearCache();
	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;  // asm_routines.dispatcher
	}
	const CommonAsmRoutinesBase *GetAsmRoutines() {
		return &asm_routines;
	}

	const char *GetName() {
		return "JITARMIL";
	}

	// Run!

	void Run();
	void SingleStep();
	//
	void WriteCode(u32 exitAddress);
	void WriteExit(u32 destination);
	void WriteExitDestInReg(ArmGen::ARMReg Reg);
	void WriteRfiExitDestInR(ArmGen::ARMReg Reg);
	void WriteExceptionExit();

	// OPCODES
	void unknown_instruction(UGeckoInstruction inst);
	void FallBackToInterpreter(UGeckoInstruction inst);
	void DoNothing(UGeckoInstruction inst);
	void HLEFunction(UGeckoInstruction inst);
	void Break(UGeckoInstruction inst);

	void DynaRunTable4(UGeckoInstruction inst);
	void DynaRunTable19(UGeckoInstruction inst);
	void DynaRunTable31(UGeckoInstruction inst);
	void DynaRunTable59(UGeckoInstruction inst);
	void DynaRunTable63(UGeckoInstruction inst);

	// Binary ops
	void BIN_AND(ArmGen::ARMReg reg, ArmGen::Operand2 op2);
	void BIN_XOR(ArmGen::ARMReg reg, ArmGen::Operand2 op2);
	void BIN_OR(ArmGen::ARMReg reg, ArmGen::Operand2 op2);
	void BIN_ADD(ArmGen::ARMReg reg, ArmGen::Operand2 op2);

	// Branches
	void bx(UGeckoInstruction inst);
	void bcx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction inst);
	void bcctrx(UGeckoInstruction inst);
};
