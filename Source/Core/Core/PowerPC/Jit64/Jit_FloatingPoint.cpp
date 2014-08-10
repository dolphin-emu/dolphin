// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

static const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask2[2])  = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
static const double GC_ALIGNED16(half_qnan_and_s32_max[2]) = {0x7FFFFFFF, -0x80000};

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg))
{
	fpr.Lock(d, a, b);
	if (d == a)
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
	fpr.UnlockAll();
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	// Only the interpreter has "proper" support for (some) FP flags
	FALLBACK_IF(inst.SUBOP5 == 25 && Core::g_CoreStartupParameter.bEnableFPRF);

	bool single = inst.OPCD == 59;
	switch (inst.SUBOP5)
	{
	case 18: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::DIVSD); break; //div
	case 20: fp_tri_op(inst.FD, inst.FA, inst.FB, false, single, &XEmitter::SUBSD); break; //sub
	case 21: fp_tri_op(inst.FD, inst.FA, inst.FB, true,  single, &XEmitter::ADDSD); break; //add
	case 25: fp_tri_op(inst.FD, inst.FA, inst.FC, true, single, &XEmitter::MULSD); break; //mul
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith WTF!!!");
	}
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	// Only the interpreter has "proper" support for (some) FP flags
	FALLBACK_IF(inst.SUBOP5 == 29 && Core::g_CoreStartupParameter.bEnableFPRF);

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
		PXOR(XMM0, M((void*)&psSignBits2));
		break;
	case 31: //nmadd
		MULSD(XMM0, fpr.R(c));
		ADDSD(XMM0, fpr.R(b));
		PXOR(XMM0, M((void*)&psSignBits2));
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
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int b = inst.FB;
	fpr.Lock(b, d);
	fpr.BindToRegister(d, true, true);
	MOVSD(XMM0, fpr.R(b));
	switch (inst.SUBOP10) {
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

// helper functions for picking the right CR value based on a set of possible flags
void Jit64::PickGTEQ()
{
	static const u64 cr[2] = { PPCCRToInternal(CR_GT), PPCCRToInternal(CR_EQ) };
	SETcc(CC_E, R(AL));
	MOVZX(32, 8, EAX, R(AL));
	MOV(64, R(RAX), MScaled(EAX, 8, (u32)(u64)cr));
}

void Jit64::PickLTEQ()
{
	static const u64 cr[2] = { PPCCRToInternal(CR_LT), PPCCRToInternal(CR_EQ) };
	SETcc(CC_E, R(AL));
	MOVZX(32, 8, EAX, R(AL));
	MOV(64, R(RAX), MScaled(EAX, 8, (u32)(u64)cr));
}

void Jit64::PickLTGT()
{
	static const u64 cr[2] = { PPCCRToInternal(CR_LT), PPCCRToInternal(CR_GT) };
	SETcc(CC_B, R(AL));
	MOVZX(32, 8, EAX, R(AL));
	MOV(64, R(RAX), MScaled(EAX, 8, (u32)(u64)cr));
}

void Jit64::PickGTSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_GT)));
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickEQSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickLTSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_LT)));
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickGTEQSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	PickGTEQ();
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickLTEQSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	PickLTEQ();
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickLTGTSO(FixupBranch *pBranchNan)
{
	FixupBranch continue1;
	PickLTGT();
	continue1 = J();
	SetJumpTarget(*pBranchNan);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
	SetJumpTarget(continue1);
}

void Jit64::PickLTGTEQ()
{
	// probably not very important, so we'll just do it with branches
	FixupBranch pLesser, pGreater, continue1, continue2;
	pLesser = J_CC(CC_A);
	pGreater = J_CC(CC_B);

	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
	continue1 = J();

	SetJumpTarget(pGreater);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_GT)));
	continue2 = J();

	SetJumpTarget(pLesser);
	MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_LT)));

	SetJumpTarget(continue1);
	SetJumpTarget(continue2);
}

void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(jo.fpAccurateFcmp);

	//bool ordered = inst.SUBOP10 == 32;
	int a   = inst.FA;
	int b   = inst.FB;
	int crf = inst.CRFD;

	bool merge_branch = false;
	int test_crf = js.next_inst.BI >> 2;

	// Check if the next instruction is a branch - if it is, merge the two.
	if (((js.next_inst.OPCD == 16 /* bcx */) ||
		((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 528) /* bcctrx */) ||
		((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 16) /* bclrx */)) &&
		(js.next_inst.BO & BO_DONT_DECREMENT_FLAG) &&
		!(js.next_inst.BO & BO_DONT_CHECK_CONDITION)) {
		// Looks like a decent conditional branch that we can merge with.
		// It only test CR, not CTR.
		if (test_crf == crf) {
			merge_branch = true;
		}
	}

	fpr.Lock(a,b);
	fpr.BindToRegister(b, true, false);

	// Are we masking sNaN invalid floating point exceptions? If not this could crash if we don't handle the exception?
	UCOMISD(fpr.R(b).GetSimpleReg(), fpr.R(a));

	if (merge_branch)
	{
		js.downcountAmount++;
		js.skipnext = true;
		int test_bit = 8 >> (js.next_inst.BI & 3);
		bool condition = js.next_inst.BO & BO_BRANCH_IF_TRUE;

		FixupBranch pDontBranch, pDontBranchNaN;

		gpr.UnlockAll();
		fpr.UnlockAll();
		gpr.Flush(FLUSH_MAINTAIN_STATE);
		fpr.Flush(FLUSH_MAINTAIN_STATE);

		// handle the taken path. keep in mind all the x86 conditions (A/B) are reversed because
		// the register order is backwards (as in the non-merged path).
		ERROR_LOG(COMMON, "%x %d", test_bit, condition);
		if (test_bit & 8)
		{
			if (condition)
			{
				// less than, so jump over path has to handle CR_GT or CR_EQ or CR_SO
				// and the taken path has to handle CR_LT
				pDontBranchNaN = J_CC(CC_P);
				pDontBranch = J_CC(CC_BE);
				MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_LT)));
			}
			else
			{
				// greater than or equal, so jump over path has to handle CR_LT or CR_SO
				// and the taken path has to handle CR_GT or CR_EQ
				if (a != b)
				{
					pDontBranch = J_CC(CC_A);
					// we can put the NaN check after because it doesn't conflict with the NE branch
					pDontBranchNaN = J_CC(CC_P);
					PickGTEQ();
				}
				else
				{
					pDontBranch = J_CC(CC_P);
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
				}
			}
		}
		else if (test_bit & 4)
		{
			if (condition)
			{
				// greater than, so jump over path has to handle CR_LT or CR_EQ or CR_SO
				// and the taken path has to handle CR_GT
				pDontBranchNaN = J_CC(CC_P);
				pDontBranch = J_CC(CC_AE);
				MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_GT)));
			}
			else
			{
				// less than or equal, so jump over path has to handle CR_GT or CR_SO
				// and the taken path has to handle CR_LT or CR_EQ
				if (a != b)
				{
					pDontBranchNaN = J_CC(CC_P);
					pDontBranch = J_CC(CC_B);
					PickLTEQ();
				}
				else
				{
					pDontBranch = J_CC(CC_P);
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
				}
			}
		}
		else if (test_bit & 2)
		{
			if (condition)
			{
				// equal to, so jump over path has to handle CR_LT or CR_GT or CR_SO
				// and the taken path has to handle CR_EQ
				if (a != b)
				{
					pDontBranch = J_CC(CC_NE);
					// we can put the NaN check after because it doesn't conflict with the NE branch
					pDontBranchNaN = J_CC(CC_P);
				}
				else
				{
					pDontBranch = J_CC(CC_P);
				}
				MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
			}
			else
			{
				// not equal to, so jump over path has to handle CR_EQ or CR_SO
				// and the taken path has to handle CR_LT or CR_GT
				pDontBranchNaN = J_CC(CC_P);
				pDontBranch = J_CC(CC_E);
				if (a != b)
					PickLTGT();
			}
		}
		else
		{
			if (condition)
			{
				// NaN, so jump over path has to handle CR_LT or CR_GT or CR_EQ
				// and the taken path has to handle CR_SO
				pDontBranch = J_CC(CC_NP);
				MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
			}
			else
			{
				// not NaN, so jump over path has to handle CR_SO
				// and the taken path has to handle CR_LT or CR_GT or CR_EQ
				pDontBranch = J_CC(CC_P);
				if (a != b)
					PickLTGTEQ();
				else
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
			}
		}
		MOV(64, M(&PowerPC::ppcState.cr_val[crf]), R(RAX));

		DoMergedBranch();

		SetJumpTarget(pDontBranch);

		// handle the untaken path
		if (test_bit & 8)
		{
			if (condition)
			{
				if (a != b)
					PickGTEQSO(&pDontBranchNaN);
				else
					PickEQSO(&pDontBranchNaN);
			}
			else
			{
				if (a != b)
					PickLTSO(&pDontBranchNaN);
				else
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
			}
		}
		else if (test_bit & 4)
		{
			if (condition)
			{
				if (a != b)
					PickLTEQSO(&pDontBranchNaN);
				else
					PickEQSO(&pDontBranchNaN);
			}
			else
			{
				if (a != b)
					PickGTSO(&pDontBranchNaN);
				else
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
			}
		}
		else if (test_bit & 2)
		{
			if (condition)
			{
				if (a != b)
					PickLTGTSO(&pDontBranchNaN);
				else
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
			}
			else
			{
				PickEQSO(&pDontBranchNaN);
			}
		}
		else
		{
			if (condition)
			{
				if (a != b)
					PickLTGTEQ();
				else
					MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
			}
			else
			{
				MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));
			}
		}
		MOV(64, M(&PowerPC::ppcState.cr_val[crf]), R(RAX));
	}
	else
	{
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

		MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_EQ)));
		continue1 = J();

		SetJumpTarget(pNaN);
		MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_SO)));

		if (a != b)
		{
			continue2 = J();

			SetJumpTarget(pGreater);
			MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_GT)));
			continue3 = J();

			SetJumpTarget(pLesser);
			MOV(64, R(RAX), Imm64(PPCCRToInternal(CR_LT)));
		}

		SetJumpTarget(continue1);
		if (a != b)
		{
			SetJumpTarget(continue2);
			SetJumpTarget(continue3);
		}

		MOV(64, M(&PowerPC::ppcState.cr_val[crf]), R(RAX));
		fpr.UnlockAll();
	}
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
