// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"

using namespace Gen;

void Jit64::SafeWrite(GPRRegister& reg_value, GPRRegister& reg_addr, GPRRegister& offset, int accessSize, bool swap, bool update)
{
	_assert_msg_(DYNAREC, accessSize == 8 || accessSize == 16 || accessSize == 32 || accessSize == 64, "Invalid write size: %d", accessSize);

	if (update && !reg_addr.HasPPCRegister())
	{
		// This form of instruction is illegal
		WriteInvalidInstruction();
		return;
	}

	bool valueAddressShareRegister = reg_value.IsAliasOf(reg_addr);
	bool valueOffsetShareRegister = reg_value.IsAliasOf(offset);

	// We want to preserve any non-scratch registers passed into this
	// function. If a scratch register is passed, we assume it may be safely
	// clobbered.
	if (WriteClobbersRegValue(accessSize, swap) && reg_value.HasPPCRegister())
	{
		// TODO: test for free register, otherwise swap back
		auto scratch = regs.gpr.Borrow();
		MOV(32, scratch, reg_value);
		SafeWrite(scratch, reg_addr, offset, accessSize, swap, update);
		return;
	}

	// Note that while SafeWriteRegToReg takes a register and an OpArg, it really wants things to be
	// registers or immediates.
	auto val = (reg_value.IsImm() || reg_value.IsRegBound()) ? reg_value : reg_value.Bind(BindMode::Read);

	// Optimize swap flag
	if (swap)
	{
		// No need to swap
		if (accessSize == 8)
		{
			swap = false;
		}
		else
		{
			// We can swap these ahead of time
			if (val.IsImm())
			{
				swap = false;
				switch (accessSize)
				{
					case 16:
						val = regs.gpr.Imm32(Common::swap16(val.Imm32()));
						break;
					case 32:
						val = regs.gpr.Imm32(Common::swap32(val.Imm32()));
						break;
					case 64:
						// We don't support 64-bit immediates (TODO?)
						UnexpectedInstructionForm();
						break;
				}
			}
		}
	}

	u32 flags = swap ? 0 : SAFE_LOADSTORE_NO_SWAP;

	// Constant write (only available if swap is true)
	if (reg_addr.IsImm() && offset.IsImm())
	{
		u32 const_addr = reg_addr.Imm32() + offset.Imm32();
		if (swap)
		{
			bool exception = WriteToConstAddress(accessSize, val, const_addr, CallerSavedRegistersInUse());
			if (update)
			{
				if (jo.memcheck && exception)
				{
					// No need to be transactional here
					MemoryExceptionCheck();
					auto xa = reg_addr.Bind(BindMode::ReadWrite);
					ADD(32, xa, offset);
				}
				else
				{
					reg_addr.SetImm32(const_addr);
				}
			}
			return;
		}
	}

	// We need to bind a register, so may as well just add the offset here
	if (reg_addr.IsImm())
	{
		if (update)
		{
			// Update reg_addr inside a transaction
			auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
			MOV_sum(32, xa, xa, offset);
			SafeWriteRegToReg(val, xa, accessSize, 0, CallerSavedRegistersInUse(), flags);
		}
		else
		{
			auto scratch = regs.gpr.Borrow();
			MOV_sum(32, scratch, reg_addr, offset);
			SafeWriteRegToReg(val, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
		}
		return;
	}

	auto addr = reg_addr.Bind(BindMode::Read);

	// Easy: zero offset, so updates don't matter
	if (offset.IsZero())
	{
		SafeWriteRegToReg(valueAddressShareRegister ? addr : val, addr, accessSize, 0, CallerSavedRegistersInUse(), flags);
		return;
	}

	// If we're updating we can clobber address (transactionally)
	if (update)
	{
		if (valueAddressShareRegister)
		{
			auto scratch = regs.gpr.Borrow();
			auto xv = reg_value.Bind(BindMode::ReadWrite);
			MOV_sum(32, scratch, xv, offset);
			SafeWriteRegToReg((X64Reg)xv, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
			MemoryExceptionCheck();
			auto xa = addr.Bind(BindMode::Write);
			MOV(32, xa, scratch);
		}
		else
		{
			// Update reg_addr inside a transaction
			auto xa = addr.Bind(BindMode::ReadWriteTransactional);
			MOV_sum(32, xa, xa, offset);
			SafeWriteRegToReg(val, xa, accessSize, 0, CallerSavedRegistersInUse(), flags);
		}
		return;
	}

	if (valueAddressShareRegister)
	{
		auto xv = reg_value.Bind(BindMode::ReadWrite);
		auto scratch = regs.gpr.Borrow();
		MOV_sum(32, scratch, xv, offset);
		SafeWriteRegToReg((X64Reg)xv, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
		return;
	}

	if (valueOffsetShareRegister)
	{
		auto xv = reg_value.Bind(BindMode::ReadWrite);
		auto scratch = regs.gpr.Borrow();
		MOV_sum(32, scratch, addr, xv);
		SafeWriteRegToReg((X64Reg)xv, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
		return;
	}

	// No update so we need to sum things in a scratch register. We can't just pass the offset into 
	// SafeWriteRegToReg because that will clobber the address register we pass in
	auto scratch = regs.gpr.Borrow();
	MOV_sum(32, scratch, addr, offset);
	SafeWriteRegToReg(val, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
}

void Jit64::SafeLoad(GPRNative& reg_value, GPRRegister& reg_addr, GPRRegister& offset, int accessSize, bool signExtend, bool swap, bool update)
{
	bool valueAddressShareRegister = reg_value.IsAliasOf(reg_addr);
	bool valueOffsetShareRegister = reg_value.IsAliasOf(offset);

	if (update && !reg_addr.HasPPCRegister())
	{
		// This form of instruction is illegal
		WriteInvalidInstruction();
		return;
	}

	if (!swap && accessSize == 8)
	{
		// Swapping automatically is currently cheaper
		swap = true;
	}

	if (!swap)
	{
		// TODO: add a flag to disable swap in SafeLoadToReg
		SafeLoad(reg_value, reg_addr, offset, accessSize, signExtend, true, update);
		BSWAP(accessSize, reg_value);
		return;
	}

	// Easy: zero offset, so updates don't matter
	if (offset.IsZero())
	{
		if (valueAddressShareRegister)
		{
			auto xv = reg_value.Bind(BindMode::ReadWrite);
			SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		}
		else
		{
			auto addr = (reg_addr.IsImm() || reg_addr.IsRegBound()) ? reg_addr : reg_addr.Bind(BindMode::Read);
			SafeLoadToReg(reg_value, addr, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		}
		return;
	}

	// If we're updating we can clobber address (transactionally)
	if (update)
	{
		if (valueAddressShareRegister)
		{
			// If update is set, the value and address cannot share a register
			WriteInvalidInstruction();
			return;
		}
		else if (valueOffsetShareRegister)
		{
			auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
			auto xo = offset.Bind(BindMode::ReadWrite);
			MOV_sum(32, xa, xa, xo);
			SafeLoadToReg(xo, xa, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		}
		else
		{
			auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
			MOV_sum(32, xa, xa, offset);
			SafeLoadToReg(reg_value, xa, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		}
		return;
	}

	// If we know the address we can pass it as an immediate
	if (reg_addr.IsImm() && offset.IsImm())
	{
		auto addr = Gen::Imm32(reg_addr.Imm32() + offset.Imm32());
		SafeLoadToReg(reg_value, addr, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		return;
	}

	// We know that address is getting clobbered
	if (valueAddressShareRegister)
	{
		auto xv = reg_value.Bind(BindMode::ReadWrite);
		MOV_sum(32, xv, xv, offset);
		SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		return;
	}

	// We know that offset is getting clobbered
	if (valueOffsetShareRegister)
	{
		auto xv = reg_value.Bind(BindMode::ReadWrite);
		MOV_sum(32, xv, reg_addr, xv);
		SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
		return;
	}

	if (offset.IsImm())
	{
		// Passing an offset requires us to provide a scratch register
		auto offsetScratch = regs.gpr.Borrow();
		auto addr = reg_addr.IsImm() || reg_addr.IsRegBound() ? reg_addr : reg_addr.Bind(BindMode::Read);
		SafeLoadToReg(reg_value, addr, accessSize, offset.Imm32(), CallerSavedRegistersInUse(), signExtend, 0, offsetScratch);
		return;
	}

	// Non-immediate offset so we need to sum things in a scratch register
	// SafeLoadToReg can clobber RSCRATCH as ABI return
	auto scratch = regs.gpr.BorrowAnyBut(BitSet32{ RSCRATCH });
	MOV_sum(32, scratch, reg_addr, offset);
	SafeLoadToReg(reg_value, scratch, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
}
