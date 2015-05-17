// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/Arm64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"

// Dedicated host registers
// X29 = ppcState pointer
using namespace Arm64Gen;

enum RegType
{
	REG_NOTLOADED = 0,
	REG_REG, // Reg type is register
	REG_IMM, // Reg is really a IMM
};
enum RegLocation
{
	REG_LOW = 0,
	REG_HIGH,
};

enum FlushMode
{
	// Flushes all registers, no exceptions
	FLUSH_ALL = 0,
	// Flushes registers in a conditional branch
	// Doesn't wipe the state of the registers from the cache
	FLUSH_MAINTAIN_STATE,
	// Flushes only the required registers for an interpreter call
	FLUSH_INTERPRETER,
};

class OpArg
{
public:
	OpArg()
		: m_type(REG_NOTLOADED), m_reg(INVALID_REG),
		  m_value(0), m_last_used(0)
	{
	}

	RegType GetType()
	{
		return m_type;
	}

	ARM64Reg GetReg()
	{
		return m_reg;
	}
	u32 GetImm()
	{
		return m_value;
	}
	void LoadToReg(ARM64Reg reg)
	{
		m_type = REG_REG;
		m_reg = reg;
	}
	void LoadToImm(u32 imm)
	{
		m_type = REG_IMM;
		m_value = imm;

		m_reg = INVALID_REG;
	}
	void Flush()
	{
		// Invalidate any previous information
		m_type = REG_NOTLOADED;
		m_reg = INVALID_REG;

		// Arbitrarily large value that won't roll over on a lot of increments
		m_last_used = 0xFFFF;
	}

	u32 GetLastUsed() { return m_last_used; }
	void ResetLastUsed() { m_last_used = 0; }
	void IncrementLastUsed() { ++m_last_used; }

	void SetDirty(bool dirty) { m_dirty = dirty; }
	bool IsDirty() { return m_dirty; }

private:
	// For REG_REG
	RegType m_type; // store type
	ARM64Reg m_reg; // host register we are in

	// For REG_IMM
	u32 m_value; // IMM value

	u32 m_last_used;

	bool m_dirty;
};

class HostReg
{
public:
	HostReg() : m_reg(INVALID_REG), m_locked(false) {}
	HostReg(ARM64Reg reg) : m_reg(reg), m_locked(false) {}
	bool IsLocked() { return m_locked; }
	void Lock() { m_locked = true; }
	void Unlock() { m_locked = false; }
	ARM64Reg GetReg() { return m_reg; }

	bool operator==(const ARM64Reg& reg)
	{
		return reg == m_reg;
	}

private:
	ARM64Reg m_reg;
	bool m_locked;
};

class Arm64RegCache
{
public:
	Arm64RegCache() : m_emit(nullptr), m_float_emit(nullptr), m_reg_stats(nullptr) {};
	virtual ~Arm64RegCache() {};

	void Init(ARM64XEmitter *emitter);

	virtual void Start(PPCAnalyst::BlockRegStats &stats) {}

	// Flushes the register cache in different ways depending on the mode
	virtual void Flush(FlushMode mode, PPCAnalyst::CodeOp* op) = 0;

	// Returns a guest register inside of a host register
	// Will dump an immediate to the host register as well
	virtual ARM64Reg R(u32 reg) = 0;

	virtual BitSet32 GetCallerSavedUsed() = 0;

	// Returns a temporary register for use
	// Requires unlocking after done
	ARM64Reg GetReg();

	void StoreRegister(u32 preg) { FlushRegister(preg, false); }

	// Locks a register so a cache cannot use it
	// Useful for function calls
	template<typename T = ARM64Reg, typename... Args>
	void Lock(Args... args)
	{
		for (T reg : {args...})
		{
			FlushByHost(reg);
			LockRegister(reg);
		}
	}

	// Unlocks a locked register
	// Unlocks registers locked with both GetReg and LockRegister
	template<typename T = ARM64Reg, typename... Args>
	void Unlock(Args... args)
	{
		for (T reg : {args...})
		{
			FlushByHost(reg);
			UnlockRegister(reg);
		}
	}

protected:
	// Get the order of the host registers
	virtual void GetAllocationOrder() = 0;

	// Flushes the most stale register
	void FlushMostStaleRegister();

	// Lock a register
	void LockRegister(ARM64Reg host_reg);

	// Unlock a register
	void UnlockRegister(ARM64Reg host_reg);

	// Flushes a guest register by host provided
	virtual void FlushByHost(ARM64Reg host_reg) = 0;

	virtual void FlushRegister(u32 preg, bool maintain_state) = 0;

	// Get available host registers
	u32 GetUnlockedRegisterCount();

	void IncrementAllUsed()
	{
		for (auto& reg : m_guest_registers)
			reg.IncrementLastUsed();
	}

	// Code emitter
	ARM64XEmitter *m_emit;

	// Float emitter
	std::unique_ptr<ARM64FloatEmitter> m_float_emit;

	// Host side registers that hold the host registers in order of use
	std::vector<HostReg> m_host_registers;

	// Our guest GPRs
	// PowerPC has 32 GPRs
	// PowerPC also has 32 paired FPRs
	OpArg m_guest_registers[32];

	// Register stats for the current block
	PPCAnalyst::BlockRegStats *m_reg_stats;
};

class Arm64GPRCache : public Arm64RegCache
{
public:
	~Arm64GPRCache() {}

	void Start(PPCAnalyst::BlockRegStats &stats);

	// Flushes the register cache in different ways depending on the mode
	void Flush(FlushMode mode, PPCAnalyst::CodeOp* op = nullptr);

	// Returns a guest register inside of a host register
	// Will dump an immediate to the host register as well
	ARM64Reg R(u32 preg);

	// Set a register to an immediate
	void SetImmediate(u32 preg, u32 imm);

	// Returns if a register is set as an immediate
	bool IsImm(u32 reg) { return m_guest_registers[reg].GetType() == REG_IMM; }

	// Gets the immediate that a register is set to
	u32 GetImm(u32 reg) { return m_guest_registers[reg].GetImm(); }

	void BindToRegister(u32 preg, bool do_load);

	BitSet32 GetCallerSavedUsed() override;

protected:
	// Get the order of the host registers
	void GetAllocationOrder();

	// Flushes a guest register by host provided
	void FlushByHost(ARM64Reg host_reg) override;

	void FlushRegister(u32 preg, bool maintain_state) override;

private:
	bool IsCalleeSaved(ARM64Reg reg);

};

class Arm64FPRCache : public Arm64RegCache
{
public:
	~Arm64FPRCache() {}
	// Flushes the register cache in different ways depending on the mode
	void Flush(FlushMode mode, PPCAnalyst::CodeOp* op = nullptr);

	// Returns a guest register inside of a host register
	// Will dump an immediate to the host register as well
	ARM64Reg R(u32 preg);

	void BindToRegister(u32 preg, bool do_load);

	BitSet32 GetCallerSavedUsed() override;

protected:
	// Get the order of the host registers
	void GetAllocationOrder();

	// Flushes a guest register by host provided
	void FlushByHost(ARM64Reg host_reg) override;

	void FlushRegister(u32 preg, bool maintain_state) override;

private:
	bool IsCalleeSaved(ARM64Reg reg);
};
