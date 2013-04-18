// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "../../HW/GPFifo.h"

#include "Jit.h"
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
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	int d = inst.FD;
	int b = inst.FB;
	if (d == b)
		return;
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), fpr.R(b));
}

void Jit64::ps_sel(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)

	Default(inst); return;

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
	fpr.BindToRegister(a, true, false);
	fpr.BindToRegister(d, false, true);
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
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	int d = inst.FD;
	int b = inst.FB;

	fpr.Lock(d, b);
	if (d != b)
	{
		fpr.BindToRegister(d, false);
		MOVAPD(fpr.RX(d), fpr.R(b));
	}
	else
	{
		fpr.BindToRegister(d, true);
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
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(d, b);
	fpr.BindToRegister(d, (d == b), true);
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
void Jit64::tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(X64Reg, OpArg))
{
	fpr.Lock(d, a, b);

	if (d == a)
	{
		fpr.BindToRegister(d, true);
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	else if (d == b && reversible)
	{
		fpr.BindToRegister(d, true);
		(this->*op)(fpr.RX(d), fpr.R(a));
	}
	else if (a != d && b != d) 
	{
		//sources different from d, can use rather quick solution
		fpr.BindToRegister(d, false);
		MOVAPD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	else if (b != d)
	{
		fpr.BindToRegister(d, false);
		MOVAPD(XMM0, fpr.R(b));
		MOVAPD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), Gen::R(XMM0));
	}
	else //Other combo, must use two temps :(
	{
		MOVAPD(XMM0, fpr.R(a));
		MOVAPD(XMM1, fpr.R(b));
		fpr.BindToRegister(d, false);
		(this->*op)(XMM0, Gen::R(XMM1));
		MOVAPD(fpr.RX(d), Gen::R(XMM0));
	}
	ForceSinglePrecisionP(fpr.RX(d));
	fpr.UnlockAll();
}

void Jit64::ps_arith(UGeckoInstruction inst)
{		
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	switch (inst.SUBOP5)
	{
	case 18: tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::DIVPD); break; //div
	case 20: tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::SUBPD); break; //sub 
	case 21: tri_op(inst.FD, inst.FA, inst.FB, true,  &XEmitter::ADDPD); break; //add
	case 23: Default(inst); break; //sel
	case 24: Default(inst); break; //res
	case 25: tri_op(inst.FD, inst.FA, inst.FC, true, &XEmitter::MULPD); break; //mul
	default:
		_assert_msg_(DYNA_REC, 0, "ps_arith WTF!!!");
	}
}

void Jit64::ps_sum(UGeckoInstruction inst)
{	
	INSTRUCTION_START
	JITDISABLE(Paired)
	// TODO: (inst.SUBOP5 == 10) breaks Sonic Colours (black screen) 
	if (inst.Rc || (inst.SUBOP5 == 10)) {
		Default(inst); return;
	}
	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	fpr.Lock(a,b,c,d);
	fpr.BindToRegister(d, d == a || d == b || d == c, true);
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
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	int d = inst.FD;
	int a = inst.FA;
	int c = inst.FC;
	fpr.Lock(a, c, d);
	fpr.BindToRegister(d, d == a || d == c, true);
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
	INSTRUCTION_START
	JITDISABLE(Paired)
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
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), Gen::R(XMM0));
	fpr.UnlockAll();
}


//TODO: add optimized cases
void Jit64::ps_maddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
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
	case 14: //madds0
		MOVDDUP(XMM1, fpr.R(c));
		MULPD(XMM0, R(XMM1));
		ADDPD(XMM0, fpr.R(b));
		break;
	case 15: //madds1
		MOVAPD(XMM1, fpr.R(c));
		SHUFPD(XMM1, R(XMM1), 3); // copy higher to lower
		MULPD(XMM0, R(XMM1));
		ADDPD(XMM0, fpr.R(b));
		break;
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
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), Gen::R(XMM0));
	ForceSinglePrecisionP(fpr.RX(d));
	fpr.UnlockAll();
}
