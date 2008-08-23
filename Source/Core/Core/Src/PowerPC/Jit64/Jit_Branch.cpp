// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitCache.h"
#include "JitAsm.h"

// The branches are known good, or at least reasonably good.
// No need for a disable-mechanism.

// If defined, clears CR0 at blr and bl-s. If the assumption that
// flags never carry over between functions holds, then the task for 
// an optimizer becomes much easier.

// #define ACID_TEST

// Zelda and many more games seem to pass the Acid Test. 

using namespace Gen;
namespace Jit64
{
	void sc(UGeckoInstruction _inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		WriteExceptionExit(EXCEPTION_SYSCALL);
	}

	void rfi(UGeckoInstruction _inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		//Bits SRR1[0, 5-9, 16-23, 25-27, 30-31] are placed into the corresponding bits of the MSR.
		//MSR[13] is set to 0.
		const int mask = 0x87C0FF73;
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

	void bx(UGeckoInstruction inst)
	{
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
	void bcx(UGeckoInstruction inst)
	{
		_assert_msg_(DYNA_REC, js.isLastInstruction, "bcx not last instruction of block");

		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);

		CCFlags branch = CC_Z;

		//const bool only_counter_check = (inst.BO & 16) ? true : false;
		//const bool only_condition_check = (inst.BO & 4) ? true : false;
		//if (only_condition_check && only_counter_check)
		//	PanicAlert("Bizarre bcx encountered. Likely bad or corrupt code.");
		bool doFullTest = (inst.BO & 16) == 0 && (inst.BO & 4) == 0;
		bool ctrDecremented = false;

		if ((inst.BO & 16) == 0)  // Test a CR bit
		{
			TEST(32, M(&CR), Imm32(0x80000000 >> inst.BI));
			if (inst.BO & 8)  // Conditional branch 
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

		if ((inst.BO & 4) == 0)  // Decrement and test CTR
		{
			// Decrement CTR 
			SUB(32, M(&CTR), Imm8(1));
			ctrDecremented = true;
			// Test whether to branch if CTR is zero or not 
			if (inst.BO & 2)
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
		FixupBranch skip = J_CC(branch);
			u32 destination;
			if (inst.LK)
				MOV(32, M(&LR), Imm32(js.compilerPC + 4));
			if(inst.AA)
				destination = SignExt16(inst.BD << 2);
			else
				destination = js.compilerPC + SignExt16(inst.BD << 2);
			WriteExit(destination, 0);
		SetJumpTarget(skip);
		WriteExit(js.compilerPC + 4, 1);
	}

	void bcctrx(UGeckoInstruction inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);

		// bool fastway = true;

		if ((inst.BO & 16) == 0)				
		{
			PanicAlert("Bizarro bcctrx %08x, not supported.", inst.hex);
			_assert_msg_(DYNA_REC, 0, "Bizarro bcctrx");
			/*
			fastway = false;
			MOV(32, M(&PC), Imm32(js.compilerPC+4));
			MOV(32, R(EAX), M(&CR));
			XOR(32, R(ECX), R(ECX));
			AND(32, R(EAX), Imm32(0x80000000 >> inst.BI));

			CCFlags branch;
			if(inst.BO & 8)
				branch = CC_NZ;
			else
				branch = CC_Z;
				*/
			// TODO(ector): Why is this commented out?
			//SETcc(branch, R(ECX));
			// check for EBX
			//TEST(32, R(ECX), R(ECX));
			//linkEnd = J_CC(branch);
		}
		// NPC = CTR & 0xfffffffc;
		MOV(32, R(EAX), M(&CTR));
		if (inst.LK)
			MOV(32, M(&LR), Imm32(js.compilerPC + 4)); //	LR = PC + 4;
		AND(32, R(EAX), Imm32(0xFFFFFFFC));
		WriteExitDestInEAX(0);
	}


	void bclrx(UGeckoInstruction inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		//Special case BLR
		if (inst.hex == 0x4e800020)
		{
			//CDynaRegCache::Flush();
			// This below line can be used to prove that blr "eats flags" in practice.
			// This observation will let us do a lot of fun observations.
#ifdef ACID_TEST
			AND(32, M(&CR), Imm32(~(0xFF000000)));
#endif
			MOV(32, R(EAX), M(&LR));
			MOV(32, M(&PC), R(EAX));
			WriteExitDestInEAX(0);
			return;
		}
		// Call interpreter
		Default(inst);
		MOV(32, R(EAX), M(&NPC));
		WriteExitDestInEAX(0);
	}

}
