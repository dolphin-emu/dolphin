// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _JITARMIL_H
#define _JITARMIL_H

#include "../PPCAnalyst.h"
#include "ArmEmitter.h"
#include "../JitArm32/JitArmCache.h"
#include "../JitILCommon/JitILBase.h"
#include "../JitILCommon/IR.h"
#include "../JitCommon/JitBase.h"
#include "JitILAsm.h"

#define JITDISABLE(setting) \
	if (Core::g_CoreStartupParameter.bJITOff || \
		Core::g_CoreStartupParameter.setting) \
		{Default(inst); return;}

#define PPCSTATE_OFF(elem) ((s32)STRUCT_OFF(PowerPC::ppcState, elem) - (s32)STRUCT_OFF(PowerPC::ppcState, spr[0]))
class JitArmIL : public JitILBase, public ArmGen::ARMXCodeBlock
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

	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx) { return NULL; }

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
	void WriteCode();
	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInReg(ARMReg Reg);
	void WriteRfiExitDestInR(ARMReg Reg);
	void WriteExceptionExit();

	// OPCODES
	void unknown_instruction(UGeckoInstruction inst);
	void Default(UGeckoInstruction inst);
	void DoNothing(UGeckoInstruction inst);
	void HLEFunction(UGeckoInstruction inst);
	void Break(UGeckoInstruction inst);

	void DynaRunTable4(UGeckoInstruction inst);
	void DynaRunTable19(UGeckoInstruction inst);
	void DynaRunTable31(UGeckoInstruction inst);
	void DynaRunTable59(UGeckoInstruction inst);
	void DynaRunTable63(UGeckoInstruction inst);

	// Binary ops
	void BIN_AND(ARMReg reg, Operand2 op2);
	void BIN_XOR(ARMReg reg, Operand2 op2);
	void BIN_OR(ARMReg reg, Operand2 op2);
	void BIN_ADD(ARMReg reg, Operand2 op2);

	// Branches
	void bx(UGeckoInstruction inst);
	void bcx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction inst);
	void bcctrx(UGeckoInstruction inst);
};

#endif
