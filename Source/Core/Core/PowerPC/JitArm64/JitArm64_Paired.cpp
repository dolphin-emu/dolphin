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

void JitArm64::ps_abs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.FABS(64, VD, VB);
}

void JitArm64::ps_madd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(64, V0, VA, VC);
	m_float_emit.FADD(64, VD, V0, VB);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_madds0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VC, 0);
	m_float_emit.FMUL(64, V0, V0, VA);
	m_float_emit.FADD(64, VD, V0, VB);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_madds1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VC, 1);
	m_float_emit.FMUL(64, V0, V0, VA);
	m_float_emit.FADD(64, VD, V0, VB);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_merge00(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.TRN1(64, VD, VA, VB);
}

void JitArm64::ps_merge01(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.INS(64, VD, 0, VA, 0);
	m_float_emit.INS(64, VD, 1, VB, 1);
}

void JitArm64::ps_merge10(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	if (d != a && d != b)
	{
		m_float_emit.INS(64, VD, 0, VA, 1);
		m_float_emit.INS(64, VD, 1, VB, 0);
	}
	else
	{
		ARM64Reg V0 = fpr.GetReg();
		m_float_emit.INS(64, V0, 0, VA, 1);
		m_float_emit.INS(64, V0, 1, VB, 0);
		m_float_emit.ORR(VD, V0, V0);
		fpr.Unlock(V0);
	}
}

void JitArm64::ps_merge11(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.TRN2(64, VD, VA, VB);
}

void JitArm64::ps_mr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	if (d == b)
		return;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.ORR(VD, VB, VB);
}

void JitArm64::ps_muls0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VC, 0);
	m_float_emit.FMUL(64, VD, VA, V0);
	fpr.FixSinglePrecision(d);
	fpr.Unlock(V0);
}

void JitArm64::ps_muls1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VC, 1);
	m_float_emit.FMUL(64, VD, VA, V0);
	fpr.FixSinglePrecision(d);
	fpr.Unlock(V0);
}

void JitArm64::ps_msub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(64, V0, VA, VC);
	m_float_emit.FSUB(64, VD, V0, VB);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_nabs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.FABS(64, VD, VB);
	m_float_emit.FNEG(64, VD, VD);
}

void JitArm64::ps_neg(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.FNEG(64, VD, VB);
}

void JitArm64::ps_nmadd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(64, V0, VA, VC);
	m_float_emit.FADD(64, VD, V0, VB);
	m_float_emit.FNEG(64, VD, VD);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_nmsub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(64, V0, VA, VC);
	m_float_emit.FSUB(64, VD, V0, VB);
	m_float_emit.FNEG(64, VD, VD);
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_res(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.FRSQRTE(64, VD, VB);
	fpr.FixSinglePrecision(d);
}

void JitArm64::ps_sel(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	if (d != a && d != b && d != c)
	{
		m_float_emit.FCMGE(64, VD, VA);
		m_float_emit.BSL(VD, VC, VB);
	}
	else
	{
		ARM64Reg V0 = fpr.GetReg();
		m_float_emit.FCMGE(64, V0, VA);
		m_float_emit.BSL(V0, VC, VB);
		m_float_emit.ORR(VD, V0, V0);
		fpr.Unlock(V0);
	}
}

void JitArm64::ps_sum0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VB, 1);
	if (d != c)
	{
		m_float_emit.FADD(64, VD, V0, VA);
		m_float_emit.INS(64, VD, 1, VC, 1);
	}
	else
	{
		m_float_emit.FADD(64, V0, V0, VA);
		m_float_emit.INS(64, VD, 0, V0, 0);
	}
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_sum1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VA, 0);
	if (d != c)
	{
		m_float_emit.FADD(64, VD, V0, VB);
		m_float_emit.INS(64, VD, 0, VC, 0);
	}
	else
	{
		m_float_emit.FADD(64, V0, V0, VB);
		m_float_emit.INS(64, VD, 1, V0, 1);
	}
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}
