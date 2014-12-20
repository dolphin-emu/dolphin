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
#include "Core/PowerPC/JitArm32/JitFPRCache.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

using namespace ArmGen;

void JitArm::lfXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	ARMReg RA;

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	u32 flags = BackPatchInfo::FLAG_LOAD;
	bool update = false;
	s32 offsetReg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 567: // lfsux
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					update = true;
					offsetReg = b;
				break;
				case 535: // lfsx
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offsetReg = b;
				break;
				case 631: // lfdux
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					update = true;
					offsetReg = b;
				break;
				case 599: // lfdx
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					offsetReg = b;
				break;
			}
		break;
		case 49: // lfsu
			flags |= BackPatchInfo::FLAG_SIZE_F32;
			update = true;
		break;
		case 48: // lfs
			flags |= BackPatchInfo::FLAG_SIZE_F32;
		break;
		case 51: // lfdu
			flags |= BackPatchInfo::FLAG_SIZE_F64;
			update = true;
		break;
		case 50: // lfd
			flags |= BackPatchInfo::FLAG_SIZE_F64;
		break;
	}

	ARMReg v0 = fpr.R0(inst.FD, false), v1 = INVALID_REG;
	if (flags & BackPatchInfo::FLAG_SIZE_F32)
		v1 = fpr.R1(inst.FD, false);

	ARMReg rA = R11;
	ARMReg addr = R12;

	u32 imm_addr = 0;
	bool is_immediate = false;
	if (update)
	{
		// Always uses RA
		if (gpr.IsImm(a) && offsetReg == -1)
		{
			is_immediate = true;
			imm_addr = offset + gpr.GetImm(a);
		}
		else if (gpr.IsImm(a) && offsetReg != -1 && gpr.IsImm(offsetReg))
		{
			is_immediate = true;
			imm_addr = gpr.GetImm(a) + gpr.GetImm(offsetReg);
		}
		else
		{
			if (offsetReg == -1)
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(addr, gpr.R(a), off);
				}
				else
				{
					MOVI2R(addr, offset);
					ADD(addr, addr, gpr.R(a));
				}
			}
			else
			{
				ADD(addr, gpr.R(offsetReg), gpr.R(a));
			}
		}
	}
	else
	{
		if (offsetReg == -1)
		{
			if (a && gpr.IsImm(a))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + offset;
			}
			else if (a)
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(addr, gpr.R(a), off);
				}
				else
				{
					MOVI2R(addr, offset);
					ADD(addr, addr, gpr.R(a));
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
			if (a && gpr.IsImm(a) && gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + gpr.GetImm(offsetReg);
			}
			else if (!a && gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(offsetReg);
			}
			else if (a)
			{
				ADD(addr, gpr.R(a), gpr.R(offsetReg));
			}
			else
			{
				MOV(addr, gpr.R(offsetReg));
			}
		}
	}

	if (update)
		RA = gpr.R(a);

	if (is_immediate)
		MOVI2R(addr, imm_addr);

	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	if (update)
		MOV(RA, addr);

	EmitBackpatchRoutine(this, flags,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			!(is_immediate && Memory::IsRAMAddress(imm_addr)), v0, v1);

	SetJumpTarget(DoNotLoad);
}

void JitArm::stfXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	ARMReg RA;

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	u32 flags = BackPatchInfo::FLAG_STORE;
	bool update = false;
	s32 offsetReg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 663: // stfsx
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offsetReg = b;
				break;
				case 695: // stfsux
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offsetReg = b;
				break;
				case 727: // stfdx
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					offsetReg = b;
				break;
				case 759: // stfdux
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					update = true;
					offsetReg = b;
				break;
			}
		break;
		case 53: // stfsu
			flags |= BackPatchInfo::FLAG_SIZE_F32;
			update = true;
		break;
		case 52: // stfs
			flags |= BackPatchInfo::FLAG_SIZE_F32;
		break;
		case 55: // stfdu
			flags |= BackPatchInfo::FLAG_SIZE_F64;
			update = true;
		break;
		case 54: // stfd
			flags |= BackPatchInfo::FLAG_SIZE_F64;
		break;
	}

	ARMReg v0 = fpr.R0(inst.FS);

	ARMReg rA = R11;
	ARMReg addr = R12;

	u32 imm_addr = 0;
	bool is_immediate = false;
	if (update)
	{
		// Always uses RA
		if (gpr.IsImm(a) && offsetReg == -1)
		{
			is_immediate = true;
			imm_addr = offset + gpr.GetImm(a);
		}
		else if (gpr.IsImm(a) && offsetReg != -1 && gpr.IsImm(offsetReg))
		{
			is_immediate = true;
			imm_addr = gpr.GetImm(a) + gpr.GetImm(offsetReg);
		}
		else
		{
			if (offsetReg == -1)
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(addr, gpr.R(a), off);
				}
				else
				{
					MOVI2R(addr, offset);
					ADD(addr, addr, gpr.R(a));
				}
			}
			else
			{
				ADD(addr, gpr.R(offsetReg), gpr.R(a));
			}
		}
	}
	else
	{
		if (offsetReg == -1)
		{
			if (a && gpr.IsImm(a))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + offset;
			}
			else if (a)
			{
				Operand2 off;
				if (TryMakeOperand2(offset, off))
				{
					ADD(addr, gpr.R(a), off);
				}
				else
				{
					MOVI2R(addr, offset);
					ADD(addr, addr, gpr.R(a));
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
			if (a && gpr.IsImm(a) && gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + gpr.GetImm(offsetReg);
			}
			else if (!a && gpr.IsImm(offsetReg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(offsetReg);
			}
			else if (a)
			{
				ADD(addr, gpr.R(a), gpr.R(offsetReg));
			}
			else
			{
				MOV(addr, gpr.R(offsetReg));
			}
		}
	}

	if (is_immediate)
		MOVI2R(addr, imm_addr);

	if (update)
	{
		RA = gpr.R(a);
		LDR(rA, R9, PPCSTATE_OFF(Exceptions));
		CMP(rA, EXCEPTION_DSI);

		SetCC(CC_NEQ);
		MOV(RA, addr);
		SetCC();
	}

	if (is_immediate)
	{
		if ((imm_addr & 0xFFFFF000) == 0xCC008000 && jit->jo.optimizeGatherPipe)
		{
			int accessSize;
			if (flags & BackPatchInfo::FLAG_SIZE_F64)
				accessSize = 64;
			else
				accessSize = 32;

			MOVI2R(R14, (u32)&GPFifo::m_gatherPipeCount);
			MOVI2R(R10, (u32)GPFifo::m_gatherPipe);
			LDR(R11, R14);
			ADD(R10, R10, R11);
			NEONXEmitter nemit(this);
			if (accessSize == 64)
			{
				PUSH(2, R0, R1);
				nemit.VREV64(I_8, D0, v0);
				VMOV(R0, D0);
				STR(R0, R10, 0);
				STR(R1, R10, 4);
				POP(2, R0, R1);
			}
			else if (accessSize == 32)
			{
				VCVT(S0, v0, 0);
				nemit.VREV32(I_8, D0, D0);
				VMOV(addr, S0);
				STR(addr, R10);
			}
			ADD(R11, R11, accessSize >> 3);
			STR(R11, R14);
			jit->js.fifoBytesThisBlock += accessSize >> 3;

		}
		else if (Memory::IsRAMAddress(imm_addr))
		{
			MOVI2R(addr, imm_addr);
			EmitBackpatchRoutine(this, flags, SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem, false, v0);
		}
		else
		{
			MOVI2R(addr, imm_addr);
			EmitBackpatchRoutine(this, flags, false, false, v0);
		}
	}
	else
	{
		EmitBackpatchRoutine(this, flags, SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem, true, v0);
	}
}

