// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"

using namespace Arm64Gen;

void Arm64RegCache::Init(ARM64XEmitter *emitter)
{
	m_emit = emitter;
	m_float_emit.reset(new ARM64FloatEmitter(m_emit));
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
	WARN_LOG(DYNA_REC, "All available registers are locked dumb dumb");
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
	_assert_msg_(DYNA_REC, reg != m_host_registers.end(), "Don't try locking a register that isn't in the cache. Reg %d", host_reg);
	reg->Lock();
}

void Arm64RegCache::UnlockRegister(ARM64Reg host_reg)
{
	auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
	_assert_msg_(DYNA_REC, reg != m_host_registers.end(), "Don't try unlocking a register that isn't in the cache. Reg %d", host_reg);
	reg->Unlock();
}

void Arm64RegCache::FlushMostStaleRegister()
{
	u32 most_stale_preg = 0;
	u32 most_stale_amount = 0;
	for (u32 i = 0; i < 32; ++i)
	{
		u32 last_used = m_guest_registers[i].GetLastUsed();
		if (last_used > most_stale_amount &&
		    m_guest_registers[i].GetType() == REG_REG)
		{
			most_stale_preg = i;
			most_stale_amount = last_used;
		}
	}
	FlushRegister(most_stale_preg, false);
}

// GPR Cache
void Arm64GPRCache::Start(PPCAnalyst::BlockRegStats &stats)
{
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

void Arm64GPRCache::FlushRegister(u32 preg, bool maintain_state)
{
	OpArg& reg = m_guest_registers[preg];
	if (reg.GetType() == REG_REG)
	{
		ARM64Reg host_reg = reg.GetReg();
		if (reg.IsDirty())
			m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));

		if (!maintain_state)
		{
			UnlockRegister(host_reg);
			reg.Flush();
		}
	}
	else if (reg.GetType() == REG_IMM)
	{
		if (!reg.GetImm())
		{
			m_emit->STR(INDEX_UNSIGNED, WSP, X29, PPCSTATE_OFF(gpr[preg]));
		}
		else
		{
			ARM64Reg host_reg = GetReg();

			m_emit->MOVI2R(host_reg, reg.GetImm());
			m_emit->STR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));

			UnlockRegister(host_reg);
		}

		if (!maintain_state)
			reg.Flush();
	}
}

void Arm64GPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
	for (int i = 0; i < 32; ++i)
	{
		bool flush = true;
		if (mode == FLUSH_INTERPRETER)
		{
			if (!(op->regsOut[i] || op->regsIn[i]))
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
				FlushRegister(i, mode == FLUSH_MAINTAIN_STATE);
		}
		else if (m_guest_registers[i].GetType() == REG_IMM)
		{
			if (flush)
				FlushRegister(i, mode == FLUSH_MAINTAIN_STATE);
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
		reg.SetDirty(true);
		return host_reg;
	}
	break;
	case REG_NOTLOADED: // Register isn't loaded at /all/
	{
		// This is a bit annoying. We try to keep these preloaded as much as possible
		// This can also happen on cases where PPCAnalyst isn't feeing us proper register usage statistics
		ARM64Reg host_reg = GetReg();
		reg.LoadToReg(host_reg);
		reg.SetDirty(false);
		m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));
		return host_reg;
	}
	break;
	default:
		ERROR_LOG(DYNA_REC, "Invalid OpArg Type!");
	break;
	}
	// We've got an issue if we end up here
	return INVALID_REG;
}

void Arm64GPRCache::SetImmediate(u32 preg, u32 imm)
{
	OpArg& reg = m_guest_registers[preg];
	if (reg.GetType() == REG_REG)
		UnlockRegister(reg.GetReg());
	reg.LoadToImm(imm);
}

void Arm64GPRCache::BindToRegister(u32 preg, bool do_load)
{
	OpArg& reg = m_guest_registers[preg];

	reg.SetDirty(true);
	if (reg.GetType() == REG_NOTLOADED)
	{
		ARM64Reg host_reg = GetReg();
		reg.LoadToReg(host_reg);
		if (do_load)
			m_emit->LDR(INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(gpr[preg]));
	}
}

void Arm64GPRCache::GetAllocationOrder()
{
	// Callee saved registers first in hopes that we will keep everything stored there first
	const std::vector<ARM64Reg> allocation_order =
	{
		// Callee saved
		W28, W27, W26, W25, W24, W23, W22, W21, W20,
		W19,

		// Caller saved
		W18, W17, W16, W15, W14, W13, W12, W11, W10,
		W9, W8, W7, W6, W5, W4, W3, W2, W1, W0,
		W30,
	};

	for (ARM64Reg reg : allocation_order)
		m_host_registers.push_back(HostReg(reg));
}

BitSet32 Arm64GPRCache::GetCallerSavedUsed()
{
	BitSet32 registers(0);
	for (auto& it : m_host_registers)
		if (it.IsLocked() && !IsCalleeSaved(it.GetReg()))
				registers[it.GetReg()] = 1;
	return registers;
}

void Arm64GPRCache::FlushByHost(ARM64Reg host_reg)
{
	for (int i = 0; i < 32; ++i)
	{
		OpArg& reg = m_guest_registers[i];
		if (reg.GetType() == REG_REG && reg.GetReg() == host_reg)
		{
			FlushRegister(i, false);
			return;
		}
	}
}

// FPR Cache
void Arm64FPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
	for (int i = 0; i < 32; ++i)
	{
		bool flush = true;
		if (mode == FLUSH_INTERPRETER)
		{
			if (!(op->regsOut[i] || op->regsIn[i]))
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
				FlushRegister(i, mode == FLUSH_MAINTAIN_STATE);
		}
	}
}

ARM64Reg Arm64FPRCache::R(u32 preg)
{
	OpArg& reg = m_guest_registers[preg];
	IncrementAllUsed();
	reg.ResetLastUsed();

	switch (reg.GetType())
	{
	case REG_REG: // already in a reg
		return reg.GetReg();
	break;
	case REG_NOTLOADED: // Register isn't loaded at /all/
	{
		ARM64Reg host_reg = GetReg();
		reg.LoadToReg(host_reg);
		reg.SetDirty(false);
		m_float_emit->LDR(128, INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(ps[preg][0]));
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

void Arm64FPRCache::BindToRegister(u32 preg, bool do_load)
{
	OpArg& reg = m_guest_registers[preg];

	reg.SetDirty(true);
	if (reg.GetType() == REG_NOTLOADED)
	{
		ARM64Reg host_reg = GetReg();
		reg.LoadToReg(host_reg);
		if (do_load)
			m_float_emit->LDR(128, INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(ps[preg][0]));
	}
}

void Arm64FPRCache::GetAllocationOrder()
{
	const std::vector<ARM64Reg> allocation_order =
	{
		// Callee saved
		Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15,

		// Caller saved
		Q16, Q17, Q18, Q19, Q20, Q21, Q22, Q23,
		Q24, Q25, Q26, Q27, Q28, Q29, Q30, Q31,
		Q7, Q6, Q5, Q4, Q3, Q2, Q1, Q0
	};

	for (ARM64Reg reg : allocation_order)
		m_host_registers.push_back(HostReg(reg));
}

void Arm64FPRCache::FlushByHost(ARM64Reg host_reg)
{
	// XXX: Scan guest registers and flush if found
}

bool Arm64FPRCache::IsCalleeSaved(ARM64Reg reg)
{
	static std::vector<ARM64Reg> callee_regs =
	{
		Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15, INVALID_REG,
	};
	return std::find(callee_regs.begin(), callee_regs.end(), EncodeRegTo64(reg)) != callee_regs.end();
}

void Arm64FPRCache::FlushRegister(u32 preg, bool maintain_state)
{
	OpArg& reg = m_guest_registers[preg];
	if (reg.GetType() == REG_REG)
	{
		ARM64Reg host_reg = reg.GetReg();

		if (reg.IsDirty())
			m_float_emit->STR(128, INDEX_UNSIGNED, host_reg, X29, PPCSTATE_OFF(ps[preg][0]));

		if (!maintain_state)
		{
			UnlockRegister(host_reg);
			reg.Flush();
		}
	}
}

BitSet32 Arm64FPRCache::GetCallerSavedUsed()
{
	BitSet32 registers(0);
	for (auto& it : m_host_registers)
		if (it.IsLocked())
			registers[Q0 - it.GetReg()] = 1;
	return registers;
}
