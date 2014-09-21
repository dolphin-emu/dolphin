// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

FixupBranch JitArm64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	FixupBranch branch;
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		LDR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[field]));
		branch = jump_if_set ? TBNZ(XA, 61) : TBZ(XA, 61);
	break;
	case CR_EQ_BIT:  // check bits 31-0 == 0
		LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(cr_val[field]));
		branch = jump_if_set ? CBZ(WA) : CBNZ(WA);
	break;
	case CR_GT_BIT:  // check val > 0
		LDR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[field]));
		CMP(XA, SP);
		branch = B(jump_if_set ? CC_GT : CC_LE);
	break;
	case CR_LT_BIT:  // check bit 62 set
		LDR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[field]));
		branch = jump_if_set ? TBNZ(XA, 62) : TBZ(XA, 62);
	break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}

	gpr.Unlock(WA);
	return branch;
}

void JitArm64::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// Don't interpret this, if we do we get thrown out
	//JITDISABLE(bJITSystemRegistersOff)

	STR(INDEX_UNSIGNED, gpr.R(inst.RS), X29, PPCSTATE_OFF(msr));

	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	WriteExit(js.compilerPC + 4);
}

void JitArm64::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	if (inst.CRFS != inst.CRFD)
	{
		ARM64Reg WA = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);
		LDR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[inst.CRFS]));
		STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[inst.CRFD]));
		gpr.Unlock(WA);
	}
}

void JitArm64::mfsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	LDR(INDEX_UNSIGNED, gpr.R(inst.RD), X29, PPCSTATE_OFF(sr[inst.SR]));
}

void JitArm64::mtsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	STR(INDEX_UNSIGNED, gpr.R(inst.RS), X29, PPCSTATE_OFF(sr[inst.SR]));
}

void JitArm64::mfsrin(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	ARM64Reg index = gpr.GetReg();
	ARM64Reg index64 = EncodeRegTo64(index);
	ARM64Reg RB = gpr.R(inst.RB);

	UBFM(index, RB, 28, 31);
	ADD(index64, X29, index64, ArithOption(index64, ST_LSL, 2));
	LDR(INDEX_UNSIGNED, gpr.R(inst.RD), index64, PPCSTATE_OFF(sr[0]));

	gpr.Unlock(index);
}

void JitArm64::mtsrin(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	ARM64Reg index = gpr.GetReg();
	ARM64Reg index64 = EncodeRegTo64(index);
	ARM64Reg RB = gpr.R(inst.RB);

	UBFM(index, RB, 28, 31);
	ADD(index64, X29, index64, ArithOption(index64, ST_LSL, 2));
	STR(INDEX_UNSIGNED, gpr.R(inst.RD), index64, PPCSTATE_OFF(sr[0]));

	gpr.Unlock(index);
}
