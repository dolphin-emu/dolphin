// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

using namespace ArmGen;

void JitArm::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	// R12 contains scale
	// R11 contains type
	// R10 is the ADDR
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

	bool update = inst.OPCD == 57;
	s32 offset = inst.SIMM_12;

	LDR(R11, R9, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
	UBFX(R12, R11, 16, 3); // Type
	LSL(R12, R12, 2);
	UBFX(R11, R11, 24, 6); // Scale
	LSL(R11, R11, 3);

	Operand2 off;
	if (TryMakeOperand2(offset, off))
	{
		if (inst.RA || update)
			ADD(R10, gpr.R(inst.RA), off);
		else
			MOV(R10, off);
	}
	else
	{
		MOVI2R(R10, (u32)offset);
		if (inst.RA || update) // Always uses the register on update
			ADD(R10, R10, gpr.R(inst.RA));
	}

	if (update)
		MOV(gpr.R(inst.RA), R10);
	MOVI2R(R14, (u32)asm_routines.pairedLoadQuantized);
	ADD(R14, R14, R12);
	LDR(R14, R14, inst.W ? 8 * 4 : 0);

	// Values returned in S0, S1
	BL(R14); // Jump to the quantizer Load

	ARMReg vD0 = fpr.R0(inst.RS, false);
	ARMReg vD1 = fpr.R1(inst.RS, false);
	VCVT(vD0, S0, 0);
	if (!inst.W)
		VCVT(vD1, S1, 0);
	else
		MOVI2F(vD1, 1.0f, INVALID_REG); // No need for temp reg with 1.0f
}

void JitArm::psq_lx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	// R12 contains scale
	// R11 contains type
	// R10 is the ADDR
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

	bool update = inst.SUBOP10 == 38;

	LDR(R11, R9, PPCSTATE_OFF(spr[SPR_GQR0 + inst.Ix]));
	UBFX(R12, R11, 16, 3); // Type
	LSL(R12, R12, 2);
	UBFX(R11, R11, 24, 6); // Scale
	LSL(R11, R11, 3);

	if (inst.RA || update) // Always uses the register on update
	{
		ADD(R10, gpr.R(inst.RB), gpr.R(inst.RA));
	}
	else
	{
		MOV(R10, gpr.R(inst.RB));
	}

	if (update)
		MOV(gpr.R(inst.RA), R10);

	MOVI2R(R14, (u32)asm_routines.pairedLoadQuantized);
	ADD(R14, R14, R12);
	LDR(R14, R14, inst.Wx ? 8 * 4 : 0);

	// Values returned in S0, S1
	BL(R14); // Jump to the quantizer Load

	ARMReg vD0 = fpr.R0(inst.RS, false);
	ARMReg vD1 = fpr.R1(inst.RS, false);
	LDR(R14, R9, PPCSTATE_OFF(Exceptions));
	CMP(R14, EXCEPTION_DSI);
	SetCC(CC_NEQ);

	VCVT(vD0, S0, 0);
	if (!inst.Wx)
		VCVT(vD1, S1, 0);
	else
		MOVI2F(vD1, 1.0f, INVALID_REG); // No need for temp reg with 1.0f
	SetCC();
}

void JitArm::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	// R12 contains scale
	// R11 contains type
	// R10 is the ADDR
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

	bool update = inst.OPCD == 61;
	s32 offset = inst.SIMM_12;

	LDR(R11, R9, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
	UBFX(R12, R11, 0, 3); // Type
	LSL(R12, R12, 2);
	UBFX(R11, R11, 8, 6); // Scale
	LSL(R11, R11, 3);

	Operand2 off;
	if (TryMakeOperand2(offset, off))
	{
		if (inst.RA || update)
			ADD(R10, gpr.R(inst.RA), off);
		else
			MOV(R10, off);
	}
	else
	{
		MOVI2R(R10, (u32)offset);
		if (inst.RA || update) // Always uses the register on update
			ADD(R10, R10, gpr.R(inst.RA));
	}

	if (update)
		MOV(gpr.R(inst.RA), R10);
	MOVI2R(R14, (u32)asm_routines.pairedStoreQuantized);
	ADD(R14, R14, R12);
	LDR(R14, R14, inst.W ? 8 * 4 : 0);

	ARMReg vD0 = fpr.R0(inst.RS);
	VCVT(S0, vD0, 0);

	if (!inst.W)
	{
		ARMReg vD1 = fpr.R1(inst.RS);
		VCVT(S1, vD1, 0);
	}
	// floats passed through D0
	BL(R14); // Jump to the quantizer Store
}

void JitArm::psq_stx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	// R12 contains scale
	// R11 contains type
	// R10 is the ADDR
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

	bool update = inst.SUBOP10 == 39;

	LDR(R11, R9, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
	UBFX(R12, R11, 0, 3); // Type
	LSL(R12, R12, 2);
	UBFX(R11, R11, 8, 6); // Scale
	LSL(R11, R11, 3);

	if (inst.RA || update) // Always uses the register on update
	{
		ADD(R10, gpr.R(inst.RA), gpr.R(inst.RB));
	}
	else
	{
		MOV(R10, gpr.R(inst.RB));
	}

	if (update)
		MOV(gpr.R(inst.RA), R10);

	MOVI2R(R14, (u32)asm_routines.pairedStoreQuantized);
	ADD(R14, R14, R12);
	LDR(R14, R14, inst.W ? 8 * 4 : 0);

	ARMReg vD0 = fpr.R0(inst.RS);
	VCVT(S0, vD0, 0);

	if (!inst.W)
	{
		ARMReg vD1 = fpr.R1(inst.RS);
		VCVT(S1, vD1, 0);
	}
	// floats passed through D0
	BL(R14); // Jump to the quantizer Store
}
