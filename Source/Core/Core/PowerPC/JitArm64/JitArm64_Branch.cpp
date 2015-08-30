// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::sc(UGeckoInstruction inst)
{
	INSTRUCTION_START

	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	ARM64Reg WA = gpr.GetReg();

	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(Exceptions));
	ORR(WA, WA, 31, 0); // Same as WA | EXCEPTION_SYSCALL
	STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(Exceptions));

	MOVI2R(WA, js.compilerPC + 4);
	STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));

	// WA is unlocked in this function
	WriteExceptionExit(WA);
}

void JitArm64::rfi(UGeckoInstruction inst)
{
	INSTRUCTION_START

	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	// See Interpreter rfi for details
	const u32 mask = 0x87C0FFFF;
	const u32 clearMSR13 = 0xFFFBFFFF; // Mask used to clear the bit MSR[13]
	// MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;
	// R0 = MSR location
	// R1 = MSR contents
	// R2 = Mask
	// R3 = Mask
	ARM64Reg WA = gpr.GetReg();
	ARM64Reg WB = gpr.GetReg();
	ARM64Reg WC = gpr.GetReg();

	MOVI2R(WA, (~mask) & clearMSR13);
	MOVI2R(WB, mask & clearMSR13);

	LDR(INDEX_UNSIGNED, WC, X29, PPCSTATE_OFF(msr));

	AND(WC, WC, WB, ArithOption(WC, ST_LSL, 0)); // rD = Masked MSR

	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_SRR1])); // rB contains SRR1 here

	AND(WA, WA, WB, ArithOption(WA, ST_LSL, 0)); // rB contains masked SRR1 here
	ORR(WA, WA, WC, ArithOption(WA, ST_LSL, 0)); // rB = Masked MSR OR masked SRR1

	STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(msr)); // STR rB in to rA

	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_SRR0]));
	gpr.Unlock(WB, WC);

	// WA is unlocked in this function
	WriteExceptionExit(WA);
}

void JitArm64::bx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	u32 destination;
	if (inst.AA)
		destination = SignExt26(inst.LI << 2);
	else
		destination = js.compilerPC + SignExt26(inst.LI << 2);

	if (inst.LK)
	{
		u32 Jumpto = js.compilerPC + 4;
		ARM64Reg WA = gpr.GetReg();
		MOVI2R(WA, Jumpto);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_LR]));
		gpr.Unlock(WA);
	}

	if (destination == js.compilerPC)
	{
		// make idle loops go faster
		ARM64Reg WA = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);

		MOVI2R(XA, (u64)&CoreTiming::Idle);
		BLR(XA);
		MOVI2R(WA, js.compilerPC);
		WriteExceptionExit(WA);
	}

	WriteExit(destination);
}

void JitArm64::bcx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	ARM64Reg WA = gpr.GetReg();
	FixupBranch pCTRDontBranch;
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_CTR]));
		SUBS(WA, WA, 1);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_CTR]));

		if (inst.BO & BO_BRANCH_IF_CTR_0)
			pCTRDontBranch = B(CC_NEQ);
		else
			pCTRDontBranch = B(CC_EQ);
	}

	FixupBranch pConditionDontBranch;

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		pConditionDontBranch = JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3),
		                                        !(inst.BO_2 & BO_BRANCH_IF_TRUE));
	}

	if (inst.LK)
	{
		u32 Jumpto = js.compilerPC + 4;
		MOVI2R(WA, Jumpto);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_LR]));
	}
	gpr.Unlock(WA);

	u32 destination;
	if (inst.AA)
		destination = SignExt16(inst.BD << 2);
	else
		destination = js.compilerPC + SignExt16(inst.BD << 2);

	gpr.Flush(FlushMode::FLUSH_MAINTAIN_STATE);
	fpr.Flush(FlushMode::FLUSH_MAINTAIN_STATE);

	WriteExit(destination);

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
		SetJumpTarget( pConditionDontBranch );
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		SetJumpTarget( pCTRDontBranch );

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush(FlushMode::FLUSH_ALL);
		fpr.Flush(FlushMode::FLUSH_ALL);
		WriteExit(js.compilerPC + 4);
	}
}

void JitArm64::bcctrx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	// bcctrx doesn't decrement and/or test CTR
	_assert_msg_(DYNA_REC, inst.BO_2 & BO_DONT_DECREMENT_FLAG, "bcctrx with decrement and test CTR option is invalid!");

	if (inst.BO_2 & BO_DONT_CHECK_CONDITION)
	{
		// BO_2 == 1z1zz -> b always

		//NPC = CTR & 0xfffffffc;
		gpr.Flush(FlushMode::FLUSH_ALL);
		fpr.Flush(FlushMode::FLUSH_ALL);

		if (inst.LK_3)
		{
			ARM64Reg WB = gpr.GetReg();
			u32 Jumpto = js.compilerPC + 4;
			MOVI2R(WB, Jumpto);
			STR(INDEX_UNSIGNED, WB, X29, PPCSTATE_OFF(spr[SPR_LR]));
			gpr.Unlock(WB);
		}

		ARM64Reg WA = gpr.GetReg();

		LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_CTR]));
		AND(WA, WA, 30, 29); // Wipe the bottom 2 bits.
		WriteExitDestInR(WA);
	}
	else
	{
		// Rare condition seen in (just some versions of?) Nintendo's NES Emulator
		// BO_2 == 001zy -> b if false
		// BO_2 == 011zy -> b if true
		_assert_msg_(DYNA_REC, false, "Haven't implemented rare form of bcctrx yet");
	}
}

void JitArm64::bclrx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	ARM64Reg WA = gpr.GetReg();
	FixupBranch pCTRDontBranch;
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_CTR]));
		SUBS(WA, WA, 1);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_CTR]));

		if (inst.BO & BO_BRANCH_IF_CTR_0)
			pCTRDontBranch = B(CC_NEQ);
		else
			pCTRDontBranch = B(CC_EQ);
	}

	FixupBranch pConditionDontBranch;
	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		pConditionDontBranch = JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3),
		                                        !(inst.BO_2 & BO_BRANCH_IF_TRUE));
	}

	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(spr[SPR_LR]));
	AND(WA, WA, 30, 29); // Wipe the bottom 2 bits.

	if (inst.LK)
	{
		ARM64Reg WB = gpr.GetReg();
		u32 Jumpto = js.compilerPC + 4;
		MOVI2R(WB, Jumpto);
		STR(INDEX_UNSIGNED, WB, X29, PPCSTATE_OFF(spr[SPR_LR]));
		gpr.Unlock(WB);
	}

	gpr.Flush(FlushMode::FLUSH_MAINTAIN_STATE);
	fpr.Flush(FlushMode::FLUSH_MAINTAIN_STATE);

	WriteExitDestInR(WA);

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
		SetJumpTarget( pConditionDontBranch );
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		SetJumpTarget( pCTRDontBranch );

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush(FlushMode::FLUSH_ALL);
		fpr.Flush(FlushMode::FLUSH_ALL);
		WriteExit(js.compilerPC + 4);
	}
}
