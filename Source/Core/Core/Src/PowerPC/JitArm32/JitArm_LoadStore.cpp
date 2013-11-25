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
	// All this gets replaced on backpatch
	Operand2 mask(3, 1); // ~(Memory::MEMVIEW32_MASK)
	BIC(dest, dest, mask); // 1
	MOVI2R(R14, (u32)Memory::base, false); // 2-3
	ADD(dest, dest, R14); // 4
	switch (accessSize)
	{
		case 32:
			REV(value, value); // 5
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
			STR(value, dest); // 6
		break;
		case 16:
			STRH(value, dest);
		break;
		case 8:
			STRB(value, dest);
		break;
	}
	NOP(1); // 7
}

void JitArm::SafeStoreFromReg(bool fastmem, s32 dest, u32 value, s32 regOffset, int accessSize, s32 offset)
{
	if (Core::g_CoreStartupParameter.bFastmem && fastmem)
	{
		ARMReg RA;
		ARMReg RB;
		ARMReg RS = gpr.R(value);

		if (dest != -1)
			RA = gpr.R(dest);

		if (regOffset != -1)
		{
			RB = gpr.R(regOffset);
			MOV(R10, RB);
			NOP(1);
		}
		else
			MOVI2R(R10, (u32)offset, false);

		if (dest != -1)
			ADD(R10, R10, RA);
		else
			NOP(1);

		MOV(R12, RS);
		UnsafeStoreFromReg(R10, R12, accessSize, 0);
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
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff)

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
					fastmem = true;
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
			fastmem = true;
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

void JitArm::UnsafeLoadToReg(ARMReg dest, ARMReg addr, int accessSize, s32 offset)
{
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, offset, false); // -3
	ADD(addr, addr, rA); // - 1

	// All this gets replaced on backpatch
	Operand2 mask(3, 1); // ~(Memory::MEMVIEW32_MASK)
	BIC(addr, addr, mask); // 1
	MOVI2R(rA, (u32)Memory::base, false); // 2-3
	ADD(addr, addr, rA); // 4
	switch (accessSize)
	{
		case 32:
			LDR(dest, addr); // 5
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
			REV(dest, dest); // 6
		break;
		case 16:
			REV16(dest, dest);
		break;
		case 8:
			NOP(1);
		break;

	}
	NOP(2); // 7-8
	gpr.Unlock(rA);
}

void JitArm::SafeLoadToReg(bool fastmem, u32 dest, s32 addr, s32 offsetReg, int accessSize, s32 offset, bool signExtend, bool reverse)
{
	ARMReg RD = gpr.R(dest);
	if (Core::g_CoreStartupParameter.bFastmem && fastmem)
	{
		if (addr != -1)
			MOV(R10, gpr.R(addr));
		else
			MOV(R10, 0);

		UnsafeLoadToReg(RD, R10, accessSize, offset);
		return;
	}
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();

	if (offsetReg == -1)
		MOVI2R(rA, offset);
	else
		MOV(rA, gpr.R(offsetReg));

	if (addr != -1)
		ADD(rA, rA, gpr.R(addr));

	switch (accessSize)
	{
		case 8:
			MOVI2R(rB, (u32)&Memory::Read_U8);
		break;
		case 16:
			MOVI2R(rB, (u32)&Memory::Read_U16);
		break;
		case 32:
			MOVI2R(rB, (u32)&Memory::Read_U32);
		break;
	}
	PUSH(4, R0, R1, R2, R3);
	MOV(R0, rA);
	BL(rB);
	MOV(rA, R0);
	POP(4, R0, R1, R2, R3);
	MOV(RD, rA);
	if (signExtend) // Only on 16 loads
		SXTH(RD, RD);
	if (reverse)
	{
		if (accessSize == 32)
			REV(RD, RD);
		else if (accessSize == 16)
			REV16(RD, RD);
	}
	gpr.Unlock(rA, rB);
}

void JitArm::lXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff)

	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	s32 offset = inst.SIMM_16;
	u32 accessSize = 0;
	s32 offsetReg = -1;
	bool zeroA = true;
	bool update = false;
	bool signExtend = false;
	bool reverse = false;
	bool fastmem = false;

	switch(inst.OPCD)
	{
		case 31:
			switch(inst.SUBOP10)
			{
				case 55: // lwzux
					zeroA = false;
					update = true;
				case 23: // lwzx
					accessSize = 32;
					offsetReg = b;
				break;
				case 119: //lbzux
					zeroA = false;
					update = true;
				case 87: // lbzx
					accessSize = 8;
					offsetReg = b;
				break;
				case 311: // lhzux
					zeroA = false;
					update = true;
				case 279: // lhzx
					accessSize = 16;
					offsetReg = b;
				break;
				case 375: // lhaux
					zeroA = false;
					update = true;
				case 343: // lhax
					accessSize = 16;
					signExtend = true;
					offsetReg = b;
				break;
				case 534: // lwbrx
					accessSize = 32;
					reverse = true;
				break;
				case 790: // lhbrx
					accessSize = 16;
					reverse = true;
				break;
			}
		break;
		case 33: // lwzu
			zeroA = false;
			update = true;
		case 32: // lwz
			fastmem = true;
			accessSize = 32;
		break;
		case 35: // lbzu
			zeroA = false;
			update = true;
		case 34: // lbz
			fastmem = true;
			accessSize = 8;
		break;
		case 41: // lhzu
			zeroA = false;
			update = true;
		case 40: // lhz
			fastmem = true;
			accessSize = 16;
		break;
		case 43: // lhau
			zeroA = false;
			update = true;
		case 42: // lha
			signExtend = true;
			accessSize = 16;
		break;
	}

	// Check for exception before loading
	ARMReg rA = gpr.GetReg(false);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	SafeLoadToReg(fastmem, d, zeroA ? a ? a : -1 : a, offsetReg, accessSize, offset, signExtend, reverse);

	if (update)
	{
		rA = gpr.GetReg(false);
		ARMReg RA = gpr.R(a);
		if (offsetReg == -1)
			MOVI2R(rA, offset);
		else
			MOV(RA, gpr.R(offsetReg));
		ADD(RA, RA, rA);
	}

	SetJumpTarget(DoNotLoad);

	// LWZ idle skipping
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
		inst.OPCD == 32 &&
		(inst.hex & 0xFFFF0000) == 0x800D0000 &&
		(Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
		(SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
		Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		ARMReg RD = gpr.R(d);
		gpr.Flush();
		fpr.Flush();

		// if it's still 0, we can wait until the next event
		TST(RD, RD);
		FixupBranch noIdle = B_CC(CC_NEQ);
		rA = gpr.GetReg();

		MOVI2R(rA, (u32)&PowerPC::OnIdle);
		MOVI2R(R0, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);
		BL(rA);

		gpr.Unlock(rA);
		WriteExceptionExit();

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}

}

// Some games use this heavily in video codecs
// We make the assumption that this pulls from main RAM at /all/ times
void JitArm::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff)
	if (!Core::g_CoreStartupParameter.bFastmem){
		Default(inst); return;
	}

	u32 a = inst.RA;
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	MOVI2R(rA, inst.SIMM_16);
	if (a)
		ADD(rA, rA, gpr.R(a));
	Operand2 mask(3, 1); // ~(Memory::MEMVIEW32_MASK)
	BIC(rA, rA, mask); // 3
	MOVI2R(rB, (u32)Memory::base, false); // 4-5
	ADD(rA, rA, rB); // 6

	for (int i = inst.RD; i < 32; i++)
	{
		ARMReg RX = gpr.R(i);
		LDR(RX, rA, (i - inst.RD) * 4);
		REV(RX, RX);
	}
	gpr.Unlock(rA, rB);
}

void JitArm::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff)
	if (!Core::g_CoreStartupParameter.bFastmem){
		Default(inst); return;
	}

	u32 a = inst.RA;
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg rC = gpr.GetReg();
	MOVI2R(rA, inst.SIMM_16);
	if (a)
		ADD(rA, rA, gpr.R(a));
	Operand2 mask(3, 1); // ~(Memory::MEMVIEW32_MASK)
	BIC(rA, rA, mask); // 3
	MOVI2R(rB, (u32)Memory::base, false); // 4-5
	ADD(rA, rA, rB); // 6

	for (int i = inst.RD; i < 32; i++)
	{
		ARMReg RX = gpr.R(i);
		REV(rC, RX);
		STR(rC, rA, (i - inst.RD) * 4);
	}
	gpr.Unlock(rA, rB, rC);
}
void JitArm::dcbst(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff)

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

