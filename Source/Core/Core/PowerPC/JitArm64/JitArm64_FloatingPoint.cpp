// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::fabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FABS(EncodeRegToDouble(VD), EncodeRegToDouble(VB));
}

void JitArm64::faddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);

	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}

void JitArm64::faddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}

void JitArm64::fmaddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));

	fpr.Unlock(V0);
}

void JitArm64::fmaddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FMADD(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC), EncodeRegToDouble(VB));
}

void JitArm64::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.INS(64, VD, 0, VB, 0);
}

void JitArm64::fmsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));

	fpr.Unlock(V0);
}

void JitArm64::fmsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FNMSUB(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC), EncodeRegToDouble(VB));
}

void JitArm64::fmulsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);

	m_float_emit.FMUL(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
}

void JitArm64::fmulx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FMUL(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
}

void JitArm64::fnabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FABS(EncodeRegToDouble(VD), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD));
}

void JitArm64::fnegx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VB));
}

void JitArm64::fnmaddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD));

	fpr.Unlock(V0);
}

void JitArm64::fnmaddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FNMADD(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC), EncodeRegToDouble(VB));
}

void JitArm64::fnmsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD));

	fpr.Unlock(V0);
}

void JitArm64::fnmsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VC = fpr.R(c, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FMSUB(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC), EncodeRegToDouble(VB));
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

void JitArm64::fsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);

	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}

void JitArm64::fsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}

void JitArm64::frspx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

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

	FixupBranch pNaN1, pNaN2, pNaN3, pLesser, pGreater;
	FixupBranch continue1, continue2, continue3;
	ORR(XA, ZR, 32, 0, true);

	if (a != b)
	{
		m_float_emit.FCMP(EncodeRegToDouble(VA), EncodeRegToDouble(VA));

		// if (B != B) or (A != A), goto NaN's jump target
		pNaN1 = B(CC_NEQ);

		m_float_emit.FCMP(EncodeRegToDouble(VB), EncodeRegToDouble(VB));

		pNaN2 = B(CC_NEQ);
	}

	m_float_emit.FCMP(EncodeRegToDouble(VA), EncodeRegToDouble(VB));

	if (a == b)
		pNaN3 = B(CC_NEQ);

	if (a != b)
	{
		// if B > A goto Greater's jump target
		pGreater = B(CC_GT);
		// if B < A, goto Lesser's jump target
		pLesser = B(CC_MI);
	}

	ORR(XA, XA, 64 - 63, 0, true);
	continue1 = B();

	if (a != b)
	{
		SetJumpTarget(pNaN1);
		SetJumpTarget(pNaN2);
	}
	else
	{
		SetJumpTarget(pNaN3);
	}

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
	}

	SetJumpTarget(continue1);
	if (a != b)
	{
		SetJumpTarget(continue2);
		SetJumpTarget(continue3);
	}

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

void JitArm64::fdivx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d);

	m_float_emit.FDIV(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}

void JitArm64::fdivsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_IS_LOADED);
	ARM64Reg VB = fpr.R(b, REG_IS_LOADED);
	ARM64Reg VD = fpr.RW(d, REG_DUP);

	m_float_emit.FDIV(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
}
