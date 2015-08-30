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
	fpr.BindToRegister(d, true);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FABS(EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::faddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);

	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 1, VD, 0);
}

void JitArm64::faddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FADD(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::fmaddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b || d == c);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 1, VD, 0);
	fpr.Unlock(V0);
}

void JitArm64::fmaddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(V0), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);
	fpr.Unlock(V0);
}

void JitArm64::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);

	m_float_emit.INS(64, VD, 0, VB, 0);
}

void JitArm64::fmsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b || d == c);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 1, VD, 0);
	fpr.Unlock(V0);
}

void JitArm64::fmsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(V0), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);
	fpr.Unlock(V0);
}

void JitArm64::fmulsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == c);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);

	m_float_emit.FMUL(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.INS(64, VD, 1, VD, 0);
}

void JitArm64::fmulx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::fnabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FABS(EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(V0), EncodeRegToDouble(V0));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::fnegx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FNEG(EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::fnmaddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b || d == c);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD));
	m_float_emit.INS(64, VD, 1, VD, 0);
	fpr.Unlock(V0);
}

void JitArm64::fnmaddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FADD(EncodeRegToDouble(V0), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(V0), EncodeRegToDouble(V0));
	m_float_emit.INS(64, VD, 0, V0, 0);
	fpr.Unlock(V0);
}

void JitArm64::fnmsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b || d == c);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(VD), EncodeRegToDouble(VD));
	m_float_emit.INS(64, VD, 1, VD, 0);
	fpr.Unlock(V0);
}

void JitArm64::fnmsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FMUL(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VC));
	m_float_emit.FSUB(EncodeRegToDouble(V0), EncodeRegToDouble(V0), EncodeRegToDouble(VB));
	m_float_emit.FNEG(EncodeRegToDouble(V0), EncodeRegToDouble(V0));
	m_float_emit.INS(64, VD, 0, V0, 0);
	fpr.Unlock(V0);
}

void JitArm64::fselx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VD = fpr.R(d);
	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VC = fpr.R(c);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FCMPE(EncodeRegToDouble(VA));
	m_float_emit.FCSEL(EncodeRegToDouble(V0), EncodeRegToDouble(VC), EncodeRegToDouble(VB), CC_GE);
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}

void JitArm64::fsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, d == a || d == b);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);

	m_float_emit.FSUB(EncodeRegToDouble(VD), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 1, VD, 0);
}

void JitArm64::fsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	fpr.BindToRegister(d, true);

	ARM64Reg VA = fpr.R(a);
	ARM64Reg VB = fpr.R(b);
	ARM64Reg VD = fpr.R(d);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.FSUB(EncodeRegToDouble(V0), EncodeRegToDouble(VA), EncodeRegToDouble(VB));
	m_float_emit.INS(64, VD, 0, V0, 0);

	fpr.Unlock(V0);
}
