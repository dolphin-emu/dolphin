// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::lfXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	u32 flags = BackPatchInfo::FLAG_LOAD;
	bool update = false;
	s32 offset_reg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 567: // lfsux
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					update = true;
					offset_reg = b;
				break;
				case 535: // lfsx
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offset_reg = b;
				break;
				case 631: // lfdux
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					update = true;
					offset_reg = b;
				break;
				case 599: // lfdx
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					offset_reg = b;
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

	u32 imm_addr = 0;
	bool is_immediate = false;

	fpr.BindToRegister(inst.FD, false);
	ARM64Reg VD = fpr.R(inst.FD);
	ARM64Reg addr_reg = W0;

	gpr.Lock(W0, W30);
	fpr.Lock(Q0);

	if (update)
	{
		// Always uses RA
		if (gpr.IsImm(a) && offset_reg == -1)
		{
			is_immediate = true;
			imm_addr = offset + gpr.GetImm(a);
		}
		else if (gpr.IsImm(a) && offset_reg != -1 && gpr.IsImm(offset_reg))
		{
			is_immediate = true;
			imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
		}
		else
		{
			if (offset_reg == -1)
			{
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, gpr.R(a), offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, gpr.R(a), std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, gpr.R(a));
				}			}
			else
			{
				ADD(addr_reg, gpr.R(offset_reg), gpr.R(a));
			}
		}
	}
	else
	{
		if (offset_reg == -1)
		{
			if (a && gpr.IsImm(a))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + offset;
			}
			else if (a)
			{
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, gpr.R(a), offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, gpr.R(a), std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, gpr.R(a));
				}			}
			else
			{
				is_immediate = true;
				imm_addr = offset;
			}
		}
		else
		{
			if (a && gpr.IsImm(a) && gpr.IsImm(offset_reg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
			}
			else if (!a && gpr.IsImm(offset_reg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(offset_reg);
			}
			else if (a)
			{
				ADD(addr_reg, gpr.R(a), gpr.R(offset_reg));
			}
			else
			{
				MOV(addr_reg, gpr.R(offset_reg));
			}
		}
	}

	ARM64Reg XA = EncodeRegTo64(addr_reg);

	if (is_immediate)
		MOVI2R(XA, imm_addr);

	if (update)
	{
		gpr.BindToRegister(a, false);
		MOV(gpr.R(a), addr_reg);
	}

	BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
	BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
	regs_in_use[W0] = 0;
	regs_in_use[W30] = 0;
	fprs_in_use[0] = 0; // Q0
	fprs_in_use[VD - Q0] = 0;

	if (is_immediate && PowerPC::IsOptimizableRAMAddress(imm_addr))
	{
		EmitBackpatchRoutine(this, flags, true, false, VD, XA);
	}
	else
	{
		// Has a chance of being backpatched which will destroy our state
		// push and pop everything in this instance
		ABI_PushRegisters(regs_in_use);
		m_float_emit.ABI_PushRegisters(fprs_in_use, X30);
		EmitBackpatchRoutine(this, flags,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			VD, XA);
		m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
		ABI_PopRegisters(regs_in_use);
	}

	gpr.Unlock(W0, W30);
	fpr.Unlock(Q0);
}

void JitArm64::stfXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	u32 a = inst.RA, b = inst.RB;

	s32 offset = inst.SIMM_16;
	u32 flags = BackPatchInfo::FLAG_STORE;
	bool update = false;
	s32 offset_reg = -1;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 663: // stfsx
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offset_reg = b;
				break;
				case 695: // stfsux
					flags |= BackPatchInfo::FLAG_SIZE_F32;
					offset_reg = b;
				break;
				case 727: // stfdx
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					offset_reg = b;
				break;
				case 759: // stfdux
					flags |= BackPatchInfo::FLAG_SIZE_F64;
					update = true;
					offset_reg = b;
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

	u32 imm_addr = 0;
	bool is_immediate = false;

	ARM64Reg V0 = fpr.R(inst.FS);
	ARM64Reg addr_reg = W1;

	gpr.Lock(W0, W1, W30);
	fpr.Lock(Q0);

	if (update)
	{
		// Always uses RA
		if (gpr.IsImm(a) && offset_reg == -1)
		{
			is_immediate = true;
			imm_addr = offset + gpr.GetImm(a);
		}
		else if (gpr.IsImm(a) && offset_reg != -1 && gpr.IsImm(offset_reg))
		{
			is_immediate = true;
			imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
		}
		else
		{
			if (offset_reg == -1)
			{
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, gpr.R(a), offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, gpr.R(a), std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, gpr.R(a));
				}
			}
			else
			{
				ADD(addr_reg, gpr.R(offset_reg), gpr.R(a));
			}
		}
	}
	else
	{
		if (offset_reg == -1)
		{
			if (a && gpr.IsImm(a))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + offset;
			}
			else if (a)
			{
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, gpr.R(a), offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, gpr.R(a), std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, gpr.R(a));
				}			}
			else
			{
				is_immediate = true;
				imm_addr = offset;
			}
		}
		else
		{
			if (a && gpr.IsImm(a) && gpr.IsImm(offset_reg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
			}
			else if (!a && gpr.IsImm(offset_reg))
			{
				is_immediate = true;
				imm_addr = gpr.GetImm(offset_reg);
			}
			else if (a)
			{
				ADD(addr_reg, gpr.R(a), gpr.R(offset_reg));
			}
			else
			{
				MOV(addr_reg, gpr.R(offset_reg));
			}
		}
	}

	ARM64Reg XA = EncodeRegTo64(addr_reg);

	if (is_immediate)
		MOVI2R(XA, imm_addr);

	if (update)
	{
		gpr.BindToRegister(a, false);
		MOV(gpr.R(a), addr_reg);
	}

	BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
	BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
	regs_in_use[W0] = 0;
	regs_in_use[W1] = 0;
	regs_in_use[W30] = 0;
	fprs_in_use[0] = 0; // Q0

	if (is_immediate)
	{
		if (jit->jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(imm_addr))
		{
			int accessSize;
			if (flags & BackPatchInfo::FLAG_SIZE_F64)
				accessSize = 64;
			else
				accessSize = 32;

			MOVI2R(X30, (u64)&GPFifo::m_gatherPipeCount);
			MOVI2R(X1, (u64)GPFifo::m_gatherPipe);
			LDR(INDEX_UNSIGNED, W0, X30, 0);
			ADD(X1, X1, X0);
			if (accessSize == 64)
			{
				m_float_emit.REV64(8, Q0, V0);
				m_float_emit.STR(64, INDEX_UNSIGNED, Q0, X1, 0);
			}
			else if (accessSize == 32)
			{
				m_float_emit.FCVT(32, 64, D0, EncodeRegToDouble(V0));
				m_float_emit.REV32(8, D0, D0);
				m_float_emit.STR(32, INDEX_UNSIGNED, D0, X1, 0);
			}
			ADD(W0, W0, accessSize >> 3);
			STR(INDEX_UNSIGNED, W0, X30, 0);
			jit->js.fifoBytesThisBlock += accessSize >> 3;

		}
		else if (PowerPC::IsOptimizableRAMAddress(imm_addr))
		{
			EmitBackpatchRoutine(this, flags, true, false, V0, XA);
		}
		else
		{
			ABI_PushRegisters(regs_in_use);
			m_float_emit.ABI_PushRegisters(fprs_in_use, X30);
			EmitBackpatchRoutine(this, flags, false, false, V0, XA);
			m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
			ABI_PopRegisters(regs_in_use);
		}
	}
	else
	{
		// Has a chance of being backpatched which will destroy our state
		// push and pop everything in this instance
		ABI_PushRegisters(regs_in_use);
		m_float_emit.ABI_PushRegisters(fprs_in_use, X30);
		EmitBackpatchRoutine(this, flags,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			V0, XA);
		m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
		ABI_PopRegisters(regs_in_use);
	}
	gpr.Unlock(W0, W1, W30);
	fpr.Unlock(Q0);
}
