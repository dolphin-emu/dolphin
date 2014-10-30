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

// The branches are known good, or at least reasonably good.
// No need for a disable-mechanism.

using namespace ArmGen;
void JitArm::sc(UGeckoInstruction inst)
{
	INSTRUCTION_START

	gpr.Flush();
	fpr.Flush();

	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, js.compilerPC + 4);
	STR(rA, R9, PPCSTATE_OFF(pc));
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	ORR(rA, rA, EXCEPTION_SYSCALL);
	STR(rA, R9, PPCSTATE_OFF(Exceptions));
	gpr.Unlock(rA);

	WriteExceptionExit();
}

void JitArm::rfi(UGeckoInstruction inst)
{
	INSTRUCTION_START

	gpr.Flush();
	fpr.Flush();

	// See Interpreter rfi for details
	const u32 mask = 0x87C0FFFF;
	const u32 clearMSR13 = 0xFFFBFFFF; // Mask used to clear the bit MSR[13]
	// MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;
	// R0 = MSR location
	// R1 = MSR contents
	// R2 = Mask
	// R3 = Mask
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg rC = gpr.GetReg();
	ARMReg rD = gpr.GetReg();
	MOVI2R(rB, (~mask) & clearMSR13);
	MOVI2R(rC, mask & clearMSR13);

	LDR(rD, R9, PPCSTATE_OFF(msr));

	AND(rD, rD, rB); // rD = Masked MSR

	LDR(rB, R9, PPCSTATE_OFF(spr[SPR_SRR1])); // rB contains SRR1 here

	AND(rB, rB, rC); // rB contains masked SRR1 here
	ORR(rB, rD, rB); // rB = Masked MSR OR masked SRR1

	STR(rB, R9, PPCSTATE_OFF(msr)); // STR rB in to rA

	LDR(rA, R9, PPCSTATE_OFF(spr[SPR_SRR0]));

	gpr.Unlock(rB, rC, rD);
	WriteRfiExitDestInR(rA); // rA gets unlocked here
	//AND(32, M(&MSR), Imm32((~mask) & clearMSR13));
	//MOV(32, R(EAX), M(&SRR1));
	//AND(32, R(EAX), Imm32(mask & clearMSR13));
	//OR(32, M(&MSR), R(EAX));
	// NPC = SRR0;
	//MOV(32, R(EAX), M(&SRR0));
	//WriteRfiExitDestInEAX();
}

void JitArm::bx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// We must always process the following sentence
	// even if the blocks are merged by PPCAnalyst::Flatten().
	if (inst.LK)
	{
		ARMReg rA = gpr.GetReg(false);
		u32 Jumpto = js.compilerPC + 4;
		MOVI2R(rA, Jumpto);
		STR(rA, R9, PPCSTATE_OFF(spr[SPR_LR]));
		//ARMABI_MOVI2M((u32)&LR, js.compilerPC + 4);
	}

	// If this is not the last instruction of a block,
	// we will skip the rest process.
	// Because PPCAnalyst::Flatten() merged the blocks.
	if (!js.isLastInstruction)
	{
		return;
	}

	gpr.Flush();
	fpr.Flush();

	u32 destination;
	if (inst.AA)
		destination = SignExt26(inst.LI << 2);
	else
		destination = js.compilerPC + SignExt26(inst.LI << 2);

	if (destination == js.compilerPC)
	{
		//PanicAlert("Idle loop detected at %08x", destination);
		// CALL(ProtectFunction(&CoreTiming::Idle, 0));
		// JMP(Asm::testExceptions, true);
		// make idle loops go faster
		MOVI2R(R14, (u32)&CoreTiming::Idle);
		BL(R14);
		MOVI2R(R14, js.compilerPC);
		STR(R14, R9, PPCSTATE_OFF(pc));
		WriteExceptionExit();
	}
	WriteExit(destination);
}

void JitArm::bcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// USES_CR

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	FixupBranch pCTRDontBranch;
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LDR(rB, R9, PPCSTATE_OFF(spr[SPR_CTR]));
		SUBS(rB, rB, 1);
		STR(rB, R9, PPCSTATE_OFF(spr[SPR_CTR]));

		//SUB(32, M(&CTR), Imm8(1));
		if (inst.BO & BO_BRANCH_IF_CTR_0)
			pCTRDontBranch = B_CC(CC_NEQ);
		else
			pCTRDontBranch = B_CC(CC_EQ);
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
		MOVI2R(rB, Jumpto);
		STR(rB, R9, PPCSTATE_OFF(spr[SPR_LR]));
		//ARMABI_MOVI2M((u32)&LR, js.compilerPC + 4); // Careful, destroys R14, R12
	}
	gpr.Unlock(rA, rB);

	u32 destination;
	if (inst.AA)
		destination = SignExt16(inst.BD << 2);
	else
		destination = js.compilerPC + SignExt16(inst.BD << 2);

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);
	WriteExit(destination);

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
		SetJumpTarget( pConditionDontBranch );
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		SetJumpTarget( pCTRDontBranch );

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(js.compilerPC + 4);
	}
}
void JitArm::bcctrx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	// bcctrx doesn't decrement and/or test CTR
	_dbg_assert_msg_(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG, "bcctrx with decrement and test CTR option is invalid!");

	if (inst.BO_2 & BO_DONT_CHECK_CONDITION)
	{
		// BO_2 == 1z1zz -> b always

		//NPC = CTR & 0xfffffffc;
		gpr.Flush();
		fpr.Flush();

		ARMReg rA = gpr.GetReg();

		if (inst.LK_3)
		{
			u32 Jumpto = js.compilerPC + 4;
			MOVI2R(rA, Jumpto);
			STR(rA, R9, PPCSTATE_OFF(spr[SPR_LR]));
			// ARMABI_MOVI2M((u32)&LR, js.compilerPC + 4);
		}
		LDR(rA, R9, PPCSTATE_OFF(spr[SPR_CTR]));
		BIC(rA, rA, 0x3);
		WriteExitDestInR(rA);
	}
	else
	{
		// Rare condition seen in (just some versions of?) Nintendo's NES Emulator
		// BO_2 == 001zy -> b if false
		// BO_2 == 011zy -> b if true
		ARMReg rA = gpr.GetReg();
		ARMReg rB = gpr.GetReg();

		FixupBranch b = JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3),
		                                 !(inst.BO_2 & BO_BRANCH_IF_TRUE));

		LDR(rA, R9, PPCSTATE_OFF(spr[SPR_CTR]));
		BIC(rA, rA, 0x3);

		if (inst.LK_3)
		{
			u32 Jumpto = js.compilerPC + 4;
			MOVI2R(rB, Jumpto);
			STR(rB, R9, PPCSTATE_OFF(spr[SPR_LR]));
			//ARMABI_MOVI2M((u32)&LR, js.compilerPC + 4);
		}
		gpr.Unlock(rB); // rA gets unlocked in WriteExitDestInR
		gpr.Flush(FLUSH_MAINTAIN_STATE);
		fpr.Flush(FLUSH_MAINTAIN_STATE);

		WriteExitDestInR(rA);

		SetJumpTarget(b);

		if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
		{
			gpr.Flush();
			fpr.Flush();
			WriteExit(js.compilerPC + 4);
		}
	}
}
void JitArm::bclrx(UGeckoInstruction inst)
{
	INSTRUCTION_START

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	FixupBranch pCTRDontBranch;
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LDR(rB, R9, PPCSTATE_OFF(spr[SPR_CTR]));
		SUBS(rB, rB, 1);
		STR(rB, R9, PPCSTATE_OFF(spr[SPR_CTR]));

		//SUB(32, M(&CTR), Imm8(1));
		if (inst.BO & BO_BRANCH_IF_CTR_0)
			pCTRDontBranch = B_CC(CC_NEQ);
		else
			pCTRDontBranch = B_CC(CC_EQ);
	}

	FixupBranch pConditionDontBranch;
	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		pConditionDontBranch = JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3),
		                                        !(inst.BO_2 & BO_BRANCH_IF_TRUE));
	}

	//MOV(32, R(EAX), M(&LR));
	//AND(32, R(EAX), Imm32(0xFFFFFFFC));
	LDR(rA, R9, PPCSTATE_OFF(spr[SPR_LR]));
	BIC(rA, rA, 0x3);
	if (inst.LK)
	{
		u32 Jumpto = js.compilerPC + 4;
		MOVI2R(rB, Jumpto);
		STR(rB, R9, PPCSTATE_OFF(spr[SPR_LR]));
		//ARMABI_MOVI2M((u32)&LR, js.compilerPC + 4);
	}
	gpr.Unlock(rB); // rA gets unlocked in WriteExitDestInR

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);
	WriteExitDestInR(rA);

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
		SetJumpTarget( pConditionDontBranch );
	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		SetJumpTarget( pCTRDontBranch );

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(js.compilerPC + 4);
	}
}
