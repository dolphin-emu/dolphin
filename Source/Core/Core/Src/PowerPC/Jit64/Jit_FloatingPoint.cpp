// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "CPUDetect.h"

const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
const u64 GC_ALIGNED16(psAbsMask2[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
const double GC_ALIGNED16(psOneOne2[2]) = {1.0, 1.0};

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg))
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
		// Sources different from d, can use rather quick solution
		fpr.BindToRegister(d, !dupe);
		MOVSD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), fpr.R(b));
	}
	else if (b != d)
	{
		fpr.BindToRegister(d, !dupe);
		MOVSD(XMM0, fpr.R(b));
		MOVSD(fpr.RX(d), fpr.R(a));
		(this->*op)(fpr.RX(d), Gen::R(XMM0));
	}
	else // Other combo, must use two temps :(
	{
		MOVSD(XMM0, fpr.R(a));
		MOVSD(XMM1, fpr.R(b));
		fpr.BindToRegister(d, !dupe);
		(this->*op)(XMM0, Gen::R(XMM1));
		MOVSD(fpr.RX(d), Gen::R(XMM0));
	}
	if (dupe)
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


static const double one_const = 1.0f;

void Jit64::fp_arith_s(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
	if (inst.Rc) {
		Default(inst); return;
	}

	// Causing problems for GC - Starfox Assault (invisible boss at the end of level 1)
	if (inst.SUBOP5 == 21) {
		Default(inst); return;
	}	

	if (inst.SUBOP5 == 26) {
		// frsqrtex
		int d = inst.FD;
		int b = inst.FB;
		fpr.Lock(b, d);
		fpr.BindToRegister(d, true, true);
		MOVSD(XMM0, M((void *)&one_const));
		SQRTSD(XMM1, fpr.R(b));
		DIVSD(XMM0, R(XMM1));
		MOVSD(fpr.R(d), XMM0);
		fpr.UnlockAll();
		return;
	}

	if (inst.SUBOP5 != 18 && inst.SUBOP5 != 20 && inst.SUBOP5 != 21 &&
	    inst.SUBOP5 != 25) {
		Default(inst); return;
	}

	// Only the interpreter has "proper" support for (some) FP flags
	if (inst.SUBOP5 == 25 && Core::g_CoreStartupParameter.bEnableFPRF) {
		Default(inst); return;
	}

	bool dupe = inst.OPCD == 59;
	switch (inst.SUBOP5)
	{
	case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, dupe, &XEmitter::DIVSD); break; //div
	case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, dupe, &XEmitter::SUBSD); break; //sub
	case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true,  dupe, &XEmitter::ADDSD); break; //add
	case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true, dupe, &XEmitter::MULSD); break; //mul
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith_s WTF!!!");
	}
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
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
	JITDISABLE(FloatingPoint)
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
	JITDISABLE(FloatingPoint)
	if (inst.Rc) {
		Default(inst); return;
	}
	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(b, d);
	fpr.BindToRegister(d, true, true);
	MOVSD(XMM0, fpr.R(b));
	MOVSD(fpr.R(d), XMM0);
	fpr.UnlockAll();
}
 
void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
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


