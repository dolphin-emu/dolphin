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
		PXOR(XMM0, R(XMM0));
		CMPPD(XMM0, fpr.R(a), NLE);
		MOVAPD(XMM1, fpr.R(c));
		BLENDVPD(XMM1, fpr.R(b));
	}
	else
	{
		PXOR(XMM1, R(XMM1));
		CMPPD(XMM1, fpr.R(a), NLE);
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
	fpr.BindToRegister(d, d == b);

	switch (inst.SUBOP10)
	{
	case 40: //neg
		avx_op(&XEmitter::VPXOR, &XEmitter::PXOR, fpr.RX(d), fpr.R(b), M((void*)&psSignBits));
		break;
	case 136: //nabs
		avx_op(&XEmitter::VPOR, &XEmitter::POR, fpr.RX(d), fpr.R(b), M((void*)&psSignBits));
		break;
	case 264: //abs
		avx_op(&XEmitter::VPAND, &XEmitter::PAND, fpr.RX(d), fpr.R(b), M((void*)&psAbsMask));
		break;
	}

	fpr.UnlockAll();
}

//There's still a little bit more optimization that can be squeezed out of this
void Jit64::tri_op(int d, int a, int b, bool reversible, void (XEmitter::*avxOp)(X64Reg, X64Reg, OpArg), void (XEmitter::*sseOp)(X64Reg, OpArg), UGeckoInstruction inst, bool roundRHS)
{
	fpr.Lock(d, a, b);
	fpr.BindToRegister(d, d == a || d == b);

	if (roundRHS)
	{
		if (d == a)
		{
			Force25BitPrecision(XMM0, fpr.R(b), XMM1);
			(this->*sseOp)(fpr.RX(d), R(XMM0));
		}
		else
		{
			Force25BitPrecision(fpr.RX(d), fpr.R(b), XMM0);
			(this->*sseOp)(fpr.RX(d), fpr.R(a));
		}
	}
	else
	{
		avx_op(avxOp, sseOp, fpr.RX(d), fpr.R(a), fpr.R(b), true, reversible);
	}
	ForceSinglePrecisionP(fpr.RX(d), fpr.RX(d));
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
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::VDIVPD, &XEmitter::DIVPD, inst);
		break;
	case 20: // sub
		tri_op(inst.FD, inst.FA, inst.FB, false, &XEmitter::VSUBPD, &XEmitter::SUBPD, inst);
		break;
	case 21: // add
		tri_op(inst.FD, inst.FA, inst.FB, true, &XEmitter::VADDPD, &XEmitter::ADDPD, inst);
		break;
	case 25: // mul
		tri_op(inst.FD, inst.FA, inst.FC, true, &XEmitter::VMULPD, &XEmitter::MULPD, inst, true);
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
	fpr.BindToRegister(d, false);
	ForceSinglePrecisionP(fpr.RX(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
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
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM0, fpr.R(c), fpr.R(c), 3);
		break;
	default:
		PanicAlert("ps_muls WTF!!!");
	}
	Force25BitPrecision(XMM0, R(XMM0), XMM1);
	MULPD(XMM0, fpr.R(a));
	fpr.BindToRegister(d, false);
	ForceSinglePrecisionP(fpr.RX(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
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
	fpr.BindToRegister(d, d == a || d == b);

	switch (inst.SUBOP10)
	{
	case 528:
		avx_op(&XEmitter::VUNPCKLPD, &XEmitter::UNPCKLPD, fpr.RX(d), fpr.R(a), fpr.R(b));
		break; //00
	case 560:
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, fpr.RX(d), fpr.R(a), fpr.R(b), 2);
		break; //01
	case 592:
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, fpr.RX(d), fpr.R(a), fpr.R(b), 1);
		break; //10
	case 624:
		avx_op(&XEmitter::VUNPCKHPD, &XEmitter::UNPCKHPD, fpr.RX(d), fpr.R(a), fpr.R(b));
		break; //11
	default:
		_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
	}
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

	ForceSinglePrecisionP(fpr.RX(d), fpr.RX(d));
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

	ForceSinglePrecisionP(fpr.RX(d), fpr.RX(d));
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
	bool fma = cpu_info.bFMA && !Core::g_want_determinism;
	fpr.Lock(a,b,c,d);

	if (fma)
		fpr.BindToRegister(b, true, false);

	if (inst.SUBOP5 == 14)
	{
		MOVDDUP(XMM0, fpr.R(c));
		Force25BitPrecision(XMM0, R(XMM0), XMM1);
	}
	else if (inst.SUBOP5 == 15)
	{
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM0, fpr.R(c), fpr.R(c), 3);
		Force25BitPrecision(XMM0, R(XMM0), XMM1);
	}
	else
	{
		Force25BitPrecision(XMM0, fpr.R(c), XMM1);
	}

	if (fma)
	{
		switch (inst.SUBOP5)
		{
		case 14: //madds0
		case 15: //madds1
		case 29: //madd
			VFMADD132PD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		case 28: //msub
			VFMSUB132PD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		case 30: //nmsub
			VFNMADD132PD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		case 31: //nmadd
			VFNMSUB132PD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		}
	}
	else
	{
		switch (inst.SUBOP5)
		{
		case 14: //madds0
		case 15: //madds1
		case 29: //madd
			MULPD(XMM0, fpr.R(a));
			ADDPD(XMM0, fpr.R(b));
			break;
		case 28: //msub
			MULPD(XMM0, fpr.R(a));
			SUBPD(XMM0, fpr.R(b));
			break;
		case 30: //nmsub
			MULPD(XMM0, fpr.R(a));
			SUBPD(XMM0, fpr.R(b));
			PXOR(XMM0, M((void*)&psSignBits));
			break;
		case 31: //nmadd
			MULPD(XMM0, fpr.R(a));
			ADDPD(XMM0, fpr.R(b));
			PXOR(XMM0, M((void*)&psSignBits));
			break;
		default:
			_assert_msg_(DYNA_REC, 0, "ps_maddXX WTF!!!");
			return;
		}
	}

	fpr.BindToRegister(d, false);
	ForceSinglePrecisionP(fpr.RX(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(jo.fpAccurateFcmp);

	FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
