// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::fp_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	u32 op5 = inst.SUBOP5;

	bool single = inst.OPCD == 59;
	bool packed = inst.OPCD == 4;

	bool use_c = op5 >= 25; // fmul and all kind of fmaddXX
	bool use_b = op5 != 25; // fmul uses no B

	ARM64Reg VA, VB, VC, VD;

	if (packed)
	{
		VA = fpr.R(a, REG_REG);
		if (use_b)
			VB = fpr.R(b, REG_REG);
		if (use_c)
			VC = fpr.R(c, REG_REG);
		VD = fpr.RW(d, REG_REG);

		switch (op5)
		{
		case 18: m_float_emit.FDIV(64, VD, VA, VB); break;
		case 20: m_float_emit.FSUB(64, VD, VA, VB); break;
		case 21: m_float_emit.FADD(64, VD, VA, VB); break;
		case 25: m_float_emit.FMUL(64, VD, VA, VC); break;
		default: _assert_msg_(DYNA_REC, 0, "fp_arith"); break;
		}
	}
	else
	{
		VA = EncodeRegToDouble(fpr.R(a, REG_IS_LOADED));
		if (use_b)
			VB = EncodeRegToDouble(fpr.R(b, REG_IS_LOADED));
		if (use_c)
			VC = EncodeRegToDouble(fpr.R(c, REG_IS_LOADED));
		VD = EncodeRegToDouble(fpr.RW(d, single ? REG_DUP : REG_LOWER_PAIR));

		switch (op5)
		{
		case 18: m_float_emit.FDIV(VD, VA, VB); break;
		case 20: m_float_emit.FSUB(VD, VA, VB); break;
		case 21: m_float_emit.FADD(VD, VA, VB); break;
		case 25: m_float_emit.FMUL(VD, VA, VC); break;
		case 28: m_float_emit.FNMSUB(VD, VA, VC, VB); break;
		case 29: m_float_emit.FMADD(VD, VA, VC, VB); break;
		case 30: m_float_emit.FMSUB(VD, VA, VC, VB); break;
		case 31: m_float_emit.FNMADD(VD, VA, VC, VB); break;
		default: _assert_msg_(DYNA_REC, 0, "fp_arith"); break;
		}
	}

	if (single || packed)
		fpr.FixSinglePrecision(d);
}

void JitArm64::fp_logic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;
	u32 op10 = inst.SUBOP10;

	bool packed = inst.OPCD == 4;

	// MR with source === dest => no-op
	if (op10 == 72 && b == d)
		return;

	if (packed)
	{
		ARM64Reg VB = fpr.R(b, REG_REG);
		ARM64Reg VD = fpr.RW(d, REG_REG);

		switch (op10)
		{
		case  40: m_float_emit.FNEG(64, VD, VB); break;
		case  72: m_float_emit.ORR(VD, VB, VB); break;
		case 136: m_float_emit.FABS(64, VD, VB);
		          m_float_emit.FNEG(64, VD, VD); break;
		case 264: m_float_emit.FABS(64, VD, VB); break;
		default: _assert_msg_(DYNA_REC, 0, "fp_logic"); break;
		}
	}
	else
	{
		ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
		ARM64Reg VD = fpr.RW(d);

		switch (op10)
		{
		case  40: m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VB)); break;
		case  72: m_float_emit.INS(64, VD, 0, VB, 0); break;
		case 136: m_float_emit.FABS(EncodeRegToDouble(VD), EncodeRegToDouble(VB));
		          m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD)); break;
		case 264: m_float_emit.FABS(EncodeRegToDouble(VD), EncodeRegToDouble(VB)); break;
		default: _assert_msg_(DYNA_REC, 0, "fp_logic"); break;
		}
	}
}

void JitArm64::fselx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FCMPE(EncodeRegToDouble(VA));
	m_float_emit.FCSEL(EncodeRegToDouble(VD), EncodeRegToDouble(VC), EncodeRegToDouble(VB), CC_GE);
}

void JitArm64::frspx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);

	m_float_emit.FCVT(32, 64, EncodeRegToDouble(VD), EncodeRegToDouble(VB));
	m_float_emit.FCVT(64, 32, EncodeRegToDouble(VD), EncodeRegToDouble(VD));
}

void JitArm64::fcmpX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB;
	int crf = inst.CRFD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	FixupBranch pNaN, pLesser, pGreater;
	FixupBranch continue1, continue2, continue3;
	ORR(XA, ZR, 32, 0, true);

	m_float_emit.FCMP(EncodeRegToDouble(VA), EncodeRegToDouble(VB));

	if (a != b)
	{
		// if B > A goto Greater's jump target
		pGreater = B(CC_GT);
		// if B < A, goto Lesser's jump target
		pLesser = B(CC_MI);
	}

	pNaN = B(CC_VS);

	// A == B
	ORR(XA, XA, 64 - 63, 0, true);
	continue1 = B();

	SetJumpTarget(pNaN);

	ORR(XA, XA, 64 - 61, 0, true);
	ORR(XA, XA, 0, 0, true);

	if (a != b)
	{
		continue2 = B();

		SetJumpTarget(pGreater);
		ORR(XA, XA, 0, 0, true);

		continue3 = B();

		SetJumpTarget(pLesser);
		ORR(XA, XA, 64 - 62, 1, true);
		ORR(XA, XA, 0, 0, true);

		SetJumpTarget(continue2);
		SetJumpTarget(continue3);
	}
	SetJumpTarget(continue1);

	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[0]) + (sizeof(PowerPC::ppcState.cr_val[0]) * crf));

	gpr.Unlock(WA);
}

void JitArm64::fctiwzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	ARM64Reg V0 = fpr.GetReg();

	// Generate 0xFFF8000000000000ULL
	m_float_emit.MOVI(64, EncodeRegToDouble(V0), 0xFFFF000000000000ULL);
	m_float_emit.BIC(16, EncodeRegToDouble(V0), 0x7);

	m_float_emit.FCVT(32, 64, EncodeRegToDouble(VD), EncodeRegToDouble(VB));
	m_float_emit.FCVTS(EncodeRegToSingle(VD), EncodeRegToSingle(VD), ROUND_Z);
	m_float_emit.ORR(EncodeRegToDouble(VD), EncodeRegToDouble(VD), EncodeRegToDouble(V0));
	fpr.Unlock(V0);
}
