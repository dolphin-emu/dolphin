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

FixupBranch JitArm::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
	ARMReg RA = gpr.GetReg();

	Operand2 SOBit(2, 2); // 0x10000000
	Operand2 LTBit(1, 1); // 0x80000000

	FixupBranch branch;
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		LDR(RA, R9, PPCSTATE_OFF(cr_val[field]) + sizeof(u32));
		TST(RA, SOBit);
		branch = B_CC(jump_if_set ? CC_NEQ : CC_EQ);
	break;
	case CR_EQ_BIT:  // check bits 31-0 == 0
		LDR(RA, R9, PPCSTATE_OFF(cr_val[field]));
		CMP(RA, 0);
		branch = B_CC(jump_if_set ? CC_EQ : CC_NEQ);
	break;
	case CR_GT_BIT:  // check val > 0
		LDR(RA, R9, PPCSTATE_OFF(cr_val[field]));
		CMP(RA, 1);
		LDR(RA, R9, PPCSTATE_OFF(cr_val[field]) + sizeof(u32));
		SBCS(RA, RA, 0);
		branch = B_CC(jump_if_set ? CC_GE : CC_LT);
	break;
	case CR_LT_BIT:  // check bit 62 set
		LDR(RA, R9, PPCSTATE_OFF(cr_val[field]) + sizeof(u32));
		TST(RA, LTBit);
		branch = B_CC(jump_if_set ? CC_NEQ : CC_EQ);
	break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}

	gpr.Unlock(RA);
	return branch;
}

void JitArm::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

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
		FALLBACK_IF(true);
	}

	// OK, this is easy.
	ARMReg RD = gpr.R(inst.RD);
	STR(RD, R9,  PPCSTATE_OFF(spr) + iIndex * 4);
}
void JitArm::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	mfspr(inst);
}
void JitArm::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	switch (iIndex)
	{
	case SPR_WPAR:
	case SPR_DEC:
	case SPR_TL:
	case SPR_TU:
		FALLBACK_IF(true);
	default:
		ARMReg RD = gpr.R(inst.RD);
		LDR(RD, R9, PPCSTATE_OFF(spr) + iIndex * 4);
		break;
	}
}

void JitArm::mtsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	STR(gpr.R(inst.RS), R9, PPCSTATE_OFF(sr[inst.SR]));
}

void JitArm::mfsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	LDR(gpr.R(inst.RD), R9, PPCSTATE_OFF(sr[inst.SR]));
}

void JitArm::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// Don't interpret this, if we do we get thrown out
	//JITDISABLE(bJITSystemRegistersOff);

	STR(gpr.R(inst.RS), R9, PPCSTATE_OFF(msr));

	gpr.Flush();
	fpr.Flush();

	WriteExit(js.compilerPC + 4);
}

void JitArm::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	LDR(gpr.R(inst.RD), R9, PPCSTATE_OFF(msr));
}

void JitArm::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	ARMReg rA = gpr.GetReg();

	if (inst.CRFS != inst.CRFD)
	{
		LDR(rA, R9, PPCSTATE_OFF(cr_val[inst.CRFS]));
		STR(rA, R9, PPCSTATE_OFF(cr_val[inst.CRFD]));
		LDR(rA, R9, PPCSTATE_OFF(cr_val[inst.CRFS]) + sizeof(u32));
		STR(rA, R9, PPCSTATE_OFF(cr_val[inst.CRFD]) + sizeof(u32));
	}
	gpr.Unlock(rA);
}

