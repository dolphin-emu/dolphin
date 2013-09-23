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


#include "Jit.h"
#include "JitRegCache.h"
#include "JitFPRCache.h"
#include "JitAsm.h"

void JitArm::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg v0 = fpr.R0(inst.FD);
	ARMReg v1 = fpr.R1(inst.FD);

	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else
		MOVI2R(rB, (u32)inst.SIMM_16);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	MOVI2R(rA, (u32)&Memory::Read_U32);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);

	VMOV(S0, R0);	

	VCVT(v0, S0, 0);
	VCVT(v1, S0, 0);
	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}
void JitArm::lfsu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg RA = gpr.R(inst.RA);

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg v0 = fpr.R0(inst.FD);
	ARMReg v1 = fpr.R1(inst.FD);

	MOVI2R(rB, inst.SIMM_16);
	ADD(rB, rB, RA);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	MOVI2R(rA, (u32)&Memory::Read_U32);	

	MOV(RA, rB);
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);

	VMOV(S0, R0);

	VCVT(v0, S0, 0);
	VCVT(v1, S0, 0);
	POP(4, R0, R1, R2, R3);
	

	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg RB = gpr.R(inst.RB);
	ARMReg v0 = fpr.R0(inst.FD);
	ARMReg v1 = fpr.R1(inst.FD);
	
	if (inst.RA)
		ADD(rB, RB, gpr.R(inst.RA));
	else
		MOV(rB, RB);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	
	MOVI2R(rA, (u32)&Memory::Read_U32);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);

	VMOV(S0, R0);	
	VCVT(v0, S0, 0);
	VCVT(v1, S0, 0);
	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg v0 = fpr.R0(inst.FD);

	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else
		MOVI2R(rB, (u32)inst.SIMM_16);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	MOVI2R(rA, (u32)&Memory::Read_F64);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);

#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
	VMOV(v0, R0);
#else
	VMOV(v0, D0);
#endif

	POP(4, R0, R1, R2, R3);

	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::lfdu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg v0 = fpr.R0(inst.FD);

	MOVI2R(rB, inst.SIMM_16);
	ADD(rB, rB, RA);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	MOVI2R(rA, (u32)&Memory::Read_F64);	
	MOV(RA, rB);

	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);

#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
	VMOV(v0, R0);
#else
	VMOV(v0, D0);
#endif

	POP(4, R0, R1, R2, R3);

	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg v0 = fpr.R0(inst.FS);
	VCVT(S0, v0, 0);
	
	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else
		MOVI2R(rB, (u32)inst.SIMM_16);

	
	MOVI2R(rA, (u32)&Memory::Write_U32);	
	PUSH(4, R0, R1, R2, R3);
	VMOV(R0, S0);
	MOV(R1, rB);

	BL(rA);

	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
}

void JitArm::stfsu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg RA = gpr.R(inst.RA);

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg v0 = fpr.R0(inst.FS);
	VCVT(S0, v0, 0);
	
	MOVI2R(rB, inst.SIMM_16);
	ADD(rB, rB, RA);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	
	SetCC(CC_NEQ);
	MOV(RA, rB);
	SetCC();

	MOVI2R(rA, (u32)&Memory::Write_U32);	
	PUSH(4, R0, R1, R2, R3);
	VMOV(R0, S0);
	MOV(R1, rB);

	BL(rA);

	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
}

void JitArm::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg v0 = fpr.R0(inst.FS);
	
	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else
		MOVI2R(rB, (u32)inst.SIMM_16);

	
	MOVI2R(rA, (u32)&Memory::Write_F64);	
	PUSH(4, R0, R1, R2, R3);
#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
	VMOV(R0, v0);
	MOV(R2, rB);
#else
	VMOV(D0, v0);
	MOV(R0, rB);
#endif

	BL(rA);

	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
}

void JitArm::stfdu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)
	ARMReg RA = gpr.R(inst.RA);
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg v0 = fpr.R0(inst.FS);
	
	MOVI2R(rB, inst.SIMM_16);
	ADD(rB, rB, RA);
	
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	
	SetCC(CC_NEQ);
	MOV(RA, rB);
	SetCC();

	MOVI2R(rA, (u32)&Memory::Write_F64);	
	PUSH(4, R0, R1, R2, R3);
#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
	VMOV(R0, v0);
	MOV(R2, rB);
#else
	VMOV(D0, v0);
	MOV(R0, rB);
#endif

	BL(rA);

	POP(4, R0, R1, R2, R3);
	
	gpr.Unlock(rA, rB);
}

