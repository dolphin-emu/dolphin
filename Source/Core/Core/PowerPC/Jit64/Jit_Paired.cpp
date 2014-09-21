// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

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
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;

	fpr.Lock(a, b, c, d);

	if (cpu_info.bSSE4_1)
	{
		MOVAPD(XMM1, fpr.R(a));
		PXOR(XMM0, R(XMM0));
		CMPPD(XMM0, R(XMM1), NLE);
		MOVAPD(XMM1, fpr.R(c));
		BLENDVPD(XMM1, fpr.R(b));
	}
	else
	{
		MOVAPD(XMM0, fpr.R(a));
		PXOR(XMM1, R(XMM1));
		CMPPD(XMM1, R(XMM0), NLE);
		MOVAPD(XMM0, R(XMM1));
		PAND(XMM1, fpr.R(b));
		PANDN(XMM0, fpr.R(c));
		POR(XMM1, R(XMM0));
	}
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), R(XMM1));
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

//There's still a little bit more optimization that can be squeezed out of this
void Jit64::tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(X64Reg, OpArg), UGeckoInstruction inst, bool roundRHS)
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
			(this->*op)(fpr.RX(d), R(XMM0));
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
	SetFPRFIfNeeded(inst, fpr.RX(d));
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
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::DIVPD, inst);
		break;
	case 20: // sub
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::SUBPD, inst);
		break;
	case 21: // add
		tri_op(inst.FD, inst.FA, inst.FB, true, &XEmitter::ADDPD, inst);
		break;
	case 25: // mul
		tri_op(inst.FD, inst.FA, inst.FC, true, &XEmitter::MULPD, inst, true);
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
	switch (inst.SUBOP5)
	{
	case 10:
		MOVDDUP(XMM0, fpr.R(a));  // {a.ps0, a.ps0}
		ADDPD(XMM0, fpr.R(b));    // {a.ps0 + b.ps0, a.ps0 + b.ps1}
		UNPCKHPD(XMM0, fpr.R(c)); // {a.ps0 + b.ps1, c.ps1}
		break;
	case 11:
		MOVDDUP(XMM1, fpr.R(a));  // {a.ps0, a.ps0}
		ADDPD(XMM1, fpr.R(b));    // {a.ps0 + b.ps0, a.ps0 + b.ps1}
		MOVAPD(XMM0, fpr.R(c));
		SHUFPD(XMM0, R(XMM1), 2); // {c.ps0, a.ps0 + b.ps1}
		break;
	default:
		PanicAlert("ps_sum WTF!!!");
	}
	ForceSinglePrecisionP(XMM0);
	SetFPRFIfNeeded(inst, XMM0);
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), R(XMM0));
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
	switch (inst.SUBOP5)
	{
	case 12:
		MOVDDUP(XMM0, fpr.R(c));
		break;
	case 13:
		MOVAPD(XMM0, fpr.R(c));
		SHUFPD(XMM0, R(XMM0), 3);
		break;
	default:
		PanicAlert("ps_muls WTF!!!");
	}
	Force25BitPrecision(XMM0, XMM1);
	MULPD(XMM0, fpr.R(a));
	ForceSinglePrecisionP(XMM0);
	SetFPRFIfNeeded(inst, XMM0);
	fpr.BindToRegister(d, false);
	MOVAPD(fpr.RX(d), R(XMM0));
	fpr.UnlockAll();
}


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
	MOVAPD(fpr.RX(d), R(XMM0));
	fpr.UnlockAll();
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	fpr.Lock(b, d);
	fpr.BindToRegister(b, true, false);
	fpr.BindToRegister(d, false);

	MOVSD(XMM0, fpr.R(b));
	CALL((void *)asm_routines.frsqrte);
	MOVSD(fpr.R(d), XMM0);

	MOVHLPS(XMM0, fpr.RX(b));
	CALL((void *)asm_routines.frsqrte);
	MOVLHPS(fpr.RX(d), XMM0);

	ForceSinglePrecisionP(fpr.RX(d));
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::ps_res(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	fpr.Lock(b, d);
	fpr.BindToRegister(b, true, false);
	fpr.BindToRegister(d, false);

	MOVSD(XMM0, fpr.R(b));
	CALL((void *)asm_routines.fres);
	MOVSD(fpr.R(d), XMM0);

	MOVHLPS(XMM0, fpr.RX(b));
	CALL((void *)asm_routines.fres);
	MOVLHPS(fpr.RX(d), XMM0);

	ForceSinglePrecisionP(fpr.RX(d));
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
	gpr.UnlockAllX();
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
	ForceSinglePrecisionP(XMM0);
	SetFPRFIfNeeded(inst, XMM0);
	MOVAPD(fpr.RX(d), R(XMM0));
	fpr.UnlockAll();
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(jo.fpAccurateFcmp);

	FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
