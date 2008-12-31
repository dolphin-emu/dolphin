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

#include "../../Core.h"
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

	void Jit64::sc(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITBranchOff)
			{Default(inst); return;} // turn off from debugger

		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		WriteExceptionExit(EXCEPTION_SYSCALL);
	}

	void Jit64::rfi(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITBranchOff)
			{Default(inst); return;} // turn off from debugger

		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		//Bits SRR1[0, 5-9, 16-23, 25-27, 30-31] are placed into the corresponding bits of the MSR.
		//MSR[13] is set to 0.
		const u32 mask = 0x87C0FF73;
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
		if (inst.LK)
			ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));

		u32 destination;
		if (inst.AA)
			destination = SignExt26(inst.LI << 2);
		else
			destination = js.compilerPC + SignExt26(inst.LI << 2);

		ibuild.EmitBranchUncond(ibuild.EmitIntConst(destination));
	}

	// TODO - optimize to hell and beyond
	// TODO - make nice easy to optimize special cases for the most common
	// variants of this instruction.
	void Jit64::bcx(UGeckoInstruction inst)
	{
		if (inst.LK)
			ibuild.EmitStoreLink(
				ibuild.EmitIntConst(js.compilerPC + 4));

		IREmitter::InstLoc CRTest = 0, CTRTest = 0;
		if ((inst.BO & 16) == 0)  // Test a CR bit
		{
			IREmitter::InstLoc CRReg = ibuild.EmitLoadCR(inst.BI >> 2);
			IREmitter::InstLoc CRCmp = ibuild.EmitIntConst(8 >> (inst.BI & 3));
			CRTest = ibuild.EmitAnd(CRReg, CRCmp);
			if (inst.BO & 8)
				CRTest = ibuild.EmitXor(CRTest, CRCmp);
		}

		if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0) {
			IREmitter::InstLoc c = ibuild.EmitLoadCTR();
			c = ibuild.EmitSub(c, ibuild.EmitIntConst(1));
			ibuild.EmitStoreCTR(c);
		}

		if ((inst.BO & 4) == 0) {
			IREmitter::InstLoc c = ibuild.EmitLoadCTR();
			if (!(inst.BO & 2)) {
				CTRTest = ibuild.EmitICmpEq(c,
						ibuild.EmitIntConst(0));
			} else {
				CTRTest = c;
			}
		}

		IREmitter::InstLoc Test = CRTest;
		if (CTRTest) {
			if (Test)
				Test = ibuild.EmitOr(Test, CTRTest);
			else
				Test = CTRTest;
		}

		if (!Test) {
			PanicAlert("Unconditional conditional branch?!");
		}

		u32 destination;
		if(inst.AA)
			destination = SignExt16(inst.BD << 2);
		else
			destination = js.compilerPC + SignExt16(inst.BD << 2);

		ibuild.EmitBranchCond(Test, ibuild.EmitIntConst(destination));
		ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
	}

	void Jit64::bcctrx(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITBranchOff)
			{Default(inst); return;} // turn off from debugger

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


	void Jit64::bclrx(UGeckoInstruction inst)
	{
		if (inst.hex == 0x4e800020) {
			ibuild.EmitBranchUncond(ibuild.EmitLoadLink());
			return;
		}
		Default(inst);
		return;
	}

