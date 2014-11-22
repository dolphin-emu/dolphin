// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

using namespace ArmGen;

void JitArm::SafeStoreFromReg(s32 dest, u32 value, s32 regOffset, int accessSize, s32 offset)
{
	// We want to make sure to not get LR as a temp register
	ARMReg rA = R12;

	u32 imm_addr = 0;
	bool is_immediate = false;

	if (regOffset == -1)
	{
		if (dest != -1)
		{
			if (gpr.IsImm(dest))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(dest) + offset;
			}
			else
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(rA, gpr.R(dest), off);
				}
				else
				{
					MOVI2R(rA, offset);
					ADD(rA, rA, gpr.R(dest));
				}
			}
		}
		else
		{
			is_immediate = true;
			imm_addr = offset;
		}
	}
	else
	{
		if (dest != -1)
		{
			if (gpr.IsImm(dest) && gpr.IsImm(regOffset))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(dest) + gpr.GetImm(regOffset);
			}
			else if (gpr.IsImm(dest) && !gpr.IsImm(regOffset))
			{
				Operand2 off;
				if (TryMakeOperand2(gpr.GetImm(dest), off))
				{
					ADD(rA, gpr.R(regOffset), off);
				}
				else
				{
					MOVI2R(rA, gpr.GetImm(dest));
					ADD(rA, rA, gpr.R(regOffset));
				}
			}
			else if (!gpr.IsImm(dest) && gpr.IsImm(regOffset))
			{
				Operand2 off;
				if (TryMakeOperand2(gpr.GetImm(regOffset), off))
				{
					ADD(rA, gpr.R(dest), off);
				}
				else
				{
					MOVI2R(rA, gpr.GetImm(regOffset));
					ADD(rA, rA, gpr.R(dest));
				}
			}
			else
			{
				ADD(rA, gpr.R(dest), gpr.R(regOffset));
			}
		}
		else
		{
			if (gpr.IsImm(regOffset))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(regOffset);
			}
			else
			{
				MOV(rA, gpr.R(regOffset));
			}
		}
	}
	ARMReg RS = gpr.R(value);

	u32 flags = BackPatchInfo::FLAG_STORE;
	if (accessSize == 32)
		flags |= BackPatchInfo::FLAG_SIZE_32;
	else if (accessSize == 16)
		flags |= BackPatchInfo::FLAG_SIZE_16;
	else
		flags |= BackPatchInfo::FLAG_SIZE_8;

	if (is_immediate)
	{
		if ((imm_addr & 0xFFFFF000) == 0xCC008000 && jit->jo.optimizeGatherPipe)
		{
			MOVI2R(R14, (u32)&GPFifo::m_gatherPipeCount);
			MOVI2R(R10, (u32)GPFifo::m_gatherPipe);
			LDR(R11, R14);
			if (accessSize == 32)
			{
				REV(RS, RS);
				STR(RS, R10, R11);
				REV(RS, RS);
			}
			else if (accessSize == 16)
			{
				REV16(RS, RS);
				STRH(RS, R10, R11);
				REV16(RS, RS);
			}
			else
			{
				STRB(RS, R10, R11);
			}
			ADD(R11, R11, accessSize >> 3);
			STR(R11, R14);
			jit->js.fifoBytesThisBlock += accessSize >> 3;
		}
		else if (Memory::IsRAMAddress(imm_addr))
		{
			MOVI2R(rA, imm_addr);
			EmitBackpatchRoutine(this, flags, SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem, false, RS);
		}
		else
		{
			MOVI2R(rA, imm_addr);
			EmitBackpatchRoutine(this, flags, false, false, RS);
		}
	}
	else
	{
		EmitBackpatchRoutine(this, flags, SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem, true, RS);
	}

}

void JitArm::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	u32 a = inst.RA, b = inst.RB, s = inst.RS;
	s32 offset = inst.SIMM_16;
	u32 accessSize = 0;
	s32 regOffset = -1;
	bool update = false;
	switch (inst.OPCD)
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
					update = true;
				case 151: // stwx
					accessSize = 32;
					regOffset = b;
				break;
				case 247: // stbux
					update = true;
				case 215: // stbx
					accessSize = 8;
					regOffset = b;
				break;
				case 439: // sthux
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

	SafeStoreFromReg(update ? a : (a ? a : -1), s, regOffset, accessSize, offset);

	if (update)
	{
		ARMReg rA = gpr.GetReg();
		ARMReg RB;
		ARMReg RA = gpr.R(a);
		if (regOffset != -1)
			RB = gpr.R(regOffset);
		// Check for DSI exception prior to writing back address
		LDR(rA, R9, PPCSTATE_OFF(Exceptions));
		TST(rA, EXCEPTION_DSI);
		SetCC(CC_EQ);
		if (regOffset == -1)
		{
			MOVI2R(rA, offset);
			ADD(RA, RA, rA);
		}
		else
		{
			ADD(RA, RA, RB);
		}
		SetCC();
		gpr.Unlock(rA);
	}
}

void JitArm::SafeLoadToReg(ARMReg dest, s32 addr, s32 offsetReg, int accessSize, s32 offset, bool signExtend, bool reverse, bool update)
{
	// We want to make sure to not get LR as a temp register
	ARMReg rA = R12;

	u32 imm_addr = 0;
	bool is_immediate = false;

	if (offsetReg == -1)
	{
		if (addr != -1)
		{
			if (gpr.IsImm(addr))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(addr) + offset;
			}
			else
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(rA, gpr.R(addr), off);
				}
				else
				{
					MOVI2R(rA, offset);
					ADD(rA, rA, gpr.R(addr));
				}
			}
		}
		else
		{
			is_immediate = true;
			imm_addr = offset;
		}
	}
	else
	{
		if (addr != -1)
		{
			if (gpr.IsImm(addr) && gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(addr) + gpr.GetImm(offsetReg);
			}
			else if (gpr.IsImm(addr) && !gpr.IsImm(offsetReg))
			{
				Operand2 off;
				if (TryMakeOperand2(gpr.GetImm(addr), off))
				{
					ADD(rA, gpr.R(offsetReg), off);
				}
				else
				{
					MOVI2R(rA, gpr.GetImm(addr));
					ADD(rA, rA, gpr.R(offsetReg));
				}
			}
			else if (!gpr.IsImm(addr) && gpr.IsImm(offsetReg))
			{
				Operand2 off;
				if (TryMakeOperand2(gpr.GetImm(offsetReg), off))
				{
					ADD(rA, gpr.R(addr), off);
				}
				else
				{
					MOVI2R(rA, gpr.GetImm(offsetReg));
					ADD(rA, rA, gpr.R(addr));
				}
			}
			else
			{
				ADD(rA, gpr.R(addr), gpr.R(offsetReg));
			}
		}
		else
		{
			if (gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(offsetReg);
			}
			else
			{
				MOV(rA, gpr.R(offsetReg));
			}
		}
	}

	if (is_immediate)
		MOVI2R(rA, imm_addr);

	u32 flags = BackPatchInfo::FLAG_LOAD;
	if (accessSize == 32)
		flags |= BackPatchInfo::FLAG_SIZE_32;
	else if (accessSize == 16)
		flags |= BackPatchInfo::FLAG_SIZE_16;
	else
		flags |= BackPatchInfo::FLAG_SIZE_8;

	if (reverse)
		flags |= BackPatchInfo::FLAG_REVERSE;

	EmitBackpatchRoutine(this, flags,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			!(is_immediate && Memory::IsRAMAddress(imm_addr)), dest);

	if (signExtend) // Only on 16 loads
		SXTH(dest, dest);

	if (update)
		MOV(gpr.R(addr), rA);
}

void JitArm::lXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	s32 offset = inst.SIMM_16;
	u32 accessSize = 0;
	s32 offsetReg = -1;
	bool update = false;
	bool signExtend = false;
	bool reverse = false;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 55: // lwzux
					update = true;
				case 23: // lwzx
					accessSize = 32;
					offsetReg = b;
				break;
				case 119: //lbzux
					update = true;
				case 87: // lbzx
					accessSize = 8;
					offsetReg = b;
				break;
				case 311: // lhzux
					update = true;
				case 279: // lhzx
					accessSize = 16;
					offsetReg = b;
				break;
				case 375: // lhaux
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
			update = true;
		case 32: // lwz
			accessSize = 32;
		break;
		case 35: // lbzu
			update = true;
		case 34: // lbz
			accessSize = 8;
		break;
		case 41: // lhzu
			update = true;
		case 40: // lhz
			accessSize = 16;
		break;
		case 43: // lhau
			update = true;
		case 42: // lha
			signExtend = true;
			accessSize = 16;
		break;
	}

	// Check for exception before loading
	ARMReg rA = gpr.GetReg(false);
	ARMReg RD = gpr.R(d);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	TST(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_NEQ);

	SafeLoadToReg(RD, update ? a : (a ? a : -1), offsetReg, accessSize, offset, signExtend, reverse, update);

	SetJumpTarget(DoNotLoad);

	// LWZ idle skipping
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
	    inst.OPCD == 32 &&
	    (inst.hex & 0xFFFF0000) == 0x800D0000 &&
	    (Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
	    (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
	    Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		// if it's still 0, we can wait until the next event
		TST(RD, RD);
		FixupBranch noIdle = B_CC(CC_NEQ);

		gpr.Flush(FLUSH_MAINTAIN_STATE);
		fpr.Flush(FLUSH_MAINTAIN_STATE);

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
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(!SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem);

	u32 a = inst.RA;
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	MOVI2R(rA, inst.SIMM_16);
	if (a)
		ADD(rA, rA, gpr.R(a));
	Operand2 mask(2, 1); // ~(Memory::MEMVIEW32_MASK)
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
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(!SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem);

	u32 a = inst.RA;
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg rC = gpr.GetReg();
	MOVI2R(rA, inst.SIMM_16);
	if (a)
		ADD(rA, rA, gpr.R(a));
	Operand2 mask(2, 1); // ~(Memory::MEMVIEW32_MASK)
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
	JITDISABLE(bJITLoadStoreOff);

	// If the dcbst instruction is preceded by dcbt, it is flushing a prefetched
	// memory location.  Do not invalidate the JIT cache in this case as the memory
	// will be the same.
	// dcbt = 0x7c00022c
	FALLBACK_IF((Memory::ReadUnchecked_U32(js.compilerPC - 4) & 0x7c00022c) != 0x7c00022c);
}

void JitArm::icbi(UGeckoInstruction inst)
{
	FallBackToInterpreter(inst);
	WriteExit(js.compilerPC + 4);
}

