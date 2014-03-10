// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

void JitArm::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

	switch (iIndex)
	{

	case SPR_DMAU:

	case SPR_SPRG0:
	case SPR_SPRG1:
	case SPR_SPRG2:
	case SPR_SPRG3:

	case SPR_SRR0:
	case SPR_SRR1:
		// These are safe to do the easy way, see the bottom of this function.
		break;

	case SPR_LR:
	case SPR_CTR:
	case SPR_XER:
	case SPR_GQR0:
	case SPR_GQR0 + 1:
	case SPR_GQR0 + 2:
	case SPR_GQR0 + 3:
	case SPR_GQR0 + 4:
	case SPR_GQR0 + 5:
	case SPR_GQR0 + 6:
	case SPR_GQR0 + 7:
		// These are safe to do the easy way, see the bottom of this function.
		break;

	default:
		FallBackToInterpreter(inst);
		return;
	}

	// OK, this is easy.
	ARMReg RD = gpr.R(inst.RD);
	STR(RD, R9,  PPCSTATE_OFF(spr) + iIndex * 4);
}
void JitArm::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	mfspr(inst);
}
void JitArm::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	switch (iIndex)
	{
	case SPR_WPAR:
	case SPR_DEC:
	case SPR_TL:
	case SPR_TU:
		FallBackToInterpreter(inst);
		return;
	default:
		ARMReg RD = gpr.R(inst.RD);
		LDR(RD, R9, PPCSTATE_OFF(spr) + iIndex * 4);
		break;
	}
}

void JitArm::mfcr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	// USES_CR
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	int d = inst.RD;
	LDRB(rA, R9, PPCSTATE_OFF(cr_fast[0]));

	for (int i = 1; i < 8; i++)
	{
		LDRB(rB, R9, PPCSTATE_OFF(cr_fast[i]));
		LSL(rA, rA, 4);
		ORR(rA, rA, rB);
	}
	MOV(gpr.R(d), rA);
	gpr.Unlock(rA, rB);
}

void JitArm::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	ARMReg rA = gpr.GetReg();

	// USES_CR
	u32 crm = inst.CRM;
	if (crm != 0)
	{
		if (gpr.IsImm(inst.RS))
		{
			for (int i = 0; i < 8; i++)
			{
				if ((crm & (0x80 >> i)) != 0)
				{
					u8 newcr = (gpr.GetImm(inst.RS) >> (28 - (i * 4))) & 0xF;
					MOV(rA, newcr);
					STRB(rA, R9, PPCSTATE_OFF(cr_fast[i]));
				}
			}
		}
		else
		{
			ARMReg rB = gpr.GetReg();
			MOV(rA, gpr.R(inst.RS));
			for (int i = 0; i < 8; i++)
			{
				if ((crm & (0x80 >> i)) != 0)
				{
					UBFX(rB, rA, 28 - (i * 4), 4);
					STRB(rB, R9, PPCSTATE_OFF(cr_fast[i]));
				}
			}
			gpr.Unlock(rB);
		}
	}
	gpr.Unlock(rA);
}

void JitArm::mtsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	STR(gpr.R(inst.RS), R9, PPCSTATE_OFF(sr[inst.SR]));
}

void JitArm::mfsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	LDR(gpr.R(inst.RD), R9, PPCSTATE_OFF(sr[inst.SR]));
}
void JitArm::mcrxr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	// Copy XER[0-3] into CR[inst.CRFD]
	LDR(rA, R9, PPCSTATE_OFF(spr[SPR_XER]));
	MOV(rB, rA);
	LSR(rA, rA, 28);
	STRB(rA, R9, PPCSTATE_OFF(cr_fast[inst.CRFD]));

	// Clear XER[0-3]
	Operand2 Top4(0xF, 2);
	BIC(rB, rB, Top4);
	STR(rB, R9, PPCSTATE_OFF(spr[SPR_XER]));
	gpr.Unlock(rA, rB);
}

void JitArm::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// Don't interpret this, if we do we get thrown out
	//JITDISABLE(bJITSystemRegistersOff)

	STR(gpr.R(inst.RS), R9, PPCSTATE_OFF(msr));

	gpr.Flush();
	fpr.Flush();

	WriteExit(js.compilerPC + 4);
}

void JitArm::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	LDR(gpr.R(inst.RD), R9, PPCSTATE_OFF(msr));
}

void JitArm::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	ARMReg rA = gpr.GetReg();

	if (inst.CRFS != inst.CRFD)
	{
		LDRB(rA, R9, PPCSTATE_OFF(cr_fast[inst.CRFS]));
		STRB(rA, R9, PPCSTATE_OFF(cr_fast[inst.CRFD]));
	}
	gpr.Unlock(rA);
}

void JitArm::crXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	// Get bit CRBA aligned with bit CRBD
	LDRB(rA, R9, PPCSTATE_OFF(cr_fast[inst.CRBA >> 2]));
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	if (shiftA < 0)
		LSL(rA, rA, -shiftA);
	else if (shiftA > 0)
		LSR(rA, rA, shiftA);

	// Get bit CRBB aligned with bit CRBD
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	LDRB(rB, R9, PPCSTATE_OFF(cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		LSL(rB, rB, -shiftB);
	else if (shiftB > 0)
		LSR(rB, rB, shiftB);

	// Compute combined bit
	switch (inst.SUBOP10)
	{
	case 33: // crnor
		ORR(rA, rA, rB);
		MVN(rA, rA);
		break;

	case 129: // crandc
		MVN(rB, rB);
		AND(rA, rA, rB);
		break;

	case 193: // crxor
		EOR(rA, rA, rB);
		break;

	case 225: // crnand
		AND(rA, rA, rB);
		MVN(rA, rA);
		break;

	case 257: // crand
		AND(rA, rA, rB);
		break;

	case 289: // creqv
		EOR(rA, rA, rB);
		MVN(rA, rA);
		break;

	case 417: // crorc
		MVN(rA, rA);
		ORR(rA, rA, rB);
		break;

	case 449: // cror
		ORR(rA, rA, rB);
		break;
	}
	// Store result bit in CRBD
	AND(rA, rA, 0x8 >> (inst.CRBD & 3));
	LDRB(rB, R9, PPCSTATE_OFF(cr_fast[inst.CRBD >> 2]));
	BIC(rB, rB, 0x8 >> (inst.CRBD & 3));
	ORR(rB, rB, rA);
	STRB(rB, R9, PPCSTATE_OFF(cr_fast[inst.CRBD >> 2]));
	gpr.Unlock(rA, rB);
}
