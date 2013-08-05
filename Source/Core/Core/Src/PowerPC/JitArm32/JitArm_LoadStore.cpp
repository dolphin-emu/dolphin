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
#include "JitAsm.h"

void JitArm::stb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RS = gpr.R(inst.RS);
	ARMReg ValueReg = gpr.GetReg();
	ARMReg Addr = gpr.GetReg();
	ARMReg Function = gpr.GetReg();

	MOV(ValueReg, RS);
	if (inst.RA)
	{
		MOVI2R(Addr, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(Addr, Addr, RA);
	}
	else
		MOVI2R(Addr, (u32)inst.SIMM_16);
	
	MOVI2R(Function, (u32)&Memory::Write_U8);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, ValueReg);
	MOV(R1, Addr);
	BL(Function);
	POP(4, R0, R1, R2, R3);
	gpr.Unlock(ValueReg, Addr, Function);
}

void JitArm::stbu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg ValueReg = gpr.GetReg();
	ARMReg Addr = gpr.GetReg();
	ARMReg Function = gpr.GetReg();
	
	MOVI2R(Addr, inst.SIMM_16);
	ADD(Addr, Addr, RA);

	// Check for DSI exception prior to writing back address
	LDR(Function, R9, PPCSTATE_OFF(Exceptions));
	CMP(Function, EXCEPTION_DSI);
	FixupBranch DoNotWrite = B_CC(CC_EQ);
	MOV(RA, Addr);
	SetJumpTarget(DoNotWrite);

	MOV(ValueReg, RS);
	
	MOVI2R(Function, (u32)&Memory::Write_U8);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, ValueReg);
	MOV(R1, Addr);
	BL(Function);
	POP(4, R0, R1, R2, R3);

	gpr.Unlock(ValueReg, Addr, Function);
}
void JitArm::sth(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RS = gpr.R(inst.RS);
	ARMReg ValueReg = gpr.GetReg();
	ARMReg Addr = gpr.GetReg();
	ARMReg Function = gpr.GetReg();

	MOV(ValueReg, RS);
	if (inst.RA)
	{
		MOVI2R(Addr, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(Addr, Addr, RA);
	}
	else
		MOVI2R(Addr, (u32)inst.SIMM_16);
	
	MOVI2R(Function, (u32)&Memory::Write_U16);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, ValueReg);
	MOV(R1, Addr);
	BL(Function);
	POP(4, R0, R1, R2, R3);
	gpr.Unlock(ValueReg, Addr, Function);
}
void JitArm::sthu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg ValueReg = gpr.GetReg();
	ARMReg Addr = gpr.GetReg();
	ARMReg Function = gpr.GetReg();
	
	MOVI2R(Addr, inst.SIMM_16);
	ADD(Addr, Addr, RA);

	// Check for DSI exception prior to writing back address
	LDR(Function, R9, PPCSTATE_OFF(Exceptions));
	CMP(Function, EXCEPTION_DSI);
	FixupBranch DoNotWrite = B_CC(CC_EQ);
	MOV(RA, Addr);
	SetJumpTarget(DoNotWrite);

	MOV(ValueReg, RS);
	
	MOVI2R(Function, (u32)&Memory::Write_U16);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, ValueReg);
	MOV(R1, Addr);
	BL(Function);
	POP(4, R0, R1, R2, R3);

	gpr.Unlock(ValueReg, Addr, Function);
}

void JitArm::stw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RS = gpr.R(inst.RS);
	{
		ARMReg ValueReg = gpr.GetReg();
		ARMReg Addr = gpr.GetReg();
		ARMReg Function = gpr.GetReg();

		MOV(ValueReg, RS);
		if (inst.RA)
		{
			MOVI2R(Addr, inst.SIMM_16);
			ARMReg RA = gpr.R(inst.RA);
			ADD(Addr, Addr, RA);
		}
		else
			MOVI2R(Addr, (u32)inst.SIMM_16);
		
		MOVI2R(Function, (u32)&Memory::Write_U32);	
		PUSH(4, R0, R1, R2, R3);
		MOV(R0, ValueReg);
		MOV(R1, Addr);
		BL(Function);
		POP(4, R0, R1, R2, R3);
		gpr.Unlock(ValueReg, Addr, Function);
	}
}
void JitArm::stwu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg ValueReg = gpr.GetReg();
	ARMReg Addr = gpr.GetReg();
	ARMReg Function = gpr.GetReg();
	
	MOVI2R(Addr, inst.SIMM_16);
	ADD(Addr, Addr, RA);

	// Check for DSI exception prior to writing back address
	LDR(Function, R9, PPCSTATE_OFF(Exceptions));
	CMP(Function, EXCEPTION_DSI);
	FixupBranch DoNotWrite = B_CC(CC_EQ);
	MOV(RA, Addr);
	SetJumpTarget(DoNotWrite);

	MOV(ValueReg, RS);
	
	MOVI2R(Function, (u32)&Memory::Write_U32);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, ValueReg);
	MOV(R1, Addr);
	BL(Function);
	POP(4, R0, R1, R2, R3);

	gpr.Unlock(ValueReg, Addr, Function);
}
void JitArm::lbz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	{
		if (inst.RA)
		{
			MOVI2R(rB, inst.SIMM_16);
			ARMReg RA = gpr.R(inst.RA);
			ADD(rB, rB, RA);
		}
		else	
			MOVI2R(rB, (u32)inst.SIMM_16);
		
		MOVI2R(rA, (u32)&Memory::Read_U8);	
		PUSH(4, R0, R1, R2, R3);
		MOV(R0, rB);
		BL(rA);
		MOV(rA, R0);
		POP(4, R0, R1, R2, R3);
		MOV(RD, rA);
		gpr.Unlock(rA, rB);
	}
	SetJumpTarget(DoNotLoad);
}

void JitArm::lhz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else	
		MOVI2R(rB, (u32)inst.SIMM_16);
	
	MOVI2R(rA, (u32)&Memory::Read_U16);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);
	MOV(rA, R0);
	POP(4, R0, R1, R2, R3);
	MOV(RD, rA);
	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}
void JitArm::lha(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	if (inst.RA)
	{
		MOVI2R(rB, inst.SIMM_16);
		ARMReg RA = gpr.R(inst.RA);
		ADD(rB, rB, RA);
	}
	else	
		MOVI2R(rB, (u32)inst.SIMM_16);
	
	MOVI2R(rA, (u32)&Memory::Read_U16);	
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	BL(rA);
	MOV(rA, R0);
	SXTH(rA, rA);
	POP(4, R0, R1, R2, R3);
	MOV(RD, rA);
	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::lwz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	{
		if (inst.RA)
		{
			MOVI2R(rB, inst.SIMM_16);
			ARMReg RA = gpr.R(inst.RA);
			ADD(rB, rB, RA);
		}
		else
			MOVI2R(rB, (u32)inst.SIMM_16);

		MOVI2R(rA, (u32)&Memory::Read_U32);	
		PUSH(4, R0, R1, R2, R3);
		MOV(R0, rB);
		BL(rA);
		MOV(rA, R0);
		POP(4, R0, R1, R2, R3);
		MOV(RD, rA);
		gpr.Unlock(rA, rB);
	}
	SetJumpTarget(DoNotLoad);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
		(inst.hex & 0xFFFF0000) == 0x800D0000 &&
		(Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
		(SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
		Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		gpr.Flush();
		fpr.Flush();
		
		// if it's still 0, we can wait until the next event
		TST(RD, RD);
		FixupBranch noIdle = B_CC(CC_NEQ);
		rA = gpr.GetReg();	
		
		MOVI2R(rA, (u32)&PowerPC::OnIdle);
		MOVI2R(R0, PowerPC::ppcState.gpr[inst.RA] + (s32)(s16)inst.SIMM_16); 
		BL(rA);

		gpr.Unlock(rA);
		WriteExceptionExit();

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}
}
void JitArm::lwzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	ARMReg RB = gpr.R(inst.RB);
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	{
		if (inst.RA)
		{
			ARMReg RA = gpr.R(inst.RA);
			ADD(rB, RA, RB);
		}
		else
			MOV(rB, RB);
		
		MOVI2R(rA, (u32)&Memory::Read_U32);	
		PUSH(4, R0, R1, R2, R3);
		MOV(R0, rB);
		BL(rA);
		MOV(rA, R0);
		POP(4, R0, R1, R2, R3);
		MOV(RD, rA);
		gpr.Unlock(rA, rB);
	}
	SetJumpTarget(DoNotLoad);
	////	u32 temp = Memory::Read_U32(_inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB]);
}

void JitArm::dcbst(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	// If the dcbst instruction is preceded by dcbt, it is flushing a prefetched
	// memory location.  Do not invalidate the JIT cache in this case as the memory
	// will be the same.
	// dcbt = 0x7c00022c
	if ((Memory::ReadUnchecked_U32(js.compilerPC - 4) & 0x7c00022c) != 0x7c00022c)
	{
		Default(inst); return;
	}
}
void JitArm::icbi(UGeckoInstruction inst)
{
	Default(inst);
	WriteExit(js.compilerPC + 4, 0);
}

