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

	bool inputs_are_singles = fpr.IsSingle(a) && (!use_b || fpr.IsSingle(b)) && (!use_c || fpr.IsSingle(c));

	ARM64Reg VA, VB, VC, VD;

	if (packed)
	{
		RegType type = inputs_are_singles ? REG_REG_SINGLE : REG_REG;
		u8 size = inputs_are_singles ? 32 : 64;
		VA = fpr.R(a, type);
		if (use_b)
			VB = fpr.R(b, type);
		if (use_c)
			VC = fpr.R(c, type);
		VD = fpr.RW(d, type);

		switch (op5)
		{
		case 18: m_float_emit.FDIV(size, VD, VA, VB); break;
		case 20: m_float_emit.FSUB(size, VD, VA, VB); break;
		case 21: m_float_emit.FADD(size, VD, VA, VB); break;
		case 25: m_float_emit.FMUL(size, VD, VA, VC); break;
		default: _assert_msg_(DYNA_REC, 0, "fp_arith"); break;
		}
	}
	else
	{
		RegType type = (inputs_are_singles && single) ? REG_IS_LOADED_SINGLE : REG_IS_LOADED;
		RegType type_out = single ? (inputs_are_singles ? REG_DUP_SINGLE : REG_DUP) : REG_LOWER_PAIR;
		ARM64Reg (*reg_encoder)(ARM64Reg) = (inputs_are_singles && single) ? EncodeRegToSingle : EncodeRegToDouble;

		VA = reg_encoder(fpr.R(a, type));
		if (use_b)
			VB = reg_encoder(fpr.R(b, type));
		if (use_c)
			VC = reg_encoder(fpr.R(c, type));
		VD = reg_encoder(fpr.RW(d, type_out));

		switch (op5)
		{
		case 18: m_float_emit.FDIV(VD, VA, VB); break;
		case 20: m_float_emit.FSUB(VD, VA, VB); break;
		case 21: m_float_emit.FADD(VD, VA, VB); break;
		case 25: m_float_emit.FMUL(VD, VA, VC); break;
		case 28: m_float_emit.FNMSUB(VD, VA, VC, VB); break; // fmsub: "D = A*C - B" vs "Vd = (-Va) + Vn*Vm"
		case 29: m_float_emit.FMADD(VD, VA, VC, VB); break; // fmadd: "D = A*C + B" vs "Vd = Va + Vn*Vm"
		case 30: m_float_emit.FMSUB(VD, VA, VC, VB); break; // fnmsub: "D = -(A*C - B)" vs "Vd = Va + (-Vn)*Vm"
		case 31: m_float_emit.FNMADD(VD, VA, VC, VB); break; // fnmadd: "D = -(A*C + B)" vs "Vd = (-Va) + (-Vn)*Vm"
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

	bool is_single = fpr.IsSingle(b);

	if (packed)
	{
		RegType type = is_single ? REG_REG_SINGLE : REG_REG;
		u8 size = is_single ? 32 : 64;

		ARM64Reg VB = fpr.R(b, type);
		ARM64Reg VD = fpr.RW(d, type);

		switch (op10)
		{
		case  40: m_float_emit.FNEG(size, VD, VB); break;
		case  72: m_float_emit.ORR(VD, VB, VB); break;
		case 136: m_float_emit.FABS(size, VD, VB);
		          m_float_emit.FNEG(size, VD, VD); break;
		case 264: m_float_emit.FABS(size, VD, VB); break;
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

	if (fpr.IsSingle(b))
	{
		// Source is already in single precision, so no need to do anything but to copy to PSR1.
		ARM64Reg VB = fpr.R(b, REG_IS_LOADED_SINGLE);
		ARM64Reg VD = fpr.RW(d, REG_DUP_SINGLE);

		if (b != d)
			m_float_emit.ORR(VD, VB, VB);
	}

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP_SINGLE);

	m_float_emit.FCVT(32, 64, EncodeRegToDouble(VD), EncodeRegToDouble(VB));
}

void JitArm64::fcmpX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB;
	int crf = inst.CRFD;

	bool singles = fpr.IsSingle(a) && fpr.IsSingle(b);
	RegType type = singles ? REG_IS_LOADED_SINGLE : REG_IS_LOADED;
	ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToSingle : EncodeRegToDouble;

	ARM64Reg VA = reg_encoder(fpr.R(a, type));
	ARM64Reg VB = reg_encoder(fpr.R(b, type));

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	FixupBranch pNaN, pLesser, pGreater;
	FixupBranch continue1, continue2, continue3;
	ORR(XA, ZR, 32, 0, true);

	m_float_emit.FCMP(VA, VB);

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
