// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

static const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask2[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
static const double GC_ALIGNED16(half_qnan_and_s32_max[2]) = {0x7FFFFFFF, -0x80000};

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg), UGeckoInstruction inst, bool roundRHS)
{
	fpr.Lock(d, a, b);
	if (roundRHS)
	{
		if (d == a)
		{
			fpr.BindToRegister(d, true);
			MOVSD(XMM0, fpr.R(b));
			Force25BitPrecision(XMM0, XMM1);
			(this->*op)(fpr.RX(d), R(XMM0));
		}
		else
		{
			fpr.BindToRegister(d, d == b);
			if (d != b)
				MOVSD(fpr.RX(d), fpr.R(b));
			Force25BitPrecision(fpr.RX(d), XMM0);
			(this->*op)(fpr.RX(d), fpr.R(a));
		}
	}
	else if (d == a)
	{
		fpr.BindToRegister(d, true);
		if (!single)
		{
			fpr.BindToRegister(b, true, false);
		}
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	else if (d == b)
	{
		if (reversible)
		{
			fpr.BindToRegister(d, true);
			if (!single)
			{
				fpr.BindToRegister(a, true, false);
			}
			(this->*op)(fpr.RX(d), fpr.R(a));
		}
		else
		{
			MOVSD(XMM0, fpr.R(b));
			fpr.BindToRegister(d, !single);
			MOVSD(fpr.RX(d), fpr.R(a));
			(this->*op)(fpr.RX(d), Gen::R(XMM0));
		}
	}
	else
	{
		// Sources different from d, can use rather quick solution
		fpr.BindToRegister(d, !single);
		if (!single)
		{
			fpr.BindToRegister(b, true, false);
		}
		MOVSD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	if (single)
	{
		ForceSinglePrecisionS(fpr.RX(d));
		if (cpu_info.bSSE3)
		{
			MOVDDUP(fpr.RX(d), fpr.R(d));
		}
		else
		{
			if (!fpr.R(d).IsSimpleReg(fpr.RX(d)))
				MOVQ_xmm(fpr.RX(d), fpr.R(d));
			UNPCKLPD(fpr.RX(d), R(fpr.RX(d)));
		}
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
	case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::DIVSD, inst); break; //div
	case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::SUBSD, inst); break; //sub
	case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true, single, &XEmitter::ADDSD, inst); break; //add
	case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true, single, &XEmitter::MULSD, inst, single); break; //mul
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
	MOVSD(XMM0, fpr.R(c));
	if (single_precision)
		Force25BitPrecision(XMM0, XMM1);
	switch (inst.SUBOP5)
	{
	case 28: //msub
		MULSD(XMM0, fpr.R(a));
		SUBSD(XMM0, fpr.R(b));
		break;
	case 29: //madd
		MULSD(XMM0, fpr.R(a));
		ADDSD(XMM0, fpr.R(b));
		break;
	case 30: //nmsub
		MULSD(XMM0, fpr.R(a));
		SUBSD(XMM0, fpr.R(b));
		PXOR(XMM0, M((void*)&psSignBits2));
		break;
	case 31: //nmadd
		MULSD(XMM0, fpr.R(a));
		ADDSD(XMM0, fpr.R(b));
		PXOR(XMM0, M((void*)&psSignBits2));
		break;
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
	MOVSD(XMM0, fpr.R(b));
	switch (inst.SUBOP10)
	{
	case 40:  // fnegx
		PXOR(XMM0, M((void*)&psSignBits2));
		break;
	case 264: // fabsx
		PAND(XMM0, M((void*)&psAbsMask2));
		break;
	case 136: // fnabs
		POR(XMM0, M((void*)&psSignBits2));
		break;
	default:
		PanicAlert("fsign bleh");
		break;
	}
	MOVSD(fpr.R(d), XMM0);
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

	// We don't need to load d, but if it is loaded, we need to mark it as dirty.
	if (fpr.IsBound(d))
		fpr.BindToRegister(d);

	// b needs to be in a register because "MOVSD reg, mem" sets the upper bits (64+) to zero and we don't want that.
	fpr.BindToRegister(b, true, false);

	MOVSD(fpr.R(d), fpr.RX(b));

	fpr.UnlockAll();
}

void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	// FIXME: accurate fcmp is currently set if FPRF is on, so the FPRF path never gets taken.
	// Whether the non-FPRF aspects of interpreter fcmp are actually needed should be tested.
	FALLBACK_IF(jo.fpAccurateFcmp);

	//bool ordered = inst.SUBOP10 == 32;
	int a   = inst.FA;
	int b   = inst.FB;
	int crf = inst.CRFD;
	bool fprf = SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableFPRF && js.op->wantsFPRF;

	fpr.Lock(a, b);

	// These tables don't hurt cache as much as one would think, since EQ/SO are pretty rare for floats,
	// and the middle sections are never accessed.
	// EFLAGS = [SF(0),ZF,0,AF(0),0,PF,1,CF]
	static const u8 FlagsToFPRF[68] =
	{
		CR_LT, CR_GT,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		CR_EQ, 0, 0, CR_SO
	};
	static const u64 FlagsToCR[68] =
	{
		PPCCRToInternal(CR_LT), PPCCRToInternal(CR_GT),
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		PPCCRToInternal(CR_EQ), 0, 0, PPCCRToInternal(CR_SO)
	};

	fpr.BindToRegister(b, true, false);

	// Are we masking sNaN invalid floating point exceptions? If not this could crash if we don't handle the exception?
	UCOMISD(fpr.RX(b), fpr.R(a));
	if (!cpu_info.bLAHFSAHF64)
	{
		// Probably way slower than it needs to be, but very few CPUs don't have LAHF support, so let's not
		// worry too much about them.
		SETcc(CC_Z, R(RSCRATCH));
		SETcc(CC_P, R(RSCRATCH2));
		SETcc(CC_C, R(AH));					// CF
		SHL(8, R(RSCRATCH), Imm8(6));		// ZF
		SHL(8, R(RSCRATCH2), Imm8(2));		// PF
		OR(8, R(RSCRATCH), R(AH));
		OR(8, R(RSCRATCH), R(RSCRATCH2));
		MOVZX(32, 8, RSCRATCH, R(RSCRATCH));
	}
	else
	{
		LAHF();
		MOVZX(32, 8, RSCRATCH, R(AH));
	}
	MOV(64, R(RSCRATCH2), MScaled(RSCRATCH, SCALE_8, (u32)(u64)FlagsToCR - (cpu_info.bLAHFSAHF64 ? 16 : 0)));
	MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH2));
	if (fprf)
	{
		MOV(64, R(RSCRATCH2), MDisp(RSCRATCH, (u32)(u64)FlagsToFPRF - (cpu_info.bLAHFSAHF64 ? 16 : 0)));
		AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));
		SHL(32, R(RSCRATCH), Imm8(FPRF_SHIFT));
		OR(32, PPCSTATE(fpscr), R(RSCRATCH));
	}
	fpr.UnlockAll();
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
