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

#ifdef ANDROID
#define FASTMEM 0
#else
#define FASTMEM 0
#endif
void JitArm::stw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg RS = gpr.R(inst.RS);
#if FASTMEM
	// R10 contains the dest address
	ARMReg Value = R11;
	ARMReg RA;
	if (inst.RA)
		RA = gpr.R(inst.RA);
	MOV(Value, RS);
	if (inst.RA)
	{
		MOVI2R(R10, inst.SIMM_16, false);
		ADD(R10, R10, RA);
	}
	else
	{
		MOVI2R(R10, (u32)inst.SIMM_16, false);
		NOP(1);
	}
	StoreFromReg(R10, Value, 32, 0);
#else
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
#endif
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

	// Check and set the update before writing since calling a function can
	// mess with the "special registers R11+ which may cause some issues.
	LDR(Function, R9, STRUCT_OFF(PowerPC::ppcState, Exceptions));
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
void JitArm::StoreFromReg(ARMReg dest, ARMReg value, int accessSize, s32 offset)
{
	ARMReg rA = gpr.GetReg();

	// All this gets replaced on backpatch
	MOVI2R(rA, Memory::MEMVIEW32_MASK, false); // 1-2 
	AND(dest, dest, rA); // 3
	MOVI2R(rA, (u32)Memory::base, false); // 4-5
	ADD(dest, dest, rA); // 6
	switch (accessSize)
	{
		case 32:
			REV(value, value); // 7
		break;
		case 16:
			REV16(value, value);
		break;
		case 8:
			NOP(1);
		break;
	}
	switch (accessSize)
	{
		case 32:
			STR(dest, value); // 8
		break;
		case 16:
			// Not implemented
		break;
		case 8:
			// Not implemented
		break;
	}
	gpr.Unlock(rA);
}
void JitArm::LoadToReg(ARMReg dest, ARMReg addr, int accessSize, s32 offset)
{
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, offset, false); // -3
	ADD(addr, addr, rA); // - 1

	// All this gets replaced on backpatch
	MOVI2R(rA, Memory::MEMVIEW32_MASK, false); // 2 
	AND(addr, addr, rA); // 3
	MOVI2R(rA, (u32)Memory::base, false); // 5
	ADD(addr, addr, rA); // 6
	switch (accessSize)
	{
		case 32:
			LDR(dest, addr); // 7
		break;
		case 16:
			LDRH(dest, addr);
		break;
		case 8:
			LDRB(dest, addr);
		break;
	}
	switch (accessSize)
	{
		case 32:
			REV(dest, dest); // 9
		break;
		case 16:
			REV16(dest, dest);
		break;
		case 8:
			NOP(1);
		break;

	}
	gpr.Unlock(rA);
}
void JitArm::lbz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, STRUCT_OFF(PowerPC::ppcState, Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	gpr.Unlock(rA, rB);
	if (inst.RA)
	{
		ARMReg RA = gpr.R(inst.RA);
		MOV(R10, RA); // - 4
	}
	else
		MOV(R10, 0); // - 4
	LoadToReg(RD, R10, 8, inst.SIMM_16);	
#else

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
#endif
	SetJumpTarget(DoNotLoad);
}

void JitArm::lhz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, STRUCT_OFF(PowerPC::ppcState, Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
#if 0 // FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	gpr.Unlock(rA, rB);
	if (inst.RA)
	{
		ARMReg RA = gpr.R(inst.RA);
		printf("lhz jump to here: 0x%08x\n", (u32)GetCodePtr());
		MOV(R10, RA); // - 4
	}
	else
	{
		printf("lhz jump to here: 0x%08x\n", (u32)GetCodePtr());
		MOV(R10, 0); // - 4
	}
	LoadToReg(RD, R10, 16, (u32)inst.SIMM_16);	
#else

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
#endif
	SetJumpTarget(DoNotLoad);
}
void JitArm::lwz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RD = gpr.R(inst.RD);
	LDR(rA, R9, STRUCT_OFF(PowerPC::ppcState, Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
	
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	gpr.Unlock(rA, rB);
	if (inst.RA)
	{
		ARMReg RA = gpr.R(inst.RA);
		MOV(R10, RA); // - 4
	}
	else
		MOV(R10, 0); // - 4
	LoadToReg(RD, R10, 32, (u32)inst.SIMM_16);	
#else
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
#endif
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
	LDR(rA, R9, STRUCT_OFF(PowerPC::ppcState, Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	gpr.Unlock(rA, rB);
	if (inst.RA)
	{
		ARMReg RA = gpr.R(inst.RA);
		ADD(R10, RA, RB); // - 4
	}
	else
		MOV(R10, RB); // -4
	LoadToReg(RD, R10, 32, 0);	
#else
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
#endif
	SetJumpTarget(DoNotLoad);
	////	u32 temp = Memory::Read_U32(_inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB]);
}
void JitArm::icbi(UGeckoInstruction inst)
{
	Default(inst);
	WriteExit(js.compilerPC + 4, 0);
}

