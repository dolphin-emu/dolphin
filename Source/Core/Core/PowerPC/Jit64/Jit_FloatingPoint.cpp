// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

static const u64 GC_ALIGNED16(psSignBits[2]) = {0x8000000000000000ULL, 0x0000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask[2])  = {0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
static const double GC_ALIGNED16(half_qnan_and_s32_max[2]) = {0x7FFFFFFF, -0x80000};

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single, void (XEmitter::*avxOp)(X64Reg, X64Reg, OpArg),
                      void (XEmitter::*sseOp)(X64Reg, OpArg), UGeckoInstruction inst, bool roundRHS)
{
	fpr.Lock(d, a, b);
	fpr.BindToRegister(d, d == a || d == b || !single);
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
		avx_op(avxOp, sseOp, fpr.RX(d), fpr.R(a), fpr.R(b), false, reversible);
	}
	if (single)
	{
		ForceSinglePrecisionS(fpr.RX(d));
		MOVDDUP(fpr.RX(d), fpr.R(d));
	}
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
}

// We can avoid calculating FPRF if it's not needed; every float operation resets it, so
// if it's going to be clobbered in a future instruction before being read, we can just
// not calculate it.
void Jit64::SetFPRFIfNeeded(UGeckoInstruction inst, X64Reg xmm)
{
	// As far as we know, the games that use this flag only need FPRF for fmul and fmadd, but
	// FPRF is fast enough in JIT that we might as well just enable it for every float instruction
	// if the enableFPRF flag is set.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableFPRF && js.op->wantsFPRF)
		SetFPRF(xmm);
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	bool single = inst.OPCD == 59;
	switch (inst.SUBOP5)
	{
	case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::VDIVSD, &XEmitter::DIVSD, inst); break; //div
	case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::VSUBSD, &XEmitter::SUBSD, inst); break; //sub
	case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true, single, &XEmitter::VADDSD, &XEmitter::ADDSD, inst); break; //add
	case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true, single, &XEmitter::VMULSD, &XEmitter::MULSD, inst, single); break; //mul
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith WTF!!!");
	}
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	bool single_precision = inst.OPCD == 59;

	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	int d = inst.FD;

	fpr.Lock(a, b, c, d);

	// nmsub is implemented a little differently ((b - a*c) instead of -(a*c - b)), so handle it separately
	if (inst.SUBOP5 == 30) //nmsub
	{
		if (single_precision)
			Force25BitPrecision(XMM1, fpr.R(c), XMM0);
		else
			MOVSD(XMM1, fpr.R(c));
		MULSD(XMM1, fpr.R(a));
		MOVSD(XMM0, fpr.R(b));
		SUBSD(XMM0, R(XMM1));
	}
	else
	{
		if (single_precision)
			Force25BitPrecision(XMM0, fpr.R(c), XMM1);
		else
			MOVSD(XMM0, fpr.R(c));
		MULSD(XMM0, fpr.R(a));
		if (inst.SUBOP5 == 28) //msub
			SUBSD(XMM0, fpr.R(b));
		else                   //(n)madd
			ADDSD(XMM0, fpr.R(b));
		if (inst.SUBOP5 == 31) //nmadd
			PXOR(XMM0, M((void*)&psSignBits));
	}
	fpr.BindToRegister(d, false);
	//YES it is necessary to dupe the result :(
	//TODO : analysis - does the top reg get used? If so, dupe, if not, don't.
	if (single_precision)
	{
		ForceSinglePrecisionS(XMM0);
		MOVDDUP(fpr.RX(d), R(XMM0));
	}
	else
	{
		MOVSD(fpr.RX(d), R(XMM0));
	}
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
}

void Jit64::fsign(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(b, d);
	fpr.BindToRegister(d, true, true);

	if (d != b)
		MOVSD(fpr.RX(d), fpr.R(b));
	switch (inst.SUBOP10)
	{
	case 40:  // fnegx
		// We can cheat and not worry about clobbering the top half by using masks
		// that don't modify the top half.
		PXOR(fpr.RX(d), M((void*)&psSignBits));
		break;
	case 264: // fabsx
		PAND(fpr.RX(d), M((void*)&psAbsMask));
		break;
	case 136: // fnabs
		POR(fpr.RX(d), M((void*)&psSignBits));
		break;
	default:
		PanicAlert("fsign bleh");
		break;
	}
	fpr.UnlockAll();
}

void Jit64::fselx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;

	fpr.Lock(a, b, c, d);
	MOVSD(XMM1, fpr.R(a));
	PXOR(XMM0, R(XMM0));
	// This condition is very tricky; there's only one right way to handle both the case of
	// negative/positive zero and NaN properly.
	// (a >= -0.0 ? c : b) transforms into (0 > a ? b : c), hence the NLE.
	CMPSD(XMM0, R(XMM1), NLE);
	if (cpu_info.bSSE4_1)
	{
		MOVSD(XMM1, fpr.R(c));
		BLENDVPD(XMM1, fpr.R(b));
	}
	else
	{
		MOVSD(XMM1, R(XMM0));
		PAND(XMM0, fpr.R(b));
		PANDN(XMM1, fpr.R(c));
		POR(XMM1, R(XMM0));
	}
	fpr.BindToRegister(d, true);
	MOVSD(fpr.RX(d), R(XMM1));
	fpr.UnlockAll();
}

void Jit64::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int b = inst.FB;

	if (d == b)
		return;

	fpr.Lock(b, d);

	if (fpr.IsBound(d))
	{
		// We don't need to load d, but if it is loaded, we need to mark it as dirty.
		fpr.BindToRegister(d);
		// We have to use MOVLPD if b isn't loaded because "MOVSD reg, mem" sets the upper bits (64+)
		// to zero and we don't want that.
		if (!fpr.R(b).IsSimpleReg())
			MOVLPD(fpr.RX(d), fpr.R(b));
		else
			MOVSD(fpr.R(d), fpr.RX(b));
	}
	else
	{
		fpr.BindToRegister(b, true, false);
		MOVSD(fpr.R(d), fpr.RX(b));
	}

	fpr.UnlockAll();
}

void Jit64::FloatCompare(UGeckoInstruction inst, bool upper)
{
	bool fprf = SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableFPRF && js.op->wantsFPRF;
	//bool ordered = !!(inst.SUBOP10 & 32);
	int a = inst.FA;
	int b = inst.FB;
	int crf = inst.CRFD;

	fpr.Lock(a, b);
	fpr.BindToRegister(b, true, false);

	if (fprf)
		AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));

	if (upper)
	{
		fpr.BindToRegister(a, true, false);
		MOVHLPS(XMM0, fpr.RX(a));
		MOVHLPS(XMM1, fpr.RX(b));
		UCOMISD(XMM1, R(XMM0));
	}
	else
	{
		// Are we masking sNaN invalid floating point exceptions? If not this could crash if we don't handle the exception?
		UCOMISD(fpr.RX(b), fpr.R(a));
	}

	FixupBranch pNaN, pLesser, pGreater;
	FixupBranch continue1, continue2, continue3;

	if (a != b)
	{
		// if B > A, goto Lesser's jump target
		pLesser = J_CC(CC_A);
	}

	// if (B != B) or (A != A), goto NaN's jump target
	pNaN = J_CC(CC_P);

	if (a != b)
	{
		// if B < A, goto Greater's jump target
		// JB can't precede the NaN check because it doesn't test ZF
		pGreater = J_CC(CC_B);
	}

	MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(CR_EQ)));
	if (fprf)
		OR(32, PPCSTATE(fpscr), Imm32(CR_EQ << FPRF_SHIFT));

	continue1 = J();

	SetJumpTarget(pNaN);
	MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(CR_SO)));
	if (fprf)
		OR(32, PPCSTATE(fpscr), Imm32(CR_SO << FPRF_SHIFT));

	if (a != b)
	{
		continue2 = J();

		SetJumpTarget(pGreater);
		MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(CR_GT)));
		if (fprf)
			OR(32, PPCSTATE(fpscr), Imm32(CR_GT << FPRF_SHIFT));
		continue3 = J();

		SetJumpTarget(pLesser);
		MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(CR_LT)));
		if (fprf)
			OR(32, PPCSTATE(fpscr), Imm32(CR_LT << FPRF_SHIFT));
	}

	SetJumpTarget(continue1);
	if (a != b)
	{
		SetJumpTarget(continue2);
		SetJumpTarget(continue3);
	}

	MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
	fpr.UnlockAll();
}

void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(jo.fpAccurateFcmp);

	FloatCompare(inst);
}

void Jit64::fctiwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.RD;
	int b = inst.RB;
	fpr.Lock(d, b);
	fpr.BindToRegister(d, d == b);

	// Intel uses 0x80000000 as a generic error code while PowerPC uses clamping:
	//
	// input       | output fctiw | output CVTPD2DQ
	// ------------+--------------+----------------
	// > +2^31 - 1 | 0x7fffffff   | 0x80000000
	// < -2^31     | 0x80000000   | 0x80000000
	// any NaN     | 0x80000000   | 0x80000000
	//
	// The upper 32 bits of the result are set to 0xfff80000,
	// except for -0.0 where they are set to 0xfff80001 (TODO).

	MOVAPD(XMM0, M(&half_qnan_and_s32_max));
	MINSD(XMM0, fpr.R(b));
	switch (inst.SUBOP10)
	{
		// fctiwx
		case 14:
			CVTPD2DQ(XMM0, R(XMM0));
			break;

		// fctiwzx
		case 15:
			CVTTPD2DQ(XMM0, R(XMM0));
			break;
	}
	// d[64+] must not be modified
	MOVSD(fpr.R(d), XMM0);
	fpr.UnlockAll();
}

void Jit64::frspx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	int b = inst.FB;
	int d = inst.FD;

	fpr.Lock(b, d);
	fpr.BindToRegister(d, d == b);
	if (b != d)
		MOVAPD(fpr.RX(d), fpr.R(b));
	ForceSinglePrecisionS(fpr.RX(d));
	MOVDDUP(fpr.RX(d), fpr.R(d));
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
}

void Jit64::frsqrtex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	fpr.Lock(b, d);
	fpr.BindToRegister(d, d == b);
	MOVSD(XMM0, fpr.R(b));
	CALL((void *)asm_routines.frsqrte);
	MOVSD(fpr.R(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::fresx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	fpr.Lock(b, d);
	fpr.BindToRegister(d, d == b);
	MOVSD(XMM0, fpr.R(b));
	CALL((void *)asm_routines.fres);
	MOVSD(fpr.R(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
	gpr.UnlockAllX();
}
