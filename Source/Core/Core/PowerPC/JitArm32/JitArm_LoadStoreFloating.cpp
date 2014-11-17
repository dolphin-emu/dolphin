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

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RA;

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	bool single = false;
	bool update = false;
	bool zeroA = false;
	s32 offsetReg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 567: // lfsux
					single = true;
					update = true;
					offsetReg = b;
				break;
				case 535: // lfsx
					single = true;
					zeroA = true;
					offsetReg = b;
				break;
				case 631: // lfdux
					update = true;
					offsetReg = b;
				break;
				case 599: // lfdx
					zeroA = true;
					offsetReg = b;
				break;
			}
		break;
		case 49: // lfsu
			update = true;
			single = true;
		break;
		case 48: // lfs
			single = true;
			zeroA = true;
		break;
		case 51: // lfdu
			update = true;
		break;
		case 50: // lfd
			zeroA = true;
		break;
	}

	ARMReg v0 = fpr.R0(inst.FD, false), v1;
	if (single)
		v1 = fpr.R1(inst.FD, false);

	if (update)
	{
		RA = gpr.R(a);
		// Update path /always/ uses RA
		if (offsetReg == -1) // uses SIMM_16
		{
			MOVI2R(rB, offset);
			ADD(rB, rB, RA);
		}
		else
		{
			ADD(rB, gpr.R(offsetReg), RA);
		}
	}
	else
	{
		if (zeroA)
		{
			if (offsetReg == -1)
			{
				if (a)
				{
					RA = gpr.R(a);
					MOVI2R(rB, offset);
					ADD(rB, rB, RA);
				}
				else
				{
					MOVI2R(rB, (u32)offset);
				}
			}
			else
			{
				ARMReg RB = gpr.R(offsetReg);
				if (a)
				{
					RA = gpr.R(a);
					ADD(rB, RB, RA);
				}
				else
				{
					MOV(rB, RB);
				}
			}
		}
	}
	LDR(rA, R9, PPCSTATE_OFF(Exceptions));
	CMP(rA, EXCEPTION_DSI);
	FixupBranch DoNotLoad = B_CC(CC_EQ);

	if (update)
		MOV(RA, rB);

	// This branch gets changed to a NOP when the fastpath fails
	FixupBranch fast_path;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem)
		fast_path = B();

	{
		PUSH(4, R0, R1, R2, R3);
		MOV(R0, rB);
		if (single)
		{
			MOVI2R(rA, (u32)&Memory::Read_U32);
			BL(rA);
			VMOV(S0, R0);
			VCVT(v0, S0, 0);
			VCVT(v1, S0, 0);
		}
		else
		{
			MOVI2R(rA, (u32)&Memory::Read_F64);
			BL(rA);

#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
			VMOV(v0, R0);
#else
			VMOV(v0, D0);
#endif
		}
		POP(4, R0, R1, R2, R3);
	}
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem)
	{
		FixupBranch slow_out = B();
		SetJumpTarget(fast_path);
		{
			Operand2 mask(2, 1); // ~(Memory::MEMVIEW32_MASK)
			ARMReg rC = gpr.GetReg();
			BIC(rC, rB, mask);
			MOVI2R(rA, (u32)Memory::base);
			ADD(rC, rC, rA);

			NEONXEmitter nemit(this);
			if (single)
			{
				nemit.VLD1(F_32, D0, rC);
				nemit.VREV32(I_8, D0, D0); // Byte swap to result
				VCVT(v0, S0, 0);
				VCVT(v1, S0, 0);
			}
			else
			{
				nemit.VLD1(I_64, v0, rC);
				nemit.VREV64(I_8, v0, v0); // Byte swap to result
			}
			gpr.Unlock(rC);
		}
		SetJumpTarget(slow_out);
	}

	gpr.Unlock(rA, rB);
	SetJumpTarget(DoNotLoad);
}

void JitArm::stfXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	ARMReg RA;

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	bool single = false;
	bool update = false;
	bool zeroA = false;
	s32 offsetReg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 663: // stfsx
					single = true;
					zeroA = true;
					offsetReg = b;
				break;
				case 695: // stfsux
					single = true;
					offsetReg = b;
				break;
				case 727: // stfdx
					zeroA = true;
					offsetReg = b;
				break;
				case 759: // stfdux
					update = true;
					offsetReg = b;
				break;
			}
		break;
		case 53: // stfsu
			update = true;
			single = true;
		break;
		case 52: // stfs
			single = true;
			zeroA = true;
		break;
		case 55: // stfdu
			update = true;
		break;
		case 54: // stfd
			zeroA = true;
		break;
	}

	ARMReg v0 = fpr.R0(inst.FS);

	if (update)
	{
		RA = gpr.R(a);
		// Update path /always/ uses RA
		if (offsetReg == -1) // uses SIMM_16
		{
			MOVI2R(rB, offset);
			ADD(rB, rB, RA);
		}
		else
		{
			ADD(rB, gpr.R(offsetReg), RA);
		}
	}
	else
	{
		if (zeroA)
		{
			if (offsetReg == -1)
			{
				if (a)
				{
					RA = gpr.R(a);
					MOVI2R(rB, offset);
					ADD(rB, rB, RA);
				}
				else
				{
					MOVI2R(rB, (u32)offset);
				}
			}
			else
			{
				ARMReg RB = gpr.R(offsetReg);
				if (a)
				{
					RA = gpr.R(a);
					ADD(rB, RB, RA);
				}
				else
				{
					MOV(rB, RB);
				}
			}
		}
	}

	if (update)
	{
		LDR(rA, R9, PPCSTATE_OFF(Exceptions));
		CMP(rA, EXCEPTION_DSI);

		SetCC(CC_NEQ);
		MOV(RA, rB);
		SetCC();
	}

	// This branch gets changed to a NOP when the fastpath fails
	FixupBranch fast_path;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem)
		fast_path = B();

	{
		PUSH(4, R0, R1, R2, R3);
		if (single)
		{
			MOV(R1, rB);
			VCVT(S0, v0, 0);
			VMOV(R0, S0);
			MOVI2R(rA, (u32)&Memory::Write_U32);
			BL(rA);
		}
		else
		{
			MOVI2R(rA, (u32)&Memory::Write_F64);
#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
			VMOV(R0, v0);
			MOV(R2, rB);
#else
			VMOV(D0, v0);
			MOV(R0, rB);
#endif
			BL(rA);
		}
		POP(4, R0, R1, R2, R3);
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem)
	{
		FixupBranch slow_out = B();
		SetJumpTarget(fast_path);
		{
			Operand2 mask(2, 1); // ~(Memory::MEMVIEW32_MASK)
			ARMReg rC = gpr.GetReg();
			BIC(rC, rB, mask);
			MOVI2R(rA, (u32)Memory::base);
			ADD(rC, rC, rA);

			NEONXEmitter nemit(this);
			if (single)
			{
				VCVT(S0, v0, 0);
				nemit.VREV32(I_8, D0, D0);
				VSTR(S0, rC, 0);
			}
			else
			{
				nemit.VREV64(I_8, D0, v0);
				VSTR(D0, rC, 0);
			}
			gpr.Unlock(rC);
		}
		SetJumpTarget(slow_out);
	}

	gpr.Unlock(rA, rB);
}

