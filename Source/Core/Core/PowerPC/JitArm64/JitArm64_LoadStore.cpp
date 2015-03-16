// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/Jit_Util.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::icbi(UGeckoInstruction inst)
{
	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	FallBackToInterpreter(inst);
	WriteExit(js.compilerPC + 4);
}

void JitArm64::SafeLoadToReg(u32 dest, s32 addr, s32 offsetReg, u32 flags, s32 offset, bool update)
{
	// We want to make sure to not get LR as a temp register
	gpr.Lock(W0, W30);

	gpr.BindToRegister(dest, dest == (u32)addr || dest == (u32)offsetReg);
	ARM64Reg dest_reg = gpr.R(dest);
	ARM64Reg up_reg = INVALID_REG;
	ARM64Reg off_reg = INVALID_REG;

	if (addr != -1 && !gpr.IsImm(addr))
		up_reg = gpr.R(addr);

	if (offsetReg != -1 && !gpr.IsImm(offsetReg))
		off_reg = gpr.R(offsetReg);

	BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
	BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
	regs_in_use[W0] = 0;
	regs_in_use[W30] = 0;
	regs_in_use[dest_reg] = 0;

	ARM64Reg addr_reg = W0;
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
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, up_reg, offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, up_reg, std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, up_reg);
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
				u32 reg_offset = gpr.GetImm(addr);
				if (reg_offset < 4096)
				{
					ADD(addr_reg, off_reg, reg_offset);
				}
				else
				{
					MOVI2R(addr_reg, gpr.GetImm(addr));
					ADD(addr_reg, addr_reg, off_reg);
				}
			}
			else if (!gpr.IsImm(addr) && gpr.IsImm(offsetReg))
			{
				u32 reg_offset = gpr.GetImm(offsetReg);
				if (reg_offset < 4096)
				{
					ADD(addr_reg, up_reg, reg_offset);
				}
				else
				{
					MOVI2R(addr_reg, gpr.GetImm(offsetReg));
					ADD(addr_reg, addr_reg, up_reg);
				}
			}
			else
			{
				ADD(addr_reg, up_reg, off_reg);
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
				MOV(addr_reg, off_reg);
			}
		}
	}

	ARM64Reg XA = EncodeRegTo64(addr_reg);

	if (is_immediate)
		MOVI2R(XA, imm_addr);

	if (update)
	{
		gpr.BindToRegister(addr, false);
		MOV(gpr.R(addr), addr_reg);
	}

	u32 access_size = BackPatchInfo::GetFlagSize(flags);
	u32 mmio_address = 0;
	if (is_immediate)
		mmio_address = PowerPC::IsOptimizableMMIOAccess(imm_addr, access_size);

	if (is_immediate && PowerPC::IsOptimizableRAMAddress(imm_addr))
	{
		EmitBackpatchRoutine(this, flags, true, false, dest_reg, XA);
	}
	else if (mmio_address)
	{
		MMIOLoadToReg(Memory::mmio_mapping, this,
		              regs_in_use, fprs_in_use, dest_reg,
		              mmio_address, flags);
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
			dest_reg, XA);
		m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
		ABI_PopRegisters(regs_in_use);
	}

	gpr.Unlock(W0, W30);
}

void JitArm64::SafeStoreFromReg(s32 dest, u32 value, s32 regOffset, u32 flags, s32 offset)
{
	// We want to make sure to not get LR as a temp register
	gpr.Lock(W0, W1, W30);

	ARM64Reg RS = gpr.R(value);

	ARM64Reg reg_dest = INVALID_REG;
	ARM64Reg reg_off = INVALID_REG;

	if (regOffset != -1 && !gpr.IsImm(regOffset))
		reg_off = gpr.R(regOffset);
	if (dest != -1 && !gpr.IsImm(dest))
		reg_dest = gpr.R(dest);

	BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
	BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
	regs_in_use[W0] = 0;
	regs_in_use[W1] = 0;
	regs_in_use[W30] = 0;

	ARM64Reg addr_reg = W1;

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
				if (offset >= 0 && offset < 4096)
				{
					ADD(addr_reg, reg_dest, offset);
				}
				else if (offset < 0 && offset > -4096)
				{
					SUB(addr_reg, reg_dest, std::abs(offset));
				}
				else
				{
					MOVI2R(addr_reg, offset);
					ADD(addr_reg, addr_reg, reg_dest);
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
				u32 reg_offset = gpr.GetImm(dest);
				if (reg_offset < 4096)
				{
					ADD(addr_reg, reg_off, reg_offset);
				}
				else
				{
					MOVI2R(addr_reg, reg_offset);
					ADD(addr_reg, addr_reg, reg_off);
				}
			}
			else if (!gpr.IsImm(dest) && gpr.IsImm(regOffset))
			{
				u32 reg_offset = gpr.GetImm(regOffset);
				if (reg_offset < 4096)
				{
					ADD(addr_reg, reg_dest, reg_offset);
				}
				else
				{
					MOVI2R(addr_reg, gpr.GetImm(regOffset));
					ADD(addr_reg, addr_reg, reg_dest);
				}
			}
			else
			{
				ADD(addr_reg, reg_dest, reg_off);
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
				MOV(addr_reg, reg_off);
			}
		}
	}

	ARM64Reg XA = EncodeRegTo64(addr_reg);

	u32 access_size = BackPatchInfo::GetFlagSize(flags);
	u32 mmio_address = 0;
	if (is_immediate)
		mmio_address = PowerPC::IsOptimizableMMIOAccess(imm_addr, access_size);

	if (is_immediate && PowerPC::IsOptimizableRAMAddress(imm_addr))
	{
		MOVI2R(XA, imm_addr);

		EmitBackpatchRoutine(this, flags, true, false, RS, XA);
	}
	else if (mmio_address && !(flags & BackPatchInfo::FLAG_REVERSE))
	{
		MMIOWriteRegToAddr(Memory::mmio_mapping, this,
		                   regs_in_use, fprs_in_use, RS,
		                   mmio_address, flags);
	}
	else
	{
		if (is_immediate)
			MOVI2R(XA, imm_addr);

		// Has a chance of being backpatched which will destroy our state
		// push and pop everything in this instance
		ABI_PushRegisters(regs_in_use);
		m_float_emit.ABI_PushRegisters(fprs_in_use, X30);
		EmitBackpatchRoutine(this, flags,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem,
			RS, XA);
		m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
		ABI_PopRegisters(regs_in_use);
	}

	gpr.Unlock(W0, W1, W30);
}

void JitArm64::lXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	s32 offset = inst.SIMM_16;
	s32 offsetReg = -1;
	u32 flags = BackPatchInfo::FLAG_LOAD;
	bool update = false;

	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 55: // lwzux
					update = true;
				case 23: // lwzx
					flags |= BackPatchInfo::FLAG_SIZE_32;
					offsetReg = b;
				break;
				case 119: //lbzux
					update = true;
				case 87: // lbzx
					flags |= BackPatchInfo::FLAG_SIZE_8;
					offsetReg = b;
				break;
				case 311: // lhzux
					update = true;
				case 279: // lhzx
					flags |= BackPatchInfo::FLAG_SIZE_16;
					offsetReg = b;
				break;
				case 375: // lhaux
					update = true;
				case 343: // lhax
					flags |= BackPatchInfo::FLAG_EXTEND |
					         BackPatchInfo::FLAG_SIZE_16;
					offsetReg = b;
				break;
				case 534: // lwbrx
					flags |= BackPatchInfo::FLAG_REVERSE |
					         BackPatchInfo::FLAG_SIZE_32;
				break;
				case 790: // lhbrx
					flags |= BackPatchInfo::FLAG_REVERSE |
					         BackPatchInfo::FLAG_SIZE_16;
				break;
			}
		break;
		case 33: // lwzu
			update = true;
		case 32: // lwz
			flags |= BackPatchInfo::FLAG_SIZE_32;
		break;
		case 35: // lbzu
			update = true;
		case 34: // lbz
			flags |= BackPatchInfo::FLAG_SIZE_8;
		break;
		case 41: // lhzu
			update = true;
		case 40: // lhz
			flags |= BackPatchInfo::FLAG_SIZE_16;
		break;
		case 43: // lhau
			update = true;
		case 42: // lha
			flags |= BackPatchInfo::FLAG_EXTEND |
			        BackPatchInfo::FLAG_SIZE_16;
		break;
	}

	SafeLoadToReg(d, update ? a : (a ? a : -1), offsetReg, flags, offset, update);

	// LWZ idle skipping
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
	    inst.OPCD == 32 &&
	    (inst.hex & 0xFFFF0000) == 0x800D0000 &&
	    (PowerPC::HostRead_U32(js.compilerPC + 4) == 0x28000000 ||
	    (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && PowerPC::HostRead_U32(js.compilerPC + 4) == 0x2C000000)) &&
	    PowerPC::HostRead_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		// if it's still 0, we can wait until the next event
		FixupBranch noIdle = CBNZ(gpr.R(d));

		gpr.Flush(FLUSH_MAINTAIN_STATE);
		fpr.Flush(FLUSH_MAINTAIN_STATE);

		ARM64Reg WA = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);

		MOVI2R(XA, (u64)&PowerPC::OnIdle);
		BLR(XA);

		gpr.Unlock(WA);
		WriteExceptionExit();

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}
}

void JitArm64::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	u32 a = inst.RA, b = inst.RB, s = inst.RS;
	s32 offset = inst.SIMM_16;
	s32 regOffset = -1;
	u32 flags = BackPatchInfo::FLAG_STORE;
	bool update = false;
	switch (inst.OPCD)
	{
		case 31:
			switch (inst.SUBOP10)
			{
				case 183: // stwux
					update = true;
				case 151: // stwx
					flags |= BackPatchInfo::FLAG_SIZE_32;
					regOffset = b;
				break;
				case 247: // stbux
					update = true;
				case 215: // stbx
					flags |= BackPatchInfo::FLAG_SIZE_8;
					regOffset = b;
				break;
				case 439: // sthux
					update = true;
				case 407: // sthx
					flags |= BackPatchInfo::FLAG_SIZE_16;
					regOffset = b;
				break;
			}
		break;
		case 37: // stwu
			update = true;
		case 36: // stw
			flags |= BackPatchInfo::FLAG_SIZE_32;
		break;
		case 39: // stbu
			update = true;
		case 38: // stb
			flags |= BackPatchInfo::FLAG_SIZE_8;
		break;
		case 45: // sthu
			update = true;
		case 44: // sth
			flags |= BackPatchInfo::FLAG_SIZE_16;
		break;

	}

	SafeStoreFromReg(update ? a : (a ? a : -1), s, regOffset, flags, offset);

	if (update)
	{
		gpr.BindToRegister(a, false);

		ARM64Reg WA = gpr.GetReg();
		ARM64Reg RB;
		ARM64Reg RA = gpr.R(a);
		if (regOffset != -1)
			RB = gpr.R(regOffset);
		if (regOffset == -1)
		{
			MOVI2R(WA, offset);
			ADD(RA, RA, WA);
		}
		else
		{
			ADD(RA, RA, RB);
		}
		gpr.Unlock(WA);
	}
}
