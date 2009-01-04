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

#include "../../Core.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitRegCache.h"

	void Jit64::fp_arith_s(UGeckoInstruction inst)
	{
		if (inst.Rc || inst.OPCD != 59 || inst.SUBOP5 != 25) {
			Default(inst); return;
		}
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
		val = ibuild.EmitDoubleToSingle(val);
		bool dupe = inst.OPCD == 59;
		switch (inst.SUBOP5)
		{
		case 25: //mul
			val = ibuild.EmitFSMul(val, ibuild.EmitDoubleToSingle(ibuild.EmitLoadFReg(inst.FC)));
		case 18: //div
		case 20: //sub
		case 21: //add
		case 23: //sel
		case 24: //res
		default:
			_assert_msg_(DYNA_REC, 0, "fp_arith_s WTF!!!");
		}
		val = ibuild.EmitDupSingleToMReg(val);
		ibuild.EmitStoreFReg(val, inst.FD);
	}

	void Jit64::fmaddXX(UGeckoInstruction inst)
	{
		if (inst.Rc || inst.OPCD != 59 || inst.SUBOP5 != 29) {
			Default(inst); return;
		}

		bool single_precision = inst.OPCD == 59;

		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
		val = ibuild.EmitDoubleToSingle(val);
		val = ibuild.EmitFSMul(val, ibuild.EmitDoubleToSingle(ibuild.EmitLoadFReg(inst.FC)));
		val = ibuild.EmitFSAdd(val, ibuild.EmitDoubleToSingle(ibuild.EmitLoadFReg(inst.FB)));
		val = ibuild.EmitDupSingleToMReg(val);
		ibuild.EmitStoreFReg(val, inst.FD);
	}
	
	void Jit64::fmrx(UGeckoInstruction inst)
	{
		if (inst.Rc) {
			Default(inst); return;
		}
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FB);
		val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
		ibuild.EmitStoreFReg(val, inst.FD);
	}

	void Jit64::fcmpx(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITFloatingPointOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (jo.fpAccurateFlags)
		{
			Default(inst);
			return;
		}
		bool ordered = inst.SUBOP10 == 32;
	/*
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);
	u32 compareResult;

	if(IsNAN(fa) ||	IsNAN(fb))	compareResult = 1; 
	else if(fa < fb)			compareResult = 8;	
	else if(fa > fb)			compareResult = 4;
	else						compareResult = 2;

	FPSCR.FPRF = compareResult;
	CR = (CR & (~(0xf0000000 >> (_inst.CRFD * 4)))) | (compareResult <<	((7	- _inst.CRFD) *	4));
*/
		int a = inst.FA;
		int b = inst.FB;
		int crf = inst.CRFD;
		int shift = crf * 4;
		//FPSCR
		//XOR(32,R(EAX),R(EAX));

		fpr.Lock(a,b);
		if (a != b)
			fpr.LoadToX64(a, true);

		// USES_CR
		if (ordered)
			COMISD(fpr.R(a).GetSimpleReg(), fpr.R(b));
		else
			UCOMISD(fpr.R(a).GetSimpleReg(), fpr.R(b));
		FixupBranch pLesser  = J_CC(CC_B);
		FixupBranch pGreater = J_CC(CC_A);
		// _x86Reg == 0
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x2));
		FixupBranch continue1 = J();
		// _x86Reg > 0
		SetJumpTarget(pGreater);
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x4));
		FixupBranch continue2 = J();
		// _x86Reg < 0
		SetJumpTarget(pLesser);
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x8));
		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
		fpr.UnlockAll();
	}
