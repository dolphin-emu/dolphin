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

//#define NORMALBRANCH_START Default(inst); ibuild.EmitInterpreterBranch(); return;
#define NORMALBRANCH_START

using namespace Gen;

	void Jit64::sc(UGeckoInstruction inst)
	{
		ibuild.EmitSystemCall(ibuild.EmitIntConst(js.compilerPC));
	}

	void Jit64::rfi(UGeckoInstruction inst)
	{
		ibuild.EmitRFIExit();
	}

	void Jit64::bx(UGeckoInstruction inst)
	{
		NORMALBRANCH_START
		INSTRUCTION_START;

		if (inst.LK)
			ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));

		u32 destination;
		if (inst.AA)
			destination = SignExt26(inst.LI << 2);
		else
			destination = js.compilerPC + SignExt26(inst.LI << 2);

		ibuild.EmitBranchUncond(ibuild.EmitIntConst(destination));
	}

	void Jit64::bcx(UGeckoInstruction inst)
	{
		NORMALBRANCH_START
		if (inst.LK)
			ibuild.EmitStoreLink(
				ibuild.EmitIntConst(js.compilerPC + 4));

		IREmitter::InstLoc CRTest = 0, CTRTest = 0;
		if ((inst.BO & 16) == 0)  // Test a CR bit
		{
			IREmitter::InstLoc CRReg = ibuild.EmitLoadCR(inst.BI >> 2);
			IREmitter::InstLoc CRCmp = ibuild.EmitIntConst(8 >> (inst.BI & 3));
			CRTest = ibuild.EmitAnd(CRReg, CRCmp);
			if (!(inst.BO & 8))
				CRTest = ibuild.EmitXor(CRTest, CRCmp);
		}

		if ((inst.BO & 4) == 0) {
			IREmitter::InstLoc c = ibuild.EmitLoadCTR();
			c = ibuild.EmitSub(c, ibuild.EmitIntConst(1));
			ibuild.EmitStoreCTR(c);
			if (inst.BO & 2) {
				CTRTest = ibuild.EmitICmpEq(c,
						ibuild.EmitIntConst(0));
			} else {
				CTRTest = c;
			}
		}

		IREmitter::InstLoc Test = CRTest;
		if (CTRTest) {
			if (Test)
				Test = ibuild.EmitAnd(Test, CTRTest);
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
		NORMALBRANCH_START
		Default(inst);
		ibuild.EmitInterpreterBranch();
		return;
	}

	void Jit64::bclrx(UGeckoInstruction inst)
	{
		NORMALBRANCH_START
		if (inst.hex == 0x4e800020) {
			ibuild.EmitBranchUncond(ibuild.EmitLoadLink());
			return;
		}
		Default(inst);
		ibuild.EmitInterpreterBranch();
		return;
	}

