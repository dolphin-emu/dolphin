// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_FPUtils.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

using namespace ArmGen;

void JitArm::ps_rsqrte(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(true);

	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	ARMReg fpscrReg = gpr.GetReg();
	ARMReg V0 = D1;
	ARMReg rA = gpr.GetReg();

	MOVI2R(fpscrReg, (u32)&PPC_NAN);
	VLDR(V0, fpscrReg, 0);
	LDR(fpscrReg, R9, PPCSTATE_OFF(fpscr));

	VCMP(vB0);
	VMRS(_PC);
	FixupBranch Less0 = B_CC(CC_LT);
		VMOV(vD0, V0);
		SetFPException(fpscrReg, FPSCR_VXSQRT);
		FixupBranch SkipOrr0 = B();
	SetJumpTarget(Less0);
	SetCC(CC_EQ);
		ORR(rA, rA, 1);
	SetCC();
	SetJumpTarget(SkipOrr0);

	VCMP(vB1);
	VMRS(_PC);
	FixupBranch Less1 = B_CC(CC_LT);
		VMOV(vD1, V0);
		SetFPException(fpscrReg, FPSCR_VXSQRT);
		FixupBranch SkipOrr1 = B();
	SetJumpTarget(Less1);
	SetCC(CC_EQ);
		ORR(rA, rA, 2);
	SetCC();
	SetJumpTarget(SkipOrr1);

	CMP(rA, 0);
	FixupBranch noException = B_CC(CC_EQ);
	SetFPException(fpscrReg, FPSCR_ZX);
	SetJumpTarget(noException);

	VCVT(S0, vB0, 0);
	VCVT(S1, vB1, 0);

	NEONXEmitter nemit(this);
	nemit.VRSQRTE(F_32, D0, D0);
	VCVT(vD0, S0, 0);
	VCVT(vD1, S1, 0);

	STR(fpscrReg, R9, PPCSTATE_OFF(fpscr));
	gpr.Unlock(fpscrReg, rA);
}

void JitArm::ps_sel(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VCMP(vA0);
	VMRS(_PC);

	FixupBranch GT0 = B_CC(CC_GE);
	VMOV(vD0, vB0);
	FixupBranch EQ0 = B();
	SetJumpTarget(GT0);
	VMOV(vD0, vC0);
	SetJumpTarget(EQ0);

	VCMP(vA1);
	VMRS(_PC);
	FixupBranch GT1 = B_CC(CC_GE);
	VMOV(vD1, vB1);
	FixupBranch EQ1 = B();
	SetJumpTarget(GT1);
	VMOV(vD1, vC1);
	SetJumpTarget(EQ1);
}

void JitArm::ps_add(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VADD(vD0, vA0, vB0);
	VADD(vD1, vA1, vB1);
}

void JitArm::ps_div(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VDIV(vD0, vA0, vB0);
	VDIV(vD1, vA1, vB1);
}

void JitArm::ps_res(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	ARMReg V0 = fpr.GetReg();
	MOVI2R(V0, 1.0, INVALID_REG); // temp reg not needed for 1.0

	VDIV(vD0, V0, vB0);
	VDIV(vD1, V0, vB1);
	fpr.Unlock(V0);
}

void JitArm::ps_nmadd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC1);
	VADD(vD0, V0, vB0);
	VADD(vD1, V1, vB1);
	VNEG(vD0, vD0);
	VNEG(vD1, vD1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_madd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC1);
	VADD(vD0, V0, vB0);
	VADD(vD1, V1, vB1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_nmsub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC1);
	VSUB(vD0, V0, vB0);
	VSUB(vD1, V1, vB1);
	VNEG(vD0, vD0);
	VNEG(vD1, vD1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_msub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC1);
	VSUB(vD0, V0, vB0);
	VSUB(vD1, V1, vB1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_madds0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC0);

	VADD(vD0, V0, vB0);
	VADD(vD1, V1, vB1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_madds1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMUL(V0, vA0, vC1);
	VMUL(V1, vA1, vC1);
	VADD(vD0, V0, vB0);
	VADD(vD1, V1, vB1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}
void JitArm::ps_sum0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VADD(vD0, vA0, vB1);
	VMOV(vD1, vC1);

}

void JitArm::ps_sum1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VMOV(vD0, vC0);
	VADD(vD1, vA0, vB1);
}


void JitArm::ps_sub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VSUB(vD0, vA0, vB0);
	VSUB(vD1, vA1, vB1);
}

void JitArm::ps_mul(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VMUL(vD0, vA0, vC0);
	VMUL(vD1, vA1, vC1);
}

void JitArm::ps_muls0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();


	VMUL(V0, vA0, vC0);
	VMUL(V1, vA1, vC0);
	VMOV(vD0, V0);
	VMOV(vD1, V1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_muls1(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();


	VMUL(V0, vA0, vC1);
	VMUL(V1, vA1, vC1);
	VMOV(vD0, V0);
	VMOV(vD1, V1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_merge00(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VMOV(vD1, vB0);
	VMOV(vD0, vA0);
}

void JitArm::ps_merge01(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	VMOV(vD0, vA0);
	VMOV(vD1, vB1);
}

void JitArm::ps_merge10(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	ARMReg V0 = fpr.GetReg();

	VMOV(V0, vB0);
	VMOV(vD0, vA1);
	VMOV(vD1, V0);

	fpr.Unlock(V0);
}

void JitArm::ps_merge11(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARMReg vA1 = fpr.R1(a);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	VMOV(vD0, vA1);
	VMOV(vD1, vB1);
}

void JitArm::ps_mr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	VMOV(vD0, vB0);
	VMOV(vD1, vB1);
}

void JitArm::ps_neg(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	VNEG(vD0, vB0);
	VNEG(vD1, vB1);
}

void JitArm::ps_abs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);
	VABS(vD0, vB0);
	VABS(vD1, vB1);
}

void JitArm::ps_nabs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	VABS(vD0, vB0);
	VNEG(vD0, vD0);
	VABS(vD1, vB1);
	VNEG(vD1, vD1);
}

