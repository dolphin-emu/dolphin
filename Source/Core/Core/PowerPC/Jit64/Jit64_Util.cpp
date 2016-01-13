// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"

using namespace Gen;
using namespace Jit64Reg;

void Jit64::SafeWrite(Any<Jit64Reg::GPR>& reg_value, Any<Jit64Reg::GPR>& reg_addr, Any<Jit64Reg::GPR>& offset, int accessSize, bool swap, bool update)
{
	_assert_msg_(DYNAREC, accessSize == 8 || accessSize == 16 || accessSize == 32, "Invalid write size: %d", accessSize);

	if (swap)
	{
		// No need to swap
		if (accessSize == 8)
		{
			swap = false;
		}

		// We can swap these ahead of time
		if (reg_value.IsImm())
		{
			swap = false;
			switch (accessSize)
			{
				case 16:
					reg_value = regs.gpr.Imm32(Common::swap16(reg_value.Imm32()));
					break;
				case 32:
					reg_value = regs.gpr.Imm32(Common::swap32(reg_value.Imm32()));
					break;
			}
		}
	}

	if (update)
	{
		// No need to update if the offset is zero
		if (offset.IsZero())
		{
			update = false;
		}

		// TODO: if address register is not used, don't both updating
	}

	// Easy case: we know the address we're writing to
	if (reg_addr.IsImm() && offset.IsImm())
	{
		WriteToConstAddress(accessSize, reg_value, reg_addr.Imm32() + offset.Imm32(), CallerSavedRegistersInUse());
		return;
	}

	// TODO: lots of optimizations in here

	// The tough case: we need to borrow a register to do all the summing
	auto scratch = regs.gpr.Borrow();
	MOV_sum(32, scratch, reg_addr, offset);

	if (update)
	{
		auto xa = reg_addr.Bind(Jit64Reg::WriteTransaction);
		MOV(32, xa, scratch);
	}

	SafeWriteRegToReg(reg_value, scratch, accessSize, 0, CallerSavedRegistersInUse());
}

void Jit64::SafeLoad(Native<Jit64Reg::GPR>& reg_value, Any<Jit64Reg::GPR>& reg_addr, Any<Jit64Reg::GPR>& offset, int accessSize, bool signExtend, bool swap, bool update)
{
	if (swap)
	{
		// No need to swap
		if (accessSize == 8)
		{
			swap = false;
		}
	}

	if (swap)
	{
		// TODO: MOVBE
		auto xv = reg_value.Bind(Jit64Reg::Write);
		SafeLoad(reg_value, reg_addr, offset, accessSize, signExtend, false, update);
		BSWAP(32, xv);
		return;
	}

	auto scratch = regs.gpr.Borrow();
	MOV_sum(32, scratch, reg_addr, offset);

	if (update)
	{
		auto xa = reg_addr.Bind(Jit64Reg::WriteTransaction);
		MOV(32, xa, scratch);
	}

	// TODO: lots of optimizations in here

	SafeLoadToReg(reg_value, scratch, accessSize, 0, CallerSavedRegistersInUse(), signExtend);
}
