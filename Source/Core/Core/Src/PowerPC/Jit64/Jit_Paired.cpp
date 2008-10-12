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
#include "Jit_Util.h"

// TODO
// ps_madds0
// ps_muls0
// ps_madds1
// ps_sel
//   cmppd, andpd, andnpd, or
//   lfsx, ps_merge01 etc

// #define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

#ifdef _M_IX86
#define DISABLE_32BIT Default(inst); return;
#else
#define DISABLE_32BIT ;
#endif

namespace Jit64
{
	const u64 GC_ALIGNED16(psSignBits[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
	const u64 GC_ALIGNED16(psAbsMask[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
	const double GC_ALIGNED16(psOneOne[2])  = {1.0, 1.0};
	const double GC_ALIGNED16(psZeroZero[2]) = {0.0, 0.0};

	void ps_mr(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
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

	void ps_sel(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
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

	void ps_sign(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
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

	void ps_rsqrte(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
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

	//There's still a little bit more optimization that can be squeezed out of this
	void tri_op(int d, int a, int b, bool reversible, void (*op)(X64Reg, OpArg))
	{
		fpr.Lock(d, a, b);
		
		if (d == a)
		{
			fpr.LoadToX64(d, true);
			op(fpr.RX(d), fpr.R(b));
		}
		else if (d == b && reversible)
		{
			fpr.LoadToX64(d, true);
			op(fpr.RX(d), fpr.R(a));
		}
		else if (a != d && b != d) 
		{
			//sources different from d, can use rather quick solution
			fpr.LoadToX64(d, false);
			MOVAPD(fpr.RX(d), fpr.R(a));
			op(fpr.RX(d), fpr.R(b));
		}
		else if (b != d)
		{
			fpr.LoadToX64(d, false);
			MOVAPD(XMM0, fpr.R(b));
			MOVAPD(fpr.RX(d), fpr.R(a));
			op(fpr.RX(d), Gen::R(XMM0));
		}
		else //Other combo, must use two temps :(
		{
			MOVAPD(XMM0, fpr.R(a));
			MOVAPD(XMM1, fpr.R(b));
			fpr.LoadToX64(d, false);
			op(XMM0, Gen::R(XMM1));
			MOVAPD(fpr.RX(d), Gen::R(XMM0));
		}
		ForceSinglePrecisionP(fpr.RX(d));
		fpr.UnlockAll();
	}

	void ps_arith(UGeckoInstruction inst)
	{	
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		switch (inst.SUBOP5)
		{
		case 18: tri_op(inst.FD, inst.FA, inst.FB, false, &DIVPD); break; //div
		case 20: tri_op(inst.FD, inst.FA, inst.FB, false, &SUBPD); break; //sub
		case 21: tri_op(inst.FD, inst.FA, inst.FB, true,  &ADDPD); break; //add
		case 23://sel
			Default(inst);
			break;
		case 24://res
			Default(inst);
			break;
		case 25: tri_op(inst.FD, inst.FA, inst.FC, true, &MULPD); break; //mul
		default:
			_assert_msg_(DYNA_REC, 0, "ps_arith WTF!!!");
		}
	}

	//TODO: find easy cases and optimize them, do a breakout like ps_arith
	void ps_mergeXX(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int d = inst.FD;
		int a = inst.FA;
		int b = inst.FB;
		fpr.Lock(a,b,d);

		MOVAPD(XMM0, fpr.R(a));
		switch (inst.SUBOP10)
		{
		case 528: 
			UNPCKLPD(XMM0, fpr.R(b)); //unpck is faster than shuf
			break; //00
		case 560:
			SHUFPD(XMM0, fpr.R(b), 2); //must use shuf here
			break; //01
		case 592:
			SHUFPD(XMM0, fpr.R(b), 1);
			break; //10
		case 624:
			UNPCKHPD(XMM0, fpr.R(b));
			break; //11
		default:
			_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
		}
		fpr.LoadToX64(d, false);
		MOVAPD(fpr.RX(d), Gen::R(XMM0));
		fpr.UnlockAll();
	}

	//one op is assumed to be add
	void quad_op(int d, int a, int b, int c, void (*op)(X64Reg, OpArg))
	{
		
	}

	//TODO: add optimized cases
	void ps_maddXX(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		if (inst.Rc) {
			Default(inst); return;
		}
		int a = inst.FA;
		int b = inst.FB;
		int c = inst.FC;
		int d = inst.FD;
		fpr.Lock(a,b,c,d);

		MOVAPD(XMM0, fpr.R(a));
		switch (inst.SUBOP5)
		{
		case 28: //msub
			MULPD(XMM0, fpr.R(c));
			SUBPD(XMM0, fpr.R(b));
			break;
		case 29: //madd
			MULPD(XMM0, fpr.R(c));
			ADDPD(XMM0, fpr.R(b));
			break;
		case 30: //nmsub
			MULPD(XMM0, fpr.R(c));
			SUBPD(XMM0, fpr.R(b));
			XORPD(XMM0, M((void*)&psSignBits));
			break;
		case 31: //nmadd
			MULPD(XMM0, fpr.R(c));
			ADDPD(XMM0, fpr.R(b));
			XORPD(XMM0, M((void*)&psSignBits));
			break;
		default:
			_assert_msg_(DYNA_REC, 0, "ps_maddXX WTF!!!");
			//Default(inst);
			//fpr.UnlockAll();
			return;
		}
		fpr.LoadToX64(d, false);
		MOVAPD(fpr.RX(d), Gen::R(XMM0));
		ForceSinglePrecisionP(fpr.RX(d));
		fpr.UnlockAll();
	}

	void ps_mulsX(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITPairedOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		Default(inst);
		return;
		if (inst.Rc) {
			Default(inst); return;
		}

		switch (inst.SUBOP5)
		{
		case 12:
		case 13:
			break;
		}
	}
}
