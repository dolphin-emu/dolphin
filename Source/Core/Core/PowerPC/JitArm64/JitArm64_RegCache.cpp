// copyright 2014 dolphin emulator project
// licensed under gplv2
// refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"

using namespace Arm64Gen;

void Arm64RegCache::Init(ARM64XEmitter* emitter)
{
	m_emit = emitter;
	GetAllocationOrder();
}

ARM64Reg Arm64RegCache::GetReg()
{
	// If we have no registers left, dump the most stale register first
	if (!GetUnlockedRegisterCount())
		FlushMostStaleRegister();

	for (auto& it : m_host_registers)
	{
		if (!it.IsLocked())
		{
			it.Lock();
			return it.GetReg();
		}
	}
	// Holy cow, how did you run out of registers?
	// We can't return anything reasonable in this case. Return INVALID_REG and watch the failure happen
	_assert_msg_(_DYNA_REC_, false, "All available registers are locked dumb dumb");
	return INVALID_REG;
}

u32 Arm64RegCache::GetUnlockedRegisterCount()
{
	u32 unlocked_registers = 0;
	for (auto& it : m_host_registers)
		if (!it.IsLocked())
			++unlocked_registers;
	return unlocked_registers;
}

void Arm64RegCache::LockRegister(ARM64Reg host_reg)
{
	auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
	if (reg == m_host_registers.end())
		_assert_msg_(DYNA_REC, false, "Don't try locking a register that isn't in the cache");
	_assert_msg_(DYNA_REC, !reg->IsLocked(), "This register is already locked");
	reg->Lock();
}

void Arm64RegCache::UnlockRegister(ARM64Reg host_reg)
{
	auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
	if (reg == m_host_registers.end())
		_assert_msg_(DYNA_REC, false, "Don't try unlocking a register that isn't in the cache");
	_assert_msg_(DYNA_REC, reg->IsLocked(), "This register is already unlocked");
	reg->Unlock();
}

// GPR Cache
void Arm64GPRCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	// To make this technique easy, let's just work on pairs of even/odd registers
	// We could do simple odd/even as well to get a few spare temporary registers
	// but it isn't really needed, we aren't starved for registers
	for (int reg = 0; reg < 32; reg += 2)
	{
		u32 regs_used = (stats.IsUsed(reg) << 1) | stats.IsUsed(reg + 1);
		switch (regs_used)
		{
		case 0x02: // Reg+0 used
		{
			ARM64Reg host_reg = GetReg();
			m_guest_registers[reg].LoadToReg(host_reg);
			m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[reg]));
		}
		break;
		case 0x01: // Reg+1 used
		{
			ARM64Reg host_reg = GetReg();
			m_guest_registers[reg + 1].LoadToReg(host_reg);
			m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[reg + 1]));
		}
		break;
		case 0x03: // Both registers used
		{
			// Get a 64bit host register
			ARM64Reg host_reg = EncodeRegTo64(GetReg());
			m_guest_registers[reg].LoadToAway(host_reg, REG_LOW);
			m_guest_registers[reg + 1].LoadToAway(host_reg, REG_HIGH);

			// host_reg is 64bit here.
			// It'll load both guest_registers in one LDR
			m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[reg]));
		}
		break;
		case 0x00: // Neither used
		default:
		break;
		}
	}
}

bool Arm64GPRCache::IsCalleeSaved(ARM64Reg reg)
{
	static std::vector<ARM64Reg> callee_regs =
	{
		X28, X27, X26, X25, X24, X23, X22, X21, X20,
		X19, INVALID_REG,
	};
	return std::find(callee_regs.begin(), callee_regs.end(), EncodeRegTo64(reg)) != callee_regs.end();
}

void Arm64GPRCache::FlushRegister(u32 preg)
{
	u32 base_reg = preg;
	OpArg& reg = m_guest_registers[preg];
	if (reg.GetType() == REG_REG)
	{
		ARM64Reg host_reg = reg.GetReg();

		m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));
		Unlock(host_reg);

		reg.Flush();
	}
	else if (reg.GetType() == REG_IMM)
	{
		ARM64Reg host_reg = GetReg();

		m_emit->MOVI2R(host_reg, reg.GetImm());
		m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));

		Unlock(host_reg);

		reg.Flush();
	}
	else if (reg.GetType() == REG_AWAY)
	{
		u32 next_reg = 0;
		if (reg.GetAwayLocation() == REG_LOW)
			next_reg = base_reg + 1;
		else
			next_reg = base_reg - 1;
		OpArg& reg2 = m_guest_registers[next_reg];
		ARM64Reg host_reg = reg.GetAwayReg();
		ARM64Reg host_reg_1 = reg.GetReg();
		ARM64Reg host_reg_2 = reg2.GetReg();
		// Flush if either of these shared registers are used.
		if (host_reg_1 == INVALID_REG)
		{
			// We never loaded this register
			// We've got to test the state of our shared register
			// Currently it is always reg+1
			if (host_reg_2 == INVALID_REG)
			{
				// We didn't load either of these registers
				// This can happen in cases where we had to flush register state
				// or if we hit an interpreted instruction before we could use it
				// Dump the whole thing in one go and flush both registers

				// 64bit host register will store 2 32bit store registers in one go
				if (reg.GetAwayLocation() == REG_LOW)
					m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[base_reg]));
				else
					m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[next_reg]));
			}
			else
			{
				// Alright, bottom register isn't used, but top one is
				// Only store the top one
				m_emit->STR(INDEX_UNSIGNED, host_reg_2, X29, PPCSTATE_OFF(gpr[next_reg]));
				Unlock(host_reg_2);
			}
		}
		else
		{
			m_emit->STR(INDEX_UNSIGNED, host_reg_1, X29, PPCSTATE_OFF(gpr[base_reg]));
			Unlock(host_reg_1);
		}
		// Flush both registers
		reg.Flush();
		reg2.Flush();
		Unlock(DecodeReg(host_reg));
	}

}

void Arm64GPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
	for (int i = 0; i < 32; ++i)
	{
		bool flush = true;
		if (mode == FLUSH_INTERPRETER)
		{
			if (!(op->regsOut[0] == i ||
			    op->regsOut[1] == i ||
			    op->regsIn[0] == i ||
			    op->regsIn[1] == i ||
			    op->regsIn[2] == i))
			{
				// This interpreted instruction doesn't use this register
				flush = false;
			}
		}

		if (m_guest_registers[i].GetType() == REG_REG)
		{
			// Has to be flushed if it isn't in a callee saved register
			ARM64Reg host_reg = m_guest_registers[i].GetReg();
			if (flush || !IsCalleeSaved(host_reg))
				FlushRegister(i);
		}
		else if (m_guest_registers[i].GetType() == REG_IMM)
		{
			if (flush)
				FlushRegister(i);
		}
		else if (m_guest_registers[i].GetType() == REG_AWAY)
		{
			// We are away, that means that this register and the next are stored in a single 64bit register
			// There is a very good chance that both the registers are out in some "temp" register
			bool flush_2 = true;
			if (mode == FLUSH_INTERPRETER)
			{
				if (!(op->regsOut[0] == (i + 1) ||
				    op->regsOut[1] == (i + 1) ||
				    op->regsIn[0] == (i + 1) ||
				    op->regsIn[1] == (i + 1) ||
				    op->regsIn[2] == (i + 1)))
				{
					// This interpreted instruction doesn't use this register
					flush_2 = false;
				}
			}

			ARM64Reg host_reg = m_guest_registers[i].GetAwayReg();
			ARM64Reg host_reg_1 = m_guest_registers[i].GetReg();
			ARM64Reg host_reg_2 = m_guest_registers[i + 1].GetReg();
			// Flush if either of these shared registers are used.
			if (flush ||
			    flush_2 ||
			    !IsCalleeSaved(host_reg) ||
			    !IsCalleeSaved(host_reg_1) ||
			    !IsCalleeSaved(host_reg_2))
			{
				FlushRegister(i); // Will flush both pairs of registers
			}
			// Skip the next register since we've handled it here
			++i;
		}
	}
}

ARM64Reg Arm64GPRCache::R(u32 preg)
{
	OpArg& reg = m_guest_registers[preg];
	IncrementAllUsed();
	reg.ResetLastUsed();

	switch (reg.GetType())
	{
	case REG_REG: // already in a reg
		return reg.GetReg();
	break;
	case REG_IMM: // Is an immediate
	{
		ARM64Reg host_reg = GetReg();
		m_emit->MOVI2R(host_reg, reg.GetImm());
		reg.LoadToReg(host_reg);
		return host_reg;
	}
	break;
	case REG_AWAY: // Register is away in a shared register
	{
		// Let's do the voodoo that we dodo
		if (reg.GetReg() == INVALID_REG)
		{
			// Alright, we need to extract from our away register
			// To our new 32bit register
			if (reg.GetAwayLocation() == REG_LOW)
			{
				OpArg& upper_reg = m_guest_registers[preg + 1];
				if (upper_reg.GetType() == REG_REG)
				{
					// If the upper reg is already moved away, just claim this one as ours now
					ARM64Reg host_reg = reg.GetAwayReg();
					reg.LoadToReg(DecodeReg(host_reg));
					return host_reg;
				}
				else
				{
					// Top register is still loaded
					// Make sure to move to a new register
					ARM64Reg host_reg = GetReg();
					ARM64Reg current_reg = reg.GetAwayReg();
					reg.LoadToReg(host_reg);

					// We are in the low bits
					// Just move it over to the low bits of the new register
					m_emit->UBFM(EncodeRegTo64(host_reg), current_reg, 0, 31);
					return host_reg;
				}
			}
			else
			{
				OpArg& lower_reg = m_guest_registers[preg - 1];
				if (lower_reg.GetType() == REG_REG)
				{
					// If the lower register is moved away, claim this one as ours
					ARM64Reg host_reg = reg.GetAwayReg();
					reg.LoadToReg(DecodeReg(host_reg));

					// Make sure to move our register from the high bits to the low bits
					m_emit->UBFM(EncodeRegTo64(host_reg), host_reg, 32, 63);
					return host_reg;
				}
				else
				{
					// Load this register in to the new low bits
					// We are no longer away
					ARM64Reg host_reg = GetReg();
					ARM64Reg current_reg = reg.GetAwayReg();
					reg.LoadToReg(host_reg);

					// We are in the high bits
					m_emit->UBFM(EncodeRegTo64(host_reg), current_reg, 32, 63);
					return host_reg;
				}
			}
		}
		else
		{
			// We've already moved to a valid place to work on
			return reg.GetReg();
		}
	}
	break;
	case REG_NOTLOADED: // Register isn't loaded at /all/
	{
		// This is a bit annoying. We try to keep these preloaded as much as possible
		// This can also happen on cases where PPCAnalyst isn't feeing us proper register usage statistics
		ARM64Reg host_reg = GetReg();
		reg.LoadToReg(host_reg);
		m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));
		return host_reg;
	}
	break;
	default:
		_dbg_assert_msg_(DYNA_REC, false, "Invalid OpArg Type!");
	break;
	}
	// We've got an issue if we end up here
	return INVALID_REG;
}

void Arm64GPRCache::GetAllocationOrder()
{
	// Callee saved registers first in hopes that we will keep everything stored there first
	const std::vector<ARM64Reg> allocation_order =
	{
		W28, W27, W26, W25, W24, W23, W22, W21, W20,
		W19, W0, W1, W2, W3, W4, W5, W6, W7, W8, W9,
		W10, W11, W12, W13, W14, W15, W16, W17, W18,
		W30,
	};

	for (ARM64Reg reg : allocation_order)
		m_host_registers.push_back(HostReg(reg));
}

void Arm64GPRCache::FlushMostStaleRegister()
{
	u32 most_stale_preg = 0;
	u32 most_stale_amount = 0;
	for (u32 i = 0; i < 32; ++i)
	{
		u32 last_used = m_guest_registers[i].GetLastUsed();
		if (last_used > most_stale_amount &&
		    m_guest_registers[i].GetType() != REG_IMM &&
		    m_guest_registers[i].GetType() != REG_NOTLOADED)
		{
			most_stale_preg = i;
			most_stale_amount = last_used;
		}
	}
	FlushRegister(most_stale_preg);
}

// FPR Cache
void Arm64FPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
	// XXX: Flush our stuff
}

ARM64Reg Arm64FPRCache::R(u32 preg)
{
	// XXX: return a host reg holding a guest register
}

void Arm64FPRCache::GetAllocationOrder()
{
	const std::vector<ARM64Reg> allocation_order =
	{
		D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10,
		D11, D12, D13, D14, D15, D16, D17, D18, D19,
		D20, D21, D22, D23, D24, D25, D26, D27, D28,
		D29, D30, D31,
	};

	for (ARM64Reg reg : allocation_order)
		m_host_registers.push_back(HostReg(reg));
}

void Arm64FPRCache::FlushMostStaleRegister()
{
	// XXX: Flush a register
}

