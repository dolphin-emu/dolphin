// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

// TODO
// ps_madds0
// ps_muls0
// ps_madds1
//   cmppd, andpd, andnpd, or
//   lfsx, ps_merge01 etc

static const u64 GC_ALIGNED16(psSignBits[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};

void Jit64::ps_mr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int b = inst.FB;
	if (d == b)
		return;

	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), fpr.R(b));
}

void Jit64::ps_sel(UGeckoInstruction inst)
{
	// we can't use (V)BLENDVPD here because it just looks at the sign bit
	// but we need -0 = +0

	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;

	fpr.Lock(a, b, c, d);
	MOVAPD(XMM0, fpr.R(a));
	PXOR(XMM1, R(XMM1));
	// XMM0 = XMM0 < 0 ? all 1s : all 0s
	CMPPD(XMM0, R(XMM1), LT);
	MOVAPD(XMM1, R(XMM0));
	PAND(XMM0, fpr.R(b));
	PANDN(XMM1, fpr.R(c));
	POR(XMM0, R(XMM1));
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), R(XMM0));
	fpr.UnlockAll();
}

void Jit64::ps_sign(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

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
		PXOR(fpr.RX(d), M((void*)&psSignBits));
		break;
	case 136: //nabs
		POR(fpr.RX(d), M((void*)&psSignBits));
		break;
	case 264: //abs
		PAND(fpr.RX(d), M((void*)&psAbsMask));
		break;
	}

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
void Jit64::tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(X64Reg, OpArg), bool roundRHS)
{
	fpr.Lock(d, a, b);

	if (roundRHS)
	{
		if (d == a)
		{
			fpr.BindToRegister(d, true);
			MOVAPD(XMM0, fpr.R(b));
			Force25BitPrecision(XMM0, XMM1);
			(this->*op)(fpr.RX(d), R(XMM0));
		}
		else
		{
			fpr.BindToRegister(d, d == b);
			if (d != b)
				MOVAPD(fpr.RX(d), fpr.R(b));
			Force25BitPrecision(fpr.RX(d), XMM0);
			(this->*op)(fpr.RX(d), fpr.R(a));
		}
	}
	else if (d == a)
	{
		fpr.BindToRegister(d, true);
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	else if (d == b)
	{
		if (reversible)
		{
			fpr.BindToRegister(d, true);
			(this->*op)(fpr.RX(d), fpr.R(a));
		}
		else
		{
			MOVAPD(XMM0, fpr.R(b));
			fpr.BindToRegister(d, false);
			MOVAPD(fpr.RX(d), fpr.R(a));
			(this->*op)(fpr.RX(d), Gen::R(XMM0));
		}
	}
	else
	{
		//sources different from d, can use rather quick solution
		fpr.BindToRegister(d, false);
		MOVAPD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	ForceSinglePrecisionP(fpr.RX(d));
	fpr.UnlockAll();
}

void Jit64::ps_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	switch (inst.SUBOP5)
	{
	case 18: // div
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::DIVPD);
		break;
	case 20: // sub
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::SUBPD);
		break;
	case 21: // add
		tri_op(inst.FD, inst.FA, inst.FB, true,  &XEmitter::ADDPD);
		break;
	case 25: // mul
		tri_op(inst.FD, inst.FA, inst.FC, true, &XEmitter::MULPD, true);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "ps_arith WTF!!!");
		break;
	}
}

void Jit64::ps_sum(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	fpr.Lock(a,b,c,d);
	fpr.BindToRegister(d, d == a || d == b || d == c, true);
	switch (inst.SUBOP5)
	{
	case 10:
		// ps_sum0, do the sum in upper subregisters, merge uppers
		MOVDDUP(XMM0, fpr.R(a));
		MOVAPD(XMM1, fpr.R(b));
		ADDPD(XMM0, R(XMM1));
		UNPCKHPD(XMM0, fpr.R(c)); //merge
		MOVAPD(fpr.R(d), XMM0);
		break;
	case 11:
		// ps_sum1, do the sum in lower subregisters, merge lowers
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
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

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
		MOVDDUP(XMM1, fpr.R(c));
		Force25BitPrecision(XMM1, XMM0);
		MOVAPD(XMM0, fpr.R(a));
		MULPD(XMM0, R(XMM1));
		MOVAPD(fpr.R(d), XMM0);
		break;
	case 13:
		// TODO - faster version for when regs are different
		MOVAPD(XMM1, fpr.R(c));
		Force25BitPrecision(XMM1, XMM0);
		MOVAPD(XMM0, fpr.R(a));
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
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

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
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	int d = inst.FD;
	fpr.Lock(a,b,c,d);

	switch (inst.SUBOP5)
	{
	case 14: //madds0
		MOVDDUP(XMM1, fpr.R(c));
		Force25BitPrecision(XMM1, XMM0);
		MOVAPD(XMM0, fpr.R(a));
		MULPD(XMM0, R(XMM1));
		ADDPD(XMM0, fpr.R(b));
		break;
	case 15: //madds1
		MOVAPD(XMM1, fpr.R(c));
		SHUFPD(XMM1, R(XMM1), 3); // copy higher to lower
		Force25BitPrecision(XMM1, XMM0);
		MOVAPD(XMM0, fpr.R(a));
		MULPD(XMM0, R(XMM1));
		ADDPD(XMM0, fpr.R(b));
		break;
	case 28: //msub
		MOVAPD(XMM0, fpr.R(c));
		Force25BitPrecision(XMM0, XMM1);
		MULPD(XMM0, fpr.R(a));
		SUBPD(XMM0, fpr.R(b));
		break;
	case 29: //madd
		MOVAPD(XMM0, fpr.R(c));
		Force25BitPrecision(XMM0, XMM1);
		MULPD(XMM0, fpr.R(a));
		ADDPD(XMM0, fpr.R(b));
		break;
	case 30: //nmsub
		MOVAPD(XMM0, fpr.R(c));
		Force25BitPrecision(XMM0, XMM1);
		MULPD(XMM0, fpr.R(a));
		SUBPD(XMM0, fpr.R(b));
		PXOR(XMM0, M((void*)&psSignBits));
		break;
	case 31: //nmadd
		MOVAPD(XMM0, fpr.R(c));
		Force25BitPrecision(XMM0, XMM1);
		MULPD(XMM0, fpr.R(a));
		ADDPD(XMM0, fpr.R(b));
		PXOR(XMM0, M((void*)&psSignBits));
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "ps_maddXX WTF!!!");
		//FallBackToInterpreter(inst);
		//fpr.UnlockAll();
		return;
	}
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), Gen::R(XMM0));
	ForceSinglePrecisionP(fpr.RX(d));
	fpr.UnlockAll();
}
