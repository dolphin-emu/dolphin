// copyright 2014 dolphin emulator project
// licensed under gplv2
// refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"

using namespace Arm64Gen;

void Arm64RegCache::Init(ARM64XEmitter *emitter)
{
	m_emit = emitter;
	GetAllocationOrder();
}

ARM64Reg Arm64RegCache::GetReg(void)
{
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
			{
				m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[i]));
				Unlock(host_reg);

				m_guest_registers[i].Flush();
			}
		}
		else if (m_guest_registers[i].GetType() == REG_IMM)
		{
			if (flush)
			{
				ARM64Reg host_reg = GetReg();

				m_emit->MOVI2R(host_reg, m_guest_registers[i].GetImm());
				m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[i]));

				Unlock(host_reg);

				m_guest_registers[i].Flush();
			}
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
						m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[i]));
					}
					else
					{
						// Alright, bottom register isn't used, but top one is
						// Only store the top one
						m_emit->STR(INDEX_UNSIGNED, host_reg_2, X29, PPCSTATE_OFF(gpr[i + 1]));
						Unlock(host_reg_2);
					}
				}
				else
				{
					m_emit->STR(INDEX_UNSIGNED, host_reg_1, X29, PPCSTATE_OFF(gpr[i]));
					Unlock(host_reg_1);
				}
				// Flush both registers
				m_guest_registers[i].Flush();
				m_guest_registers[i + 1].Flush();
				Unlock(DecodeReg(host_reg));
			}
			// Skip the next register since we've handled it here
			++i;
		}
	}
}

ARM64Reg Arm64GPRCache::R(u32 preg)
{
	OpArg& reg = m_guest_registers[preg];
	switch (reg.GetType())
	{
	case REG_REG: // already in a reg
		return reg.GetReg();
	break;
	case REG_IMM: // Is an immediate
	{
		ARM64Reg host_reg = GetReg();
		m_emit->MOVI2R(host_reg, reg.GetImm());
	}
	break;
	case REG_AWAY: // Register is away in a shared register
	{
		// Let's do the voodoo that we dodo
		if (reg.GetReg() == INVALID_REG)
		{
			// Alright, we need to move to a valid location
			ARM64Reg host_reg = GetReg();
			reg.LoadAwayToReg(host_reg);

			// Alright, we need to extract from our away register
			// To our new 32bit register
			if (reg.GetAwayLocation() == REG_LOW)
			{
				// We are in the low bits
				// Just move it over to the low bits of the new register
				m_emit->UBFM(EncodeRegTo64(host_reg), reg.GetAwayReg(), 0, 31);
			}
			else
			{
				// We are in the high bits
				m_emit->UBFM(EncodeRegTo64(host_reg), reg.GetAwayReg(), 32, 63);
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
		// This is kind of annoying, we shouldn't have gotten here
		// This can happen with instructions that use multiple registers(eg lmw)
		// The PPCAnalyst needs to be modified to handle these cases
		_dbg_assert_msg_(DYNA_REC, false, "Hit REG_NOTLOADED type oparg. Fix the PPCAnalyst");
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

void Arm64GPRCache::GetAllocationOrder(void)
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

// FPR Cache
void Arm64FPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
	// XXX: Flush our stuff
}

ARM64Reg Arm64FPRCache::R(u32 preg)
{
	// XXX: return a host reg holding a guest register
}

void Arm64FPRCache::GetAllocationOrder(void)
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

