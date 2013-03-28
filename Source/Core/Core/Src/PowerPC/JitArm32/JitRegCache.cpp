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

#include "Jit.h"
#include "JitRegCache.h"

ArmRegCache::ArmRegCache()
{
	emit = 0;
}

void ArmRegCache::Init(ARMXEmitter *emitter)
{
	emit = emitter;
	ARMReg *PPCRegs = GetPPCAllocationOrder(NUMPPCREG);
	ARMReg *Regs = GetAllocationOrder(NUMARMREG);
	for(u8 a = 0; a < 32; ++a)
	{
		// This gives us the memory locations of the gpr registers so we can
		// load them.
		regs[a].location = (u8*)&PowerPC::ppcState.gpr[a]; 	
		regs[a].UsesLeft = 0;
	}
	for(u8 a = 0; a < NUMPPCREG; ++a)
	{
		ArmCRegs[a].PPCReg = 33;
		ArmCRegs[a].Reg = PPCRegs[a];
		ArmCRegs[a].LastLoad = 0;
	}
	for(u8 a = 0; a < NUMARMREG; ++a)
	{
		ArmRegs[a].Reg = Regs[a];
		ArmRegs[a].free = true;
	}
}
void ArmRegCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	for(u8 a = 0; a < NUMPPCREG; ++a)
	{
		ArmCRegs[a].PPCReg = 33;
		ArmCRegs[a].LastLoad = 0;
	}
	for(u8 a = 0; a < 32; ++a)
		regs[a].UsesLeft = stats.GetTotalNumAccesses(a);	
}
ARMReg *ArmRegCache::GetPPCAllocationOrder(int &count)
{
	// This will return us the allocation order of the registers we can use on
	// the ppc side.
	static ARMReg allocationOrder[] = 
	{
		R0, R1, R2, R3, R4, R5, R6, R7, R8
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}
ARMReg *ArmRegCache::GetAllocationOrder(int &count)
{
	// This will return us the allocation order of the registers we can use on
	// the host side.
	static ARMReg allocationOrder[] = 
	{
		R14, R12, R11, R10
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}

ARMReg ArmRegCache::GetReg(bool AutoLock)
{
	for(u8 a = 0; a < NUMARMREG; ++a)
		if(ArmRegs[a].free)
		{
			// Alright, this one is free
			if (AutoLock)
				ArmRegs[a].free = false;
			return ArmRegs[a].Reg;
		}
	// Uh Oh, we have all them locked....
	_assert_msg_(_DYNA_REC_, false, "All available registers are locked dumb dumb");
	return R0;
}
void ArmRegCache::Lock(ARMReg Reg)
{
	for(u8 RegNum = 0; RegNum < NUMARMREG; ++RegNum)
		if(ArmRegs[RegNum].Reg == Reg)
		{
			_assert_msg_(_DYNA_REC, ArmRegs[RegNum].free, "This register is already locked");
			ArmRegs[RegNum].free = false;
		}
	_assert_msg_(_DYNA_REC, false, "Register %d can't be used with lock", Reg);
}
void ArmRegCache::Unlock(ARMReg R0, ARMReg R1, ARMReg R2, ARMReg R3)
{
	for(u8 RegNum = 0; RegNum < NUMARMREG; ++RegNum)
	{
		if(ArmRegs[RegNum].Reg == R0)
		{
			_assert_msg_(_DYNA_REC, !ArmRegs[RegNum].free, "This register is already unlocked");
			ArmRegs[RegNum].free = true;
		}
		if( R1 != INVALID_REG && ArmRegs[RegNum].Reg == R1) ArmRegs[RegNum].free = true;
		if( R2 != INVALID_REG && ArmRegs[RegNum].Reg == R2) ArmRegs[RegNum].free = true;
		if( R3 != INVALID_REG && ArmRegs[RegNum].Reg == R3) ArmRegs[RegNum].free = true;
	}
}

ARMReg ArmRegCache::R(u32 preg)
{
	u32 HighestUsed = 0;
	u8 Num = 0;
	for(u8 a = 0; a < NUMPPCREG; ++a){
		++ArmCRegs[a].LastLoad;
		if (ArmCRegs[a].LastLoad > HighestUsed)
		{
			HighestUsed = ArmCRegs[a].LastLoad;
			Num = a;
		}
	}
	// Check if already Loaded
	for(u8 a = 0; a < NUMPPCREG; ++a)
		if (ArmCRegs[a].PPCReg == preg)
		{
			ArmCRegs[a].LastLoad = 0;
			return ArmCRegs[a].Reg;
		}
	// Check if we have a free register
	for (u8 a = 0; a < NUMPPCREG; ++a)
		if (ArmCRegs[a].PPCReg == 33)
		{
			emit->LDR(ArmCRegs[a].Reg, R9, PPCSTATE_OFF(gpr) + preg * 4);
			ArmCRegs[a].PPCReg = preg;
			ArmCRegs[a].LastLoad = 0;
			return ArmCRegs[a].Reg;
		}
	// Alright, we couldn't get a free space, dump that least used register
	emit->STR(ArmCRegs[Num].Reg, R9, PPCSTATE_OFF(gpr) + ArmCRegs[Num].PPCReg * 4);
	emit->LDR(ArmCRegs[Num].Reg, R9, PPCSTATE_OFF(gpr) + preg * 4);
	ArmCRegs[Num].PPCReg = preg;
	ArmCRegs[Num].LastLoad = 0;
	return ArmCRegs[Num].Reg;		 
}

void ArmRegCache::Flush()
{
	for(u8 a = 0; a < NUMPPCREG; ++a)
		if (ArmCRegs[a].PPCReg != 33)
		{
			emit->STR(ArmCRegs[a].Reg, R9, PPCSTATE_OFF(gpr) + ArmCRegs[a].PPCReg * 4);
			ArmCRegs[a].PPCReg = 33;
			ArmCRegs[a].LastLoad = 0;
		}
}

