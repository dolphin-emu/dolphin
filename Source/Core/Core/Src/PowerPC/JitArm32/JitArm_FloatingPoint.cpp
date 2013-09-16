// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "Thunk.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../../ConfigManager.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "ArmEmitter.h"
#include "../../HW/Memmap.h"
#include "../Interpreter/Interpreter_FPUtils.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitFPRCache.h"
#include "JitAsm.h"

static const double minmaxFloat[2] = {-(double)0x80000000, (double)0x7FFFFFFF};
static const double doublenum = 0xfff8000000000000ull;
static Operand2 FRFIMask(5, 0x8); // 0x60000
static Operand2 FIMask(2, 8); // 0x20000
static Operand2 FRMask(4, 8); // 0x40000
static Operand2 FXMask(2, 1); // 0x80000000

static Operand2 XXException(2, 4); // 0x2000000
static Operand2 CVIException(1, 0xC); // 0x100 

void JitArm::Helper_UpdateCR1(ARMReg value)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

void JitArm::SetFPException(ARMReg Reg, u32 Exception)
{
	Operand2 *ExceptionMask;
	switch(Exception)
	{
		case FPSCR_VXCVI:
			ExceptionMask = &CVIException;
		break;
		case FPSCR_XX:
			ExceptionMask = &XXException;
		break;
		default:
			_assert_msg_(DYNA_REC, false, "Passed unsupported FPexception: 0x%08x", Exception);
			return;
		break;
	}
	ARMReg rB = gpr.GetReg();
	MOV(rB, Reg);
	ORR(Reg, Reg, *ExceptionMask);
	CMP(rB, Reg);
	SetCC(CC_NEQ);
	ORR(Reg, Reg, FXMask); // If exception is set, set exception bit	
	SetCC();
	BIC(Reg, Reg, FRFIMask);
	gpr.Unlock(rB);
}

void JitArm::fctiwzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	u32 b = inst.FB;
	u32 d = inst.FD;

	ARMReg vB = fpr.R0(b);
	ARMReg vD = fpr.R0(d);
	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();
	ARMReg V2 = fpr.GetReg();

	ARMReg rA = gpr.GetReg();
	ARMReg fpscrReg = gpr.GetReg();

	FixupBranch DoneMax, DoneMin;
	LDR(fpscrReg, R9, PPCSTATE_OFF(fpscr));
	MOVI2R(rA, (u32)minmaxFloat);

	// Check if greater than max float
	{
		VLDR(V0, rA, 8); // Load Max
		VCMPE(vB, V0);
		VMRS(_PC); // Loads in to APSR
		FixupBranch noException = B_CC(CC_LE);
		VMOV(vD, V0); // Set to max
		SetFPException(fpscrReg, FPSCR_VXCVI);
		DoneMax = B();
		SetJumpTarget(noException);
	}
	// Check if less than min float
	{
		VLDR(V0, rA, 0);
		VCMPE(vB, V0);
		VMRS(_PC);
		FixupBranch noException = B_CC(CC_GE);
		VMOV(vD, V0);
		SetFPException(fpscrReg, FPSCR_VXCVI);
		DoneMin = B();
		SetJumpTarget(noException);
	}
	// Within ranges, convert to integer
	VCVT(vD, vB, TO_INT | IS_SIGNED | ROUND_TO_ZERO); 
	VCMPE(vD, vB);
	VMRS(_PC);

	SetCC(CC_EQ);
		BIC(fpscrReg, fpscrReg, FRFIMask);
		FixupBranch DoneEqual = B();
	SetCC();
	SetFPException(fpscrReg, FPSCR_XX);	
	ORR(fpscrReg, fpscrReg, FIMask);
	VABS(V1, vB);
	VABS(V2, vD);
	VCMPE(V2, V1);
	VMRS(_PC);
	SetCC(CC_GT);
		ORR(fpscrReg, fpscrReg, FRMask);
	SetCC();
	SetJumpTarget(DoneEqual);

	SetJumpTarget(DoneMax);
	SetJumpTarget(DoneMin);

	MOVI2R(rA, (u32)&doublenum);
	VLDR(V0, rA, 0);
	NEONXEmitter nemit(this);
	nemit.VORR(vD, vD, V0);

	if (inst.Rc) Helper_UpdateCR1(vD);

	STR(fpscrReg, R9, PPCSTATE_OFF(fpscr));
	gpr.Unlock(rA);
	gpr.Unlock(fpscrReg);
	fpr.Unlock(V0);
	fpr.Unlock(V1);
	fpr.Unlock(V2);
}

void JitArm::fabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VABS(vD, vB);

	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fnabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VABS(vD, vB);
	VNEG(vD, vD);

	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fnegx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VNEG(vD, vB);

	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::faddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD0 = fpr.R0(inst.FD, false);
	ARMReg vD1 = fpr.R1(inst.FD, false);

	VADD(vD0, vA, vB);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}

void JitArm::faddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VADD(vD, vA, vB);
	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD0 = fpr.R0(inst.FD, false);
	ARMReg vD1 = fpr.R1(inst.FD, false);

	VSUB(vD0, vA, vB);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}

void JitArm::fsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VSUB(vD, vA, vB);
	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fmulsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)
	
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vC = fpr.R0(inst.FC);
	ARMReg vD0 = fpr.R0(inst.FD, false);
	ARMReg vD1 = fpr.R1(inst.FD, false);

	VMUL(vD0, vA, vC);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}
void JitArm::fmulx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vC = fpr.R0(inst.FC);
	ARMReg vD0 = fpr.R0(inst.FD, false);

	VMUL(vD0, vA, vC);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}
void JitArm::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD = fpr.R0(inst.FD, false);

	VMOV(vD, vB);
	
	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fmaddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vD0 = fpr.R0(d, false);
	ARMReg vD1 = fpr.R1(d, false);

	ARMReg V0 = fpr.GetReg();

	VMOV(V0, vB0);
	
	VMLA(V0, vA0, vC0);

	VMOV(vD0, V0);
	VMOV(vD1, V0);

	fpr.Unlock(V0);

	if (inst.Rc) Helper_UpdateCR1(vD0);
}

void JitArm::fmaddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff)

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARMReg vA0 = fpr.R0(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vD0 = fpr.R0(d, false);

	ARMReg V0 = fpr.GetReg();

	VMOV(V0, vB0);
	
	VMLA(V0, vA0, vC0);

	VMOV(vD0, V0);

	fpr.Unlock(V0);

	if (inst.Rc) Helper_UpdateCR1(vD0);
}
