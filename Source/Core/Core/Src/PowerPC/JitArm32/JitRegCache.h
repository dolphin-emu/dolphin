// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _JITARMREGCACHE_H
#define _JITARMREGCACHE_H

#include "ArmEmitter.h"
#include "../Gekko.h"
#include "../PPCAnalyst.h"

using namespace ArmGen;
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
	REG_REG,
	REG_IMM,
};

class OpArg 
{
	private:
	class Reg{
		public:
		RegType m_type;
		u8 m_reg; // index to register
		u32 m_value;
		Reg()
		{
			m_type = REG_NOTLOADED;
			m_reg = 33;
			m_value = 0;
		}
	} Reg;

	public:
	OpArg(){}
	
	RegType GetType()
	{
		return Reg.m_type;
	}

	u8 GetRegIndex()
	{
		return Reg.m_reg;
	}
	u32 GetImm()
	{
		return Reg.m_value;
	}
	void LoadToReg(u8 reg)
	{
		Reg.m_type = REG_REG;
		Reg.m_reg = reg;
	}
	void LoadToImm(u32 imm)
	{
		Reg.m_type = REG_IMM;
		Reg.m_value = imm;
	}
	void Flush()
	{
		Reg.m_type = REG_NOTLOADED;
	}
};

struct JRCPPC
{
	u32 PPCReg; // Tied to which PPC Register
	bool PS1;
	ARMReg Reg; // Tied to which ARM Register
	u32 LastLoad;
	bool Away; // Only used in FPR cache
};
struct JRCReg
{
	ARMReg Reg; // Which reg this is.
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

	ARMReg *GetAllocationOrder(int &count);
	ARMReg *GetPPCAllocationOrder(int &count);
	
	u32 GetLeastUsedRegister(bool increment);
	bool FindFreeRegister(u32 &regindex);
protected:
	ARMXEmitter *emit;
	
public:
	ArmRegCache();
	~ArmRegCache() {}

	void Init(ARMXEmitter *emitter);
	void Start(PPCAnalyst::BlockRegStats &stats);
	
	ARMReg GetReg(bool AutoLock = true); // Return a ARM register we can use.
	void Unlock(ARMReg R0, ARMReg R1 = INVALID_REG, ARMReg R2 = INVALID_REG, ARMReg R3 =
	INVALID_REG);
	void Flush();
	ARMReg R(u32 preg); // Returns a cached register
	bool IsImm(u32 preg) { return regs[preg].GetType() == REG_IMM; }
	u32 GetImm(u32 preg) { return regs[preg].GetImm(); }
	void SetImmediate(u32 preg, u32 imm); 
	ARMReg BindToRegister(u32 preg);
};




#endif
