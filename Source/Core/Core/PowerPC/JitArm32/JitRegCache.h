// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ArmEmitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"

// This ARM Register cache actually pre loads the most used registers before
// the block to increase speed since every memory load requires two
// instructions to load it. We are going to use R0-RMAX as registers for the
// use of PPC Registers.
// Allocation order as follows
#define ARMREGS 16
// Allocate R0 to R9 for PPC first.
// For General registers on the host side, start with R14 and go down as we go
// R13 is reserved for our stack pointer, don't ever use that. Unless you save
// it
// So we have R14, R12, R11, R10 to work with instructions

enum RegType
{
	REG_NOTLOADED = 0,
	REG_REG, // Reg type is register
	REG_IMM, // Reg is really a IMM
	REG_AWAY, // Bound to a register, but not preloaded
};

enum FlushMode
{
	FLUSH_ALL = 0,
	FLUSH_MAINTAIN_STATE,
};

class OpArg
{
	private:
	RegType m_type; // store type
	u8 m_reg; // index to register
	u32 m_value; // IMM value

	public:
	OpArg()
	{
		m_type = REG_NOTLOADED;
		m_reg = 33;
		m_value = 0;
	}

	RegType GetType()
	{
		return m_type;
	}

	u8 GetRegIndex()
	{
		return m_reg;
	}
	u32 GetImm()
	{
		return m_value;
	}
	void LoadToAway(u8 reg)
	{
		m_type = REG_AWAY;
		m_reg = reg;
	}
	void LoadToReg(u8 reg)
	{
		m_type = REG_REG;
		m_reg = reg;
	}
	void LoadToImm(u32 imm)
	{
		m_type = REG_IMM;
		m_value = imm;
	}
	void Flush()
	{
		m_type = REG_NOTLOADED;
	}
};

struct JRCPPC
{
	u32 PPCReg; // Tied to which PPC Register
	bool PS1;
	ArmGen::ARMReg Reg; // Tied to which ARM Register
	u32 LastLoad;
};
struct JRCReg
{
	ArmGen::ARMReg Reg; // Which reg this is.
	bool free;
};
class ArmRegCache
{
private:
	OpArg regs[32];
	JRCPPC ArmCRegs[ARMREGS];
	JRCReg ArmRegs[ARMREGS]; // Four registers remaining

	int NUMPPCREG;
	int NUMARMREG;

	ArmGen::ARMReg *GetAllocationOrder(int &count);
	ArmGen::ARMReg *GetPPCAllocationOrder(int &count);

	u32 GetLeastUsedRegister(bool increment);
	bool FindFreeRegister(u32 &regindex);

	// Private function can kill immediates
	ArmGen::ARMReg BindToRegister(u32 preg, bool doLoad, bool kill_imm);

protected:
	ArmGen::ARMXEmitter *emit;

public:
	ArmRegCache();
	~ArmRegCache() {}

	void Init(ArmGen::ARMXEmitter *emitter);
	void Start(PPCAnalyst::BlockRegStats &stats);

	ArmGen::ARMReg GetReg(bool AutoLock = true); // Return a ARM register we can use.
	void Unlock(ArmGen::ARMReg R0, ArmGen::ARMReg R1 = ArmGen::INVALID_REG, ArmGen::ARMReg R2 = ArmGen::INVALID_REG, ArmGen::ARMReg R3 = ArmGen::INVALID_REG);
	void Flush(FlushMode mode = FLUSH_ALL);
	ArmGen::ARMReg R(u32 preg); // Returns a cached register
	bool IsImm(u32 preg) { return regs[preg].GetType() == REG_IMM; }
	u32 GetImm(u32 preg) { return regs[preg].GetImm(); }
	void SetImmediate(u32 preg, u32 imm);

	// Public function doesn't kill immediates
	// In reality when you call R(u32) it'll bind an immediate there
	void BindToRegister(u32 preg, bool doLoad = true);

	void StoreFromRegister(u32 preg);
};
