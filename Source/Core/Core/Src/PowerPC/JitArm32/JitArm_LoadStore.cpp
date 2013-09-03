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

void JitArm::UnsafeStoreFromReg(ARMReg dest, ARMReg value, int accessSize, s32 offset)
{
	ARMReg rA = R11;

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
			STR(value, dest); // 8
		break;
		case 16:
			STRH(value, dest);
		break;
		case 8:
			STRB(value, dest);
		break;
	}
}

void JitArm::SafeStoreFromReg(bool fastmem, s32 dest, u32 value, s32 regOffset, int accessSize, s32 offset)
{
	if (Core::g_CoreStartupParameter.bFastmem && fastmem)
	{
		ARMReg rA = R10;
		ARMReg rB = R12;
		ARMReg RA;
		ARMReg RB;
		ARMReg RS = gpr.R(value);

		if (regOffset != -1)
		{
			RB = gpr.R(regOffset);
			MOV(rA, RB);
		}
		else
			MOVI2R(rA, offset);

		if (dest != -1)
		{
			RA = gpr.R(dest);
			ADD(rA, rA, RA);
		}

		MOV(rB, RS);
		UnsafeStoreFromReg(rA, rB, accessSize, 0);
		return;
	}
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg rC = gpr.GetReg();
	ARMReg RA;
	ARMReg RB;
	if (dest != -1)
		RA = gpr.R(dest);
	if (regOffset != -1)
		RB = gpr.R(regOffset);
	ARMReg RS = gpr.R(value);
	switch(accessSize)
	{
		case 32:
			MOVI2R(rA, (u32)&Memory::Write_U32);	
		break;
		case 16:
			MOVI2R(rA, (u32)&Memory::Write_U16);	
		break;
		case 8:
			MOVI2R(rA, (u32)&Memory::Write_U8);	
		break;
	}
	MOV(rB, RS);
	if (regOffset == -1)
		MOVI2R(rC, offset);
	else
		MOV(rC, RB);
	if (dest != -1)
		ADD(rC, rC, RA);

	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rB);
	MOV(R1, rC);
	BL(rA);
	POP(4, R0, R1, R2, R3);
	gpr.Unlock(rA, rB, rC);
}

void JitArm::stX(UGeckoInstruction inst)
{
	u32 a = inst.RA, b = inst.RB, s = inst.RS;
	s32 offset = inst.SIMM_16;
	u32 accessSize = 0;
	s32 regOffset = -1;
	bool zeroA = true;
	bool update = false;
	bool fastmem = false;
	switch(inst.OPCD)
	{
		case 45: // sthu
			update = true;
		case 44: // sth
			accessSize = 16;
		break;
		case 31:
			switch (inst.SUBOP10)
			{
				case 183: // stwux
					zeroA = false;
					update = true;
				case 151: // stwx
					accessSize = 32;
					regOffset = b;
				break;
				case 247: // stbux
					zeroA = false;
					update = true;
				case 215: // stbx
					accessSize = 8;
					regOffset = b;
				break;
				case 439: // sthux
					zeroA = false;
					update = true;
				case 407: // sthx
					accessSize = 16;
					regOffset = b;
				break;
			}
		break;
		case 37: // stwu
			update = true;
		case 36: // stw
			accessSize = 32;
		break;
		case 39: // stbu
			update = true;
		case 38: // stb
			accessSize = 8;
		break;
	}
	SafeStoreFromReg(fastmem, zeroA ? a ? a : -1 : a, s, regOffset, accessSize, offset); 
	if (update)
	{
		ARMReg rA = gpr.GetReg();
		ARMReg RB;
		ARMReg RA = gpr.R(a);
		if (regOffset != -1)
			RB = gpr.R(regOffset);
		// Check for DSI exception prior to writing back address
		LDR(rA, R9, PPCSTATE_OFF(Exceptions));
		CMP(rA, EXCEPTION_DSI);
		FixupBranch DoNotWrite = B_CC(CC_EQ);
		if (a)
		{
			if (regOffset == -1)
				MOVI2R(rA, offset);
			else
				MOV(rA, RB);
			ADD(RA, RA, rA);
		}
		else
			if (regOffset == -1)
				MOVI2R(RA, (u32)offset);
			else
				MOV(RA, RB);
		SetJumpTarget(DoNotWrite);
		gpr.Unlock(rA);
	}
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
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	if (Core::g_CoreStartupParameter.bFastmem)
	{
		gpr.Unlock(rA, rB);
		if (inst.RA)
		{
			ARMReg RA = gpr.R(inst.RA);
			MOV(R10, RA); // - 4
		}
		else
			MOV(R10, 0); // - 4
		LoadToReg(RD, R10, 8, inst.SIMM_16);	
	}
	else
#endif
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
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	if (Core::g_CoreStartupParameter.bFastmem)
	{
		if (inst.RA)
		{
			ARMReg RA = gpr.R(inst.RA);
			MOV(R10, RA); // - 4
		}
		else
			MOV(R10, 0); // - 4

		LoadToReg(RD, R10, 16, (u32)inst.SIMM_16);	
	}
	else
#endif
	{
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
	}

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
	
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	if (Core::g_CoreStartupParameter.bFastmem)
	{
		gpr.Unlock(rA, rB);
		if (inst.RA)
		{
			ARMReg RA = gpr.R(inst.RA);
			MOV(R10, RA); // - 4
		}
		else
			MOV(R10, 0); // - 4
		LoadToReg(RD, R10, 32, (u32)inst.SIMM_16);	
	}
	else
#endif
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
#if FASTMEM
	// Backpatch route
	// Gets loaded in to RD
	// Address is in R10
	if (Core::g_CoreStartupParameter.bFastmem)
	{
		gpr.Unlock(rA, rB);
		if (inst.RA)
		{
			ARMReg RA = gpr.R(inst.RA);
			ADD(R10, RA, RB); // - 4
		}
		else
			MOV(R10, RB); // -4
		LoadToReg(RD, R10, 32, 0);	
	}
	else
#endif
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

