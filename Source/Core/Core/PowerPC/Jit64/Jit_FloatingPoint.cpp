// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

static const u64 GC_ALIGNED16(psSignBits[2]) = {0x8000000000000000ULL, 0x0000000000000000ULL};
static const u64 GC_ALIGNED16(psSignBits2[2]) = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const u64 GC_ALIGNED16(psAbsMask[2])  = {0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
static const double GC_ALIGNED16(half_qnan_and_s32_max[2]) = {0x7FFFFFFF, -0x80000};

void Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single, void (XEmitter::*avxOp)(X64Reg, X64Reg, OpArg),
                      void (XEmitter::*sseOp)(X64Reg, OpArg), UGeckoInstruction inst, bool packed, bool roundRHS)
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
		avx_op(avxOp, sseOp, fpr.RX(d), fpr.R(a), fpr.R(b), packed, reversible);
	}
	if (single)
	{
		if (packed)
		{
			ForceSinglePrecisionP(fpr.RX(d), fpr.RX(d));
		}
		else
		{
			ForceSinglePrecisionS(fpr.RX(d), fpr.RX(d));
			MOVDDUP(fpr.RX(d), fpr.R(d));
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
	// if the FPRF flag is set.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFPRF && js.op->wantsFPRF)
		SetFPRF(xmm);
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	int d = inst.FD;
	int arg2 = inst.SUBOP5 == 25 ? c : b;

	bool single = inst.OPCD == 59;
	bool round_input = single && !jit->js.op->fprIsSingle[inst.FC];
	// If both the inputs are known to have identical top and bottom halves, we can skip the MOVDDUP at the end by
	// using packed arithmetic instead.
	bool packed = single && jit->js.op->fprIsDuplicated[a] && jit->js.op->fprIsDuplicated[arg2];
	// Packed divides are slower than scalar divides on basically all x86, so this optimization isn't worth it in that case.
	// Atoms (and a few really old CPUs) are also slower on packed operations than scalar ones.
	if (inst.SUBOP5 == 18 || cpu_info.bAtom)
		packed = false;

	switch (inst.SUBOP5)
	{
	case 18: fp_tri_op(d, a, b, false, single, packed ? &XEmitter::VDIVPD : &XEmitter::VDIVSD,
	                   packed ? &XEmitter::DIVPD : &XEmitter::DIVSD, inst, packed); break;
	case 20: fp_tri_op(d, a, b, false, single, packed ? &XEmitter::VSUBPD : &XEmitter::VSUBSD,
	                   packed ? &XEmitter::SUBPD : &XEmitter::SUBSD, inst, packed); break;
	case 21: fp_tri_op(d, a, b, true, single, packed ? &XEmitter::VADDPD : &XEmitter::VADDSD,
	                   packed ? &XEmitter::ADDPD : &XEmitter::ADDSD, inst, packed); break;
	case 25: fp_tri_op(d, a, c, true, single, packed ? &XEmitter::VMULPD : &XEmitter::VMULSD,
	                   packed ? &XEmitter::MULPD : &XEmitter::MULSD, inst, packed, round_input); break;
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith WTF!!!");
	}
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;
	int d = inst.FD;
	bool single = inst.OPCD == 59;
	bool round_input = single && !jit->js.op->fprIsSingle[c];
	bool packed = single && jit->js.op->fprIsDuplicated[a] && jit->js.op->fprIsDuplicated[b] && jit->js.op->fprIsDuplicated[c];
	if (cpu_info.bAtom)
		packed = false;

	fpr.Lock(a, b, c, d);

	// While we don't know if any games are actually affected (replays seem to work with all the usual
	// suspects for desyncing), netplay and other applications need absolute perfect determinism, so
	// be extra careful and don't use FMA, even if in theory it might be okay.
	// Note that FMA isn't necessarily less correct (it may actually be closer to correct) compared
	// to what the Gekko does here; in deterministic mode, the important thing is multiple Dolphin
	// instances on different computers giving identical results.
	if (cpu_info.bFMA && !Core::g_want_determinism)
	{
		if (single && round_input)
			Force25BitPrecision(XMM0, fpr.R(c), XMM1);
		else
			MOVAPD(XMM0, fpr.R(c));
		// Statistics suggests b is a lot less likely to be unbound in practice, so
		// if we have to pick one of a or b to bind, let's make it b.
		fpr.BindToRegister(b, true, false);
		switch (inst.SUBOP5)
		{
		case 28: //msub
			if (packed)
				VFMSUB132PD(XMM0, fpr.RX(b), fpr.R(a));
			else
				VFMSUB132SD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		case 29: //madd
			if (packed)
				VFMADD132PD(XMM0, fpr.RX(b), fpr.R(a));
			else
				VFMADD132SD(XMM0, fpr.RX(b), fpr.R(a));
			break;
			// PowerPC and x86 define NMADD/NMSUB differently
			// x86: D = -A*C (+/-) B
			// PPC: D = -(A*C (+/-) B)
			// so we have to swap them; the ADD/SUB here isn't a typo.
		case 30: //nmsub
			if (packed)
				VFNMADD132PD(XMM0, fpr.RX(b), fpr.R(a));
			else
				VFNMADD132SD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		case 31: //nmadd
			if (packed)
				VFNMSUB132PD(XMM0, fpr.RX(b), fpr.R(a));
			else
				VFNMSUB132SD(XMM0, fpr.RX(b), fpr.R(a));
			break;
		}
	}
	else if (inst.SUBOP5 == 30) //nmsub
	{
		// nmsub is implemented a little differently ((b - a*c) instead of -(a*c - b)), so handle it separately
		if (single && round_input)
			Force25BitPrecision(XMM1, fpr.R(c), XMM0);
		else
			MOVAPD(XMM1, fpr.R(c));
		MOVAPD(XMM0, fpr.R(b));
		if (packed)
		{
			MULPD(XMM1, fpr.R(a));
			SUBPD(XMM0, R(XMM1));
		}
		else
		{
			MULSD(XMM1, fpr.R(a));
			SUBSD(XMM0, R(XMM1));
		}
	}
	else
	{
		if (single && round_input)
			Force25BitPrecision(XMM0, fpr.R(c), XMM1);
		else
			MOVAPD(XMM0, fpr.R(c));
		if (packed)
		{
			MULPD(XMM0, fpr.R(a));
			if (inst.SUBOP5 == 28) //msub
				SUBPD(XMM0, fpr.R(b));
			else                   //(n)madd
				ADDPD(XMM0, fpr.R(b));
		}
		else
		{
			MULSD(XMM0, fpr.R(a));
			if (inst.SUBOP5 == 28)
				SUBSD(XMM0, fpr.R(b));
			else
				ADDSD(XMM0, fpr.R(b));
		}
		if (inst.SUBOP5 == 31) //nmadd
			PXOR(XMM0, M(packed ? psSignBits2 : psSignBits));
	}

	fpr.BindToRegister(d, !single);

	if (single)
	{
		if (packed)
		{
			ForceSinglePrecisionP(fpr.RX(d), XMM0);
		}
		else
		{
			ForceSinglePrecisionS(fpr.RX(d), XMM0);
			MOVDDUP(fpr.RX(d), fpr.R(d));
		}
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
	fpr.BindToRegister(d);

	if (d != b)
		MOVSD(fpr.RX(d), fpr.R(b));
	switch (inst.SUBOP10)
	{
	case 40:  // fnegx
		// We can cheat and not worry about clobbering the top half by using masks
		// that don't modify the top half.
		PXOR(fpr.RX(d), M(psSignBits));
		break;
	case 264: // fabsx
		PAND(fpr.RX(d), M(psAbsMask));
		break;
	case 136: // fnabs
		POR(fpr.RX(d), M(psSignBits));
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
	MOVAPD(XMM1, fpr.R(a));
	PXOR(XMM0, R(XMM0));
	// This condition is very tricky; there's only one right way to handle both the case of
	// negative/positive zero and NaN properly.
	// (a >= -0.0 ? c : b) transforms into (0 > a ? b : c), hence the NLE.
	CMPSD(XMM0, R(XMM1), NLE);
	if (cpu_info.bSSE4_1)
	{
		MOVAPD(XMM1, fpr.R(c));
		BLENDVPD(XMM1, fpr.R(b));
	}
	else
	{
		MOVAPD(XMM1, R(XMM0));
		PAND(XMM0, fpr.R(b));
		PANDN(XMM1, fpr.R(c));
		POR(XMM1, R(XMM0));
	}
	fpr.BindToRegister(d);
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
	bool fprf = SConfig::GetInstance().m_LocalCoreStartupParameter.bFPRF && js.op->wantsFPRF;
	//bool ordered = !!(inst.SUBOP10 & 32);
	int a = inst.FA;
	int b = inst.FB;
	int crf = inst.CRFD;
	int output[4] = { CR_SO, CR_EQ, CR_GT, CR_LT };

	// Merge neighboring fcmp and cror (the primary use of cror).
	UGeckoInstruction next = js.op[1].inst;
	if (analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE) &&
	    MergeAllowedNextInstructions(1) && next.OPCD == 19 && next.SUBOP10 == 449 &&
	    (next.CRBA >> 2) == crf && (next.CRBB >> 2) == crf && (next.CRBD >> 2) == crf)
	{
		js.skipInstructions = 1;
		js.downcountAmount++;
		int dst = 3 - (next.CRBD & 3);
		output[3 - (next.CRBD & 3)] &= ~(1 << dst);
		output[3 - (next.CRBA & 3)] |= 1 << dst;
		output[3 - (next.CRBB & 3)] |= 1 << dst;
	}

	bool merge_branch = CheckMergedBranch(crf, js.skipInstructions + 1);
	// True if we're taking the main path, false if we need to jump over it.
	// e.g. with a non-forward-jump branch, true means going into the branch code,
	// false means jumping to the end.
	// Without branch merging, we default to the main path.
	bool direction[4] = { true, true, true, true };
	bool cc = analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
	bool forwardJumps = analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_FORWARD_JUMP);
	bool jumpInBlock = false;
	u32 destination, nextPC;
	if (merge_branch)
	{
		js.downcountAmount++;
		js.skipInstructions++;
		next = js.op[js.skipInstructions].inst;
		nextPC = js.op[js.skipInstructions].address;
		int test_bit = 8 >> (next.BI & 3);
		bool condition = !!(next.BO & BO_BRANCH_IF_TRUE);
		if (next.OPCD == 16 && cc && forwardJumps)
		{
			if (next.AA)
				destination = SignExt16(next.BD << 2);
			else
				destination = nextPC + SignExt16(next.BD << 2);
			jumpInBlock = destination > nextPC && destination < js.blockEnd;
		}
		// If condition, branch taken (direction = true) if test_bit is set.
		// Otherwise, branch taken if test_bit is not set.
		// Forward jumps inverts the conditions.
		for (int i = 0; i < 4; i++)
			direction[i] = !(output[i] & test_bit) ^ condition ^ jumpInBlock;
	}

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
	// In branch merging mode, the continue jumps go to the branch, not the end of the instruction.
	FixupBranch pContinue[4];
	bool continueBranch[4] = { false, false, false, false };

	if (a != b)
	{
		// if B > A, goto Lesser's jump target
		pLesser = J_CC(CC_A);
	}

	// if (B != B) or (A != A), goto NaN's jump target
	pNaN = J_CC(CC_P, true);

	if (a != b)
	{
		// if B < A, goto Greater's jump target
		// JB can't precede the NaN check because it doesn't test ZF
		pGreater = J_CC(CC_B);
	}

	MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_EQ_BIT])));
	if (fprf)
		OR(32, PPCSTATE(fpscr), Imm32(CR_EQ << FPRF_SHIFT));
	// FIXME: if the jump is in the block, we have to store the CRval here before taking that path.
	if (jumpInBlock && !direction[CR_EQ_BIT])
		MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
	// We need a branch if this isn't the last case, or if we're not going directly into the branch.
	if (a != b || !direction[CR_EQ_BIT])
	{
		pContinue[CR_EQ_BIT] = J(!direction[CR_EQ_BIT]);
		continueBranch[CR_EQ_BIT] = true;
	}

	// NaN is an incredibly rare result, so put this branch in farcode.
	SwitchToFarCode();
	SetJumpTarget(pNaN);
	MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_SO_BIT])));
	if (fprf)
		OR(32, PPCSTATE(fpscr), Imm32(CR_SO << FPRF_SHIFT));
	if (jumpInBlock && !direction[CR_SO_BIT])
		MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
	pContinue[CR_SO_BIT] = J(true);
	continueBranch[CR_SO_BIT] = true;
	SwitchToNearCode();

	if (a != b)
	{
		SetJumpTarget(pGreater);
		MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_GT_BIT])));
		if (fprf)
			OR(32, PPCSTATE(fpscr), Imm32(CR_GT << FPRF_SHIFT));
		if (jumpInBlock && !direction[CR_GT_BIT])
			MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
		pContinue[CR_GT_BIT] = J(!direction[CR_GT_BIT]);
		continueBranch[CR_GT_BIT] = true;

		SetJumpTarget(pLesser);
		MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_LT_BIT])));
		if (fprf)
			OR(32, PPCSTATE(fpscr), Imm32(CR_LT << FPRF_SHIFT));
		if (!direction[CR_LT_BIT])
		{
			if (jumpInBlock)
				MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
			pContinue[CR_LT_BIT] = J(true);
			continueBranch[CR_LT_BIT] = true;
		}
	}

	fpr.UnlockAll();

	if (merge_branch)
	{
		if (jumpInBlock)
		{
			BranchTarget branchData = { {}, 0, js.downcountAmount, js.fifoBytesThisBlock, js.firstFPInstructionFound, gpr, fpr, &js.op[1] };
			for (int i = 0; i < 4; i++)
				if (continueBranch[i] && !direction[i])
					branchData.sourceBranch[branchData.branchCount++] = pContinue[i];
			branch_targets.insert(std::make_pair(destination, branchData));

			for (int i = 0; i < 4; i++)
				if (continueBranch[i] && direction[i])
					SetJumpTarget(pContinue[i]);
			MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
		}
		else
		{
			for (int i = 0; i < 4; i++)
				if (continueBranch[i] && direction[i])
					SetJumpTarget(pContinue[i]);
			MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
			gpr.Flush(FLUSH_MAINTAIN_STATE);
			fpr.Flush(FLUSH_MAINTAIN_STATE);
			DoMergedBranch(js.skipInstructions);
			for (int i = 0; i < 4; i++)
				if (continueBranch[i] && !direction[i])
					SetJumpTarget(pContinue[i]);
			MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));

			if (!cc)
			{
				gpr.Flush();
				fpr.Flush();
				WriteExit(nextPC + 4);
			}
		}
	}
	else
	{
		for (int i = 0; i < 4; i++)
			if (continueBranch[i])
				SetJumpTarget(pContinue[i]);

		MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
	}
}

void Jit64::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);

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
	fpr.BindToRegister(d);

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

	MOVAPD(XMM0, M(half_qnan_and_s32_max));
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
	ForceSinglePrecisionS(fpr.RX(d), fpr.RX(d));
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
	fpr.BindToRegister(d);
	MOVAPD(XMM0, fpr.R(b));
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
	fpr.BindToRegister(d);
	MOVAPD(XMM0, fpr.R(b));
	CALL((void *)asm_routines.fres);
	MOVSD(fpr.R(d), XMM0);
	SetFPRFIfNeeded(inst, fpr.RX(d));
	fpr.UnlockAll();
	gpr.UnlockAllX();
}
