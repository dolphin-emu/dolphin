// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "CPUDetect.h"

static const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask2[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
static const double GC_ALIGNED16(psOneOne2[2]) = {1.0, 1.0};
static const double one_const = 1.0f;

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single,
		      void (XEmitter::*op_2)(Gen::X64Reg, Gen::OpArg),
		      void (XEmitter::*op_3)(Gen::X64Reg, Gen::X64Reg, Gen::OpArg))
{
	if (!cpu_info.bAVX)
	{
		op_3 = nullptr;
	}

	fpr.Lock(d, a, b);
	if (d == a)
	{
		fpr.BindToRegister(d);
		(this->*op_2)(fpr.RX(d), fpr.R(b));
	}
	else if (d == b)
	{
		if (reversible)
		{
			fpr.BindToRegister(d);
			(this->*op_2)(fpr.RX(d), fpr.R(a));
		}
		else
		{
			if (op_3)
			{
				fpr.BindToRegister(d);
				fpr.BindToRegister(a, true, false);
				(this->*op_3)(fpr.RX(d), fpr.RX(a), fpr.R(b));
			}
			else
			{
				MOVSD(XMM0, fpr.R(b));
				fpr.BindToRegister(d, false);
				MOVSD(fpr.RX(d), fpr.R(a));
				(this->*op_2)(fpr.RX(d), Gen::R(XMM0));
			}
		}
	}
	else
	{
		if (op_3)
		{
			fpr.BindToRegister(d, false);
			fpr.BindToRegister(a);
			(this->*op_3)(fpr.RX(d), fpr.RX(a), fpr.R(b));
		}
		else
		{
			fpr.BindToRegister(d, false);
			MOVSD(fpr.RX(d), fpr.R(a));
			(this->*op_2)(fpr.RX(d), fpr.R(b));
		}
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
	fpr.UnlockAll();
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	if (inst.Rc) {
		Default(inst); return;
	}

	// Only the interpreter has "proper" support for (some) FP flags
	if (inst.SUBOP5 == 25 && Core::g_CoreStartupParameter.bEnableFPRF) {
		Default(inst); return;
	}

	bool single = inst.OPCD == 59;
	switch (inst.SUBOP5)
	{
	case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::DIVSD, &XEmitter::VDIVSD); break; //div
	case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::SUBSD, &XEmitter::VSUBSD); break; //sub
	case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true,  single, &XEmitter::ADDSD, &XEmitter::VADDSD); break; //add
	case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true,  single, &XEmitter::MULSD, &XEmitter::VMULSD); break; //mul
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith WTF!!!");
	}
}

void Jit64::frsqrtex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(b, d);
	fpr.BindToRegister(d, d == b, true);
	MOVSD(XMM0, M((void *)&one_const));
	SQRTSD(XMM1, fpr.R(b));
	if (cpu_info.bAVX)
	{
		VDIVSD(fpr.RX(d), XMM0, R(XMM1));
	}
	else
	{
		DIVSD(XMM0, R(XMM1));
		MOVSD(fpr.R(d), XMM0);
	}
	fpr.UnlockAll();
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	if (inst.Rc) {
		Default(inst); return;
	}
	// Only the interpreter has "proper" support for (some) FP flags
	if (inst.SUBOP5 == 29 && Core::g_CoreStartupParameter.bEnableFPRF) {
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
	fpr.BindToRegister(d, false);
	//YES it is necessary to dupe the result :(
	//TODO : analysis - does the top reg get used? If so, dupe, if not, don't.
	if (single_precision) {
		ForceSinglePrecisionS(XMM0);
		MOVDDUP(fpr.RX(d), R(XMM0));
	} else {
		MOVSD(fpr.RX(d), R(XMM0));
	}
	// SMB checks flags after this op. Let's lie.
	//AND(32, M(&PowerPC::ppcState.fpscr), Imm32(~((0x80000000 >> 19) | (0x80000000 >> 15))));
	//OR(32, M(&PowerPC::ppcState.fpscr), Imm32((0x80000000 >> 16)));
	fpr.UnlockAll();
}

void Jit64::fsign(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	if (inst.Rc) {
		Default(inst); return;
	}

	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(b, d);
	fpr.BindToRegister(d, true, true);
	MOVSD(XMM0, fpr.R(b));
	switch (inst.SUBOP10) {
	case 40:  // fnegx
		XORPD(XMM0, M((void*)&psSignBits2));
		break;
	case 264: // fabsx
		ANDPD(XMM0, M((void*)&psAbsMask2));
		break;
	case 136: // fnabs
		ORPD(XMM0, M((void*)&psSignBits2));
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
	JITDISABLE(bJITFloatingPointOff)
	if (inst.Rc)
	{
		Default(inst); return;
	}
	int d = inst.FD;
	int b = inst.FB;
	if (d != b)
	{
		fpr.Lock(b, d);

		// we don't need to load d, but if it already is, it must be marked as dirty
		if (fpr.IsBound(d))
		{
			fpr.BindToRegister(d);
		}
		fpr.BindToRegister(b, true, false);

		// caveat: the order of ModRM:r/m, ModRM:reg is deliberate!
		// "MOVSD reg, mem" zeros out the upper half of the destination register
		MOVSD(fpr.R(d), fpr.RX(b));
		fpr.UnlockAll();
	}
}

void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	if (jo.fpAccurateFcmp) {
		Default(inst); return; // turn off from debugger
	}

	//bool ordered = inst.SUBOP10 == 32;
	int a	= inst.FA;
	int b	= inst.FB;
	int crf	= inst.CRFD;

	fpr.Lock(a,b);
	fpr.BindToRegister(b, true);

	// Are we masking sNaN invalid floating point exceptions? If not this could crash if we don't handle the exception?
	UCOMISD(fpr.R(b).GetSimpleReg(), fpr.R(a));

	FixupBranch pNaN, pLesser, pGreater;
	FixupBranch continue1, continue2, continue3;

	if (a != b)
	{
		// if B > A, goto Lesser's jump target
		pLesser  = J_CC(CC_A);
	}

	// if (B != B) or (A != A), goto NaN's jump target
	pNaN    	 = J_CC(CC_P);

	if (a != b)
	{
		// if B < A, goto Greater's jump target
		// JB can't precede the NaN check because it doesn't test ZF
		pGreater = J_CC(CC_B);
	}

	// Equal
	MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x2));
	continue1 = J();

	// NAN
	SetJumpTarget(pNaN);
	MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x1));

	if (a != b)
	{
		continue2 = J();

		// Greater Than
		SetJumpTarget(pGreater);
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x4));
		continue3 = J();

		// Less Than
		SetJumpTarget(pLesser);
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x8));
	}

	SetJumpTarget(continue1);
	if (a != b)
	{
		SetJumpTarget(continue2);
		SetJumpTarget(continue3);
	}

	fpr.UnlockAll();
}


