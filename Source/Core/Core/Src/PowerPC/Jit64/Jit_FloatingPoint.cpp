// Copyright (C) 2003-2009 Dolphin Project.

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
#include "JitRegCache.h"

	const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
	const u64 GC_ALIGNED16(psAbsMask2[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
	const double GC_ALIGNED16(psOneOne2[2]) = {1.0, 1.0};

	void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg))
	{
		fpr.Lock(d, a, b);
		if (d == a)
		{
			fpr.LoadToX64(d, true);
			(this->*op)(fpr.RX(d), fpr.R(b));
		}
		else if (d == b && reversible)
		{
			fpr.LoadToX64(d, true);
			(this->*op)(fpr.RX(d), fpr.R(a));
		}
		else if (a != d && b != d) 
		{
			// Sources different from d, can use rather quick solution
			fpr.LoadToX64(d, !dupe);
			MOVSD(fpr.RX(d), fpr.R(a));
			(this->*op)(fpr.RX(d), fpr.R(b));
		}
		else if (b != d)
		{
			fpr.LoadToX64(d, !dupe);
			MOVSD(XMM0, fpr.R(b));
			MOVSD(fpr.RX(d), fpr.R(a));
			(this->*op)(fpr.RX(d), Gen::R(XMM0));
		}
		else // Other combo, must use two temps :(
		{
			MOVSD(XMM0, fpr.R(a));
			MOVSD(XMM1, fpr.R(b));
			fpr.LoadToX64(d, !dupe);
			(this->*op)(XMM0, Gen::R(XMM1));
			MOVSD(fpr.RX(d), Gen::R(XMM0));
		}
		if (dupe) {
			ForceSinglePrecisionS(fpr.RX(d));
			MOVDDUP(fpr.RX(d), fpr.R(d));
		}
		fpr.UnlockAll();
	}

	void Jit64::fp_arith_s(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITFloatingPointOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		bool dupe = inst.OPCD == 59;
		switch (inst.SUBOP5)
		{
		case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, dupe, &XEmitter::DIVSD); break; //div
		case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, dupe, &XEmitter::SUBSD); break; //sub
		case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true,  dupe, &XEmitter::ADDSD); break; //add
		case 23: //sel
			Default(inst);
			break;
		case 24: //res
			Default(inst);
			break;
		case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true, dupe, &XEmitter::MULSD); break; //mul
		default:
			_assert_msg_(DYNA_REC, 0, "fp_arith_s WTF!!!");
		}
	}

	void Jit64::fmaddXX(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITFloatingPointOff)
			{Default(inst); return;} // turn off from debugger		
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}

		bool single_precision = inst.OPCD == 59;

		int a = inst.FA;
		int b = inst.FB;
		int c = inst.FC;
		int d = inst.FD;

		fpr.Lock(a, b, c, d);
		MOVSD(XMM0, fpr.R(a));
		switch (inst.SUBOP5)
		{
		case 28: //msub
			MULSD(XMM0, fpr.R(c));
			SUBSD(XMM0, fpr.R(b));
			break;
		case 29: //madd
			MULSD(XMM0, fpr.R(c));
			ADDSD(XMM0, fpr.R(b));
			break;
		case 30: //nmsub
			MULSD(XMM0, fpr.R(c));
			SUBSD(XMM0, fpr.R(b));
			XORPD(XMM0, M((void*)&psSignBits2));
			break;
		case 31: //nmadd
			MULSD(XMM0, fpr.R(c));
			ADDSD(XMM0, fpr.R(b));
			XORPD(XMM0, M((void*)&psSignBits2));
			break;
		}
		fpr.LoadToX64(d, false);
		//YES it is necessary to dupe the result :(
		//TODO : analysis - does the top reg get used? If so, dupe, if not, don't.
		if (single_precision) {
			ForceSinglePrecisionS(XMM0);
			MOVDDUP(fpr.RX(d), R(XMM0));
		} else {
			MOVSD(fpr.RX(d), R(XMM0));
		}
		fpr.UnlockAll();
	}
	
	void Jit64::fmrx(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITFloatingPointOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int b = inst.FB;
		fpr.LoadToX64(d, true);  // we don't want to destroy the high bit
		MOVSD(fpr.RX(d), fpr.R(b));
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
