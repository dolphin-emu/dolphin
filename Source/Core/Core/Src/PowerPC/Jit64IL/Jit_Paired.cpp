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
#include "../../HW/GPFifo.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitRegCache.h"

// TODO
// ps_madds0
// ps_muls0
// ps_madds1
// ps_sel
//   cmppd, andpd, andnpd, or
//   lfsx, ps_merge01 etc

	const u64 GC_ALIGNED16(psSignBits[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
	const u64 GC_ALIGNED16(psAbsMask[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
	const double GC_ALIGNED16(psOneOne[2])  = {1.0, 1.0};
	const double GC_ALIGNED16(psZeroZero[2]) = {0.0, 0.0};

	void Jit64::ps_mr(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int b = inst.FB;
		if (d == b)
			return;
		fpr.LoadToX64(d, false);
		MOVAPD(fpr.RX(d), fpr.R(b));
	}

	void Jit64::ps_sel(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		Default(inst);
		return;
		
		if (inst.Rc) {
			Default(inst); return;
		}
		// GRR can't get this to work 100%. Getting artifacts in D.O.N. intro.
		int d = inst.FD;
		int a = inst.FA;
		int b = inst.FB;
		int c = inst.FC;
		fpr.FlushLockX(XMM7);
		fpr.FlushLockX(XMM6);
		fpr.Lock(a, b, c, d);
		fpr.LoadToX64(a, true, false);
		fpr.LoadToX64(d, false, true);
		// BLENDPD would have been nice...
		MOVAPD(XMM7, fpr.R(a));
		CMPPD(XMM7, M((void*)psZeroZero), 1); //less-than = 111111
		MOVAPD(XMM6, R(XMM7));
		ANDPD(XMM7, fpr.R(d));
		ANDNPD(XMM6, fpr.R(c));
		MOVAPD(fpr.RX(d), R(XMM7));
		ORPD(fpr.RX(d), R(XMM6));
		fpr.UnlockAll();
		fpr.UnlockAllX();
	}

	void Jit64::ps_sign(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int b = inst.FB;

		fpr.Lock(d, b);
		if (d != b)
		{
			fpr.LoadToX64(d, false);
			MOVAPD(fpr.RX(d), fpr.R(b));
		}
		else
		{
			fpr.LoadToX64(d, true);
		}

		switch (inst.SUBOP10)
		{
		case 40: //neg 
			XORPD(fpr.RX(d), M((void*)&psSignBits));
			break;
		case 136: //nabs
			ORPD(fpr.RX(d), M((void*)&psSignBits));
			break;
		case 264: //abs
			ANDPD(fpr.RX(d), M((void*)&psAbsMask));
			break;
		}

		fpr.UnlockAll();
	}

	void Jit64::ps_rsqrte(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int b = inst.FB;
		fpr.Lock(d, b);
		SQRTPD(XMM0, fpr.R(b));
		MOVAPD(XMM1, M((void*)&psOneOne));
		DIVPD(XMM1, R(XMM0));
		MOVAPD(fpr.R(d), XMM1);
		fpr.UnlockAll();
	}

	//add a, b, c
	
	//mov a, b
	//add a, c
	//we need:
	/*
	psq_l
	psq_stu
	*/
	
	/*
	add a,b,a
	*/

	void Jit64::ps_arith(UGeckoInstruction inst)
	{
		if (inst.Rc || (inst.SUBOP5 != 21 && inst.SUBOP5 != 20 && inst.SUBOP5 != 25)) {
			Default(inst); return;
		}
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), rhs;
		if (inst.SUBOP5 == 25)
			rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		else
			rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitCompactMRegToPacked(val);
		
		switch (inst.SUBOP5)
		{
		case 20:
			val = ibuild.EmitFPSub(val, rhs);
			break;
		case 21:
			val = ibuild.EmitFPAdd(val, rhs);
			break;
		case 25:
			val = ibuild.EmitFPMul(val, rhs);
		}
		val = ibuild.EmitExpandPackedToMReg(val);
		ibuild.EmitStoreFReg(val, inst.FD);
	}

	void Jit64::ps_sum(UGeckoInstruction inst)
	{	
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int a = inst.FA;
		int b = inst.FB;
		int c = inst.FC;
		fpr.Lock(a,b,c,d);
		fpr.LoadToX64(d, d == a || d == b || d == c, true);
		switch (inst.SUBOP5)
		{
		case 10:
			// Do the sum in upper subregisters, merge uppers
			MOVDDUP(XMM0, fpr.R(a));
			MOVAPD(XMM1, fpr.R(b));
			ADDPD(XMM0, R(XMM1));
			UNPCKHPD(XMM0, fpr.R(c)); //merge
			MOVAPD(fpr.R(d), XMM0);
			break;
		case 11:
			// Do the sum in lower subregisters, merge lowers
			MOVAPD(XMM0, fpr.R(a));
			MOVAPD(XMM1, fpr.R(b));
			SHUFPD(XMM1, R(XMM1), 5); // copy higher to lower
			ADDPD(XMM0, R(XMM1)); // sum lowers
			MOVAPD(XMM1, fpr.R(c));  
			UNPCKLPD(XMM1, R(XMM0)); // merge
			MOVAPD(fpr.R(d), XMM1);
			break;
		default:
			PanicAlert("ps_sum WTF!!!");
		}
		ForceSinglePrecisionP(fpr.RX(d));
		fpr.UnlockAll();
	}


	void Jit64::ps_muls(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int a = inst.FA;
		int c = inst.FC;
		fpr.Lock(a, c, d);
		fpr.LoadToX64(d, d == a || d == c, true);
		switch (inst.SUBOP5)
		{
		case 12:
			// Single multiply scalar high
			// TODO - faster version for when regs are different
			MOVAPD(XMM0, fpr.R(a));
			MOVDDUP(XMM1, fpr.R(c));
			MULPD(XMM0, R(XMM1));
			MOVAPD(fpr.R(d), XMM0);
			break;
		case 13:
			// TODO - faster version for when regs are different
			MOVAPD(XMM0, fpr.R(a));
			MOVAPD(XMM1, fpr.R(c));
			SHUFPD(XMM1, R(XMM1), 3); // copy higher to lower
			MULPD(XMM0, R(XMM1));
			MOVAPD(fpr.R(d), XMM0);
			break;
		default:
			PanicAlert("ps_muls WTF!!!");
		}
		ForceSinglePrecisionP(fpr.RX(d));
		fpr.UnlockAll();
	}


	//TODO: find easy cases and optimize them, do a breakout like ps_arith
	void Jit64::ps_mergeXX(UGeckoInstruction inst)
	{
		if (inst.Rc) {
			Default(inst); return;
		}

		IREmitter::InstLoc val = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FA)),
				   rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));

		switch (inst.SUBOP10)
		{
		case 528: 
			val = ibuild.EmitFPMerge00(val, rhs);
			break; //00
		case 560:
			val = ibuild.EmitFPMerge01(val, rhs);
			break; //01
		case 592:
			val = ibuild.EmitFPMerge10(val, rhs);
			break; //10
		case 624:
			val = ibuild.EmitFPMerge11(val, rhs);
			break; //11
		default:
			_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
		}
		val = ibuild.EmitExpandPackedToMReg(val);
		ibuild.EmitStoreFReg(val, inst.FD);
	}


	void Jit64::ps_maddXX(UGeckoInstruction inst)
	{
		if (inst.Rc || (inst.SUBOP5 != 28 && inst.SUBOP5 != 29 && inst.SUBOP5 != 30)) {
			Default(inst); return;
		}
		
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), op2, op3;
		val = ibuild.EmitCompactMRegToPacked(val);
		switch (inst.SUBOP5)
		{
		case 28: {//msub
			op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
			val = ibuild.EmitFPMul(val, op2);
			op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
			val = ibuild.EmitFPSub(val, op3);
			break;
		}
		case 29: {//madd
			op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
			val = ibuild.EmitFPMul(val, op2);
			op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
			val = ibuild.EmitFPAdd(val, op3);
			break;
		}
		case 30: {//nmsub
			op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
			val = ibuild.EmitFPMul(val, op2);
			op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
			val = ibuild.EmitFPSub(val, op3);
			val = ibuild.EmitFPNeg(val);
			break;
		}
		}
		val = ibuild.EmitExpandPackedToMReg(val);
		ibuild.EmitStoreFReg(val, inst.FD);
	}
