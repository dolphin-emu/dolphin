// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#include "Common.h"
#include "Thunk.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitAsm.h"

// The branches are known good, or at least reasonably good.
// No need for a disable-mechanism.

// If defined, clears CR0 at blr and bl-s. If the assumption that
// flags never carry over between functions holds, then the task for 
// an optimizer becomes much easier.

// #define ACID_TEST

// Zelda and many more games seem to pass the Acid Test. 

using namespace Gen;

void Jit64::sc(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	WriteExceptionExit(EXCEPTION_SYSCALL);
}

void Jit64::rfi(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	// See Interpreter rfi for details
	const u32 mask = 0x87C0FFFF;
	// MSR = (MSR & ~mask) | (SRR1 & mask);
	MOV(32, R(EAX), M(&MSR));
	MOV(32, R(ECX), M(&SRR1));
	AND(32, R(EAX), Imm32(~mask));
	AND(32, R(ECX), Imm32(mask));
	OR(32, R(EAX), R(ECX));       
	// MSR &= 0xFFFDFFFF; //TODO: VERIFY
	AND(32, R(EAX), Imm32(0xFFFDFFFF));
	MOV(32, M(&MSR), R(EAX));
	// NPC = SRR0; 
	MOV(32, R(EAX), M(&SRR0));
	WriteRfiExitDestInEAX();
}

void Jit64::bx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	if (inst.LK)
		MOV(32, M(&LR), Imm32(js.compilerPC + 4));
	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);

	if (js.isLastInstruction)
	{
		u32 destination;
		if (inst.AA)
			destination = SignExt26(inst.LI << 2);
		else
			destination = js.compilerPC + SignExt26(inst.LI << 2);
#ifdef ACID_TEST
		if (inst.LK)
			AND(32, M(&CR), Imm32(~(0xFF000000)));
#endif
		if (destination == js.compilerPC)
		{
			//PanicAlert("Idle loop detected at %08x", destination);
		//	CALL(ProtectFunction(&CoreTiming::Idle, 0));
		//	JMP(Asm::testExceptions, true);
			// make idle loops go faster
			js.downcountAmount += 8;
		}
		WriteExit(destination, 0);
	}
	else {
		// TODO: investigate the good old method of merging blocks here.
		PanicAlert("bx not last instruction of block"); // this should not happen
	}
}

// TODO - optimize to hell and beyond
// TODO - make nice easy to optimize special cases for the most common
// variants of this instruction.
void Jit64::bcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	// USES_CR
	_assert_msg_(DYNA_REC, js.isLastInstruction, "bcx not last instruction of block");

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);

	CCFlags branch = CC_Z;

	//const bool only_counter_check = (inst.BO & 16) ? true : false;
	//const bool only_condition_check = (inst.BO & 4) ? true : false;
	//if (only_condition_check && only_counter_check)
	//	PanicAlert("Bizarre bcx encountered. Likely bad or corrupt code.");
	bool doFullTest = ((inst.BO & BO_DONT_CHECK_CONDITION) == 0) && ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0);
	bool ctrDecremented = false;

	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		TEST(8, M(&PowerPC::ppcState.cr_fast[inst.BI >> 2]), Imm8(8 >> (inst.BI & 3)));
		if (inst.BO & BO_BRANCH_IF_TRUE)  // Conditional branch 
			branch = CC_NZ;
		else
			branch = CC_Z; 
		
		if (doFullTest)
			SETcc(branch, R(EAX));
	}
	else
	{
		if (doFullTest)
			MOV(32, R(EAX), Imm32(1));
	}

	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		// Decrement CTR 
		SUB(32, M(&CTR), Imm8(1));
		ctrDecremented = true;
		// Test whether to branch if CTR is zero or not 
		if (inst.BO & BO_BRANCH_IF_CTR_0)
			branch = CC_Z;
		else
			branch = CC_NZ;
		
		if (doFullTest)
			SETcc(branch, R(ECX));
	}
	else
	{
		if (doFullTest)
			MOV(32, R(ECX), Imm32(1));
	}

	if (doFullTest)
	{
		TEST(32, R(EAX), R(ECX));
		branch = CC_Z;
	}
	else
	{
		if (branch == CC_Z)
			branch = CC_NZ;
		else
			branch = CC_Z;
	}

	if (!ctrDecremented && (inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
	{
		SUB(32, M(&CTR), Imm8(1));
	}
	FixupBranch skip;
	if (inst.BO != 20)
	{
		skip = J_CC(branch);
	}
	u32 destination;
	if (inst.LK)
		MOV(32, M(&LR), Imm32(js.compilerPC + 4));
	if(inst.AA)
		destination = SignExt16(inst.BD << 2);
	else
		destination = js.compilerPC + SignExt16(inst.BD << 2);
	WriteExit(destination, 0);
	if (inst.BO != 20)
	{
		SetJumpTarget(skip);
		WriteExit(js.compilerPC + 4, 1);
	}
}

void Jit64::bcctrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);

	// bcctrx doesn't decrement and/or test CTR
	_dbg_assert_msg_(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG, "bcctrx with decrement and test CTR option is invalid!");

	if (inst.BO_2 & BO_DONT_CHECK_CONDITION)
	{
		// BO_2 == 1z1zz -> b always

		//NPC = CTR & 0xfffffffc;
		MOV(32, R(EAX), M(&CTR));
		if (inst.LK_3)
			MOV(32, M(&LR), Imm32(js.compilerPC + 4)); //	LR = PC + 4;
		AND(32, R(EAX), Imm32(0xFFFFFFFC));
		WriteExitDestInEAX(0);
	}
	else
	{
		// Rare condition seen in (just some versions of?) Nintendo's NES Emulator

		// BO_2 == 001zy -> b if false
		// BO_2 == 011zy -> b if true

		// Ripped from bclrx
		TEST(8, M(&PowerPC::ppcState.cr_fast[inst.BI >> 2]), Imm8(8 >> (inst.BI & 3)));
		Gen::CCFlags branch;
		if (inst.BO_2 & BO_BRANCH_IF_TRUE)
			branch = CC_Z;
		else
			branch = CC_NZ; 
		MOV(32, R(EAX), Imm32(js.compilerPC + 4));
		FixupBranch b = J_CC(branch, false);
		MOV(32, R(EAX), M(&CTR));
		MOV(32, M(&PC), R(EAX));
		if (inst.LK_3)
			MOV(32, M(&LR), Imm32(js.compilerPC + 4)); //	LR = PC + 4;
		// Would really like to continue the block here, but it ends. TODO.
		SetJumpTarget(b);
		WriteExitDestInEAX(0);	
		return;
	}
}


void Jit64::bclrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Branch)

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	//Special case BLR & BLRL
	if ((inst.hex & ~1) == 0x4e800020)
	{
		//CDynaRegCache::Flush();
		// This below line can be used to prove that blr "eats flags" in practice.
		// This observation will let us do a lot of fun observations.
#ifdef ACID_TEST
		AND(32, M(&CR), Imm32(~(0xFF000000)));
#endif
		MOV(32, R(EAX), M(&LR));
		MOV(32, M(&PC), R(EAX));
		if (inst.LK_3)
			MOV(32, M(&LR), Imm32(js.compilerPC + 4)); //	LR = PC + 4;
		WriteExitDestInEAX(0);
		return;
	} else if ((inst.BO_2 & BO_DONT_DECREMENT_FLAG) == 0) {
		// Decrement CTR. Not mutually exclusive...
		// Will fall back to int, but we should be able to do it here, sometime
	} else if ((inst.BO_2 & BO_DONT_CHECK_CONDITION) == 0) {
		// Test a CR bit. Not too hard.
		// beqlr- 4d820020
		// blelr- 4c810020
		// blrl   4e800021
		// bnelr  4c820020
		// etc...
		TEST(8, M(&PowerPC::ppcState.cr_fast[inst.BI >> 2]), Imm8(8 >> (inst.BI & 3)));
		Gen::CCFlags branch;
		if (inst.BO_2 & BO_BRANCH_IF_TRUE)
			branch = CC_Z;
		else
			branch = CC_NZ; 
		MOV(32, R(EAX), Imm32(js.compilerPC + 4));
		FixupBranch b = J_CC(branch, false);
		MOV(32, R(EAX), M(&LR));
		MOV(32, M(&PC), R(EAX));
		if (inst.LK_3)
			MOV(32, M(&LR), Imm32(js.compilerPC + 4)); //	LR = PC + 4;
		// Would really like to continue the block here, but it ends. TODO.
		SetJumpTarget(b);
		WriteExitDestInEAX(0);	
		return;
	}
	// Call interpreter
	Default(inst);
	MOV(32, R(EAX), M(&NPC));
	WriteExitDestInEAX(0);	
}
