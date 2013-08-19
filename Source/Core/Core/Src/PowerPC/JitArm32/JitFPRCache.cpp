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
#include "JitFPRCache.h"

ArmFPRCache::ArmFPRCache()
{
	emit = 0;
}

void ArmFPRCache::Init(ARMXEmitter *emitter)
{
	emit = emitter;
	ARMReg *PPCRegs = GetPPCAllocationOrder(NUMPPCREG);
	ARMReg *Regs = GetAllocationOrder(NUMARMREG);

	for(u8 a = 0; a < NUMPPCREG; ++a)
	{
		ArmCRegs[a].PPCReg = 33;
		ArmCRegs[a].Reg = PPCRegs[a];
		ArmCRegs[a].LastLoad = 0;
		ArmCRegs[a].PS1 = false;
		ArmCRegs[a].Away = true;
	}
	for(u8 a = 0; a < NUMARMREG; ++a)
	{
		ArmRegs[a].Reg = Regs[a];
		ArmRegs[a].free = true;
	}
}
void ArmFPRCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	for(u8 a = 0; a < NUMPPCREG; ++a)
	{
		ArmCRegs[a].PPCReg = 33;
		ArmCRegs[a].LastLoad = 0;
	}
}
ARMReg *ArmFPRCache::GetPPCAllocationOrder(int &count)
{
	// This will return us the allocation order of the registers we can use on
	// the ppc side.
	static ARMReg allocationOrder[] = 
	{
		D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, 
		D14, D15, D16, D17, D18, D19, D20, D21, D22, 
		D23, D24, D25, D26, D27, D28, D29, D30, D31
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}
ARMReg *ArmFPRCache::GetAllocationOrder(int &count)
{
	// This will return us the allocation order of the registers we can use on
	// the host side.
	static ARMReg allocationOrder[] = 
	{
		D0, D1, D2, D3
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}

ARMReg ArmFPRCache::GetReg(bool AutoLock)
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
	return D31;
}
void ArmFPRCache::Unlock(ARMReg V0)
{
	for(u8 RegNum = 0; RegNum < NUMARMREG; ++RegNum)
	{
		if(ArmRegs[RegNum].Reg == V0)
		{
			_assert_msg_(_DYNA_REC, !ArmRegs[RegNum].free, "This register is already unlocked");
			ArmRegs[RegNum].free = true;
		}
	}
}
ARMReg ArmFPRCache::GetPPCReg(u32 preg, bool PS1, bool preLoad)
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
		if (ArmCRegs[a].PPCReg == preg && ArmCRegs[a].PS1 == PS1)
		{
			ArmCRegs[a].LastLoad = 0;
			// Check if the value is actually in the reg
			if (ArmCRegs[a].Away && preLoad)
			{
				// Load it now since we want it
				s16 offset = PPCSTATE_OFF(ps) + (preg * 16) + (PS1 ? 8 : 0);
				emit->VLDR(ArmCRegs[a].Reg, R9, offset);
				ArmCRegs[a].Away = false;
			}
			return ArmCRegs[a].Reg;
		}
	// Check if we have a free register
	for (u8 a = 0; a < NUMPPCREG; ++a)
		if (ArmCRegs[a].PPCReg == 33)
		{
			s16 offset = PPCSTATE_OFF(ps) + (preg * 16) + (PS1 ? 8 : 0);
			if (preLoad)
				emit->VLDR(ArmCRegs[a].Reg, R9, offset);
			ArmCRegs[a].PPCReg = preg;
			ArmCRegs[a].LastLoad = 0;
			ArmCRegs[a].PS1 = PS1;
			ArmCRegs[a].Away = !preLoad;
			return ArmCRegs[a].Reg;
		}
	// Alright, we couldn't get a free space, dump that least used register
	s16 offsetOld = PPCSTATE_OFF(ps) + (ArmCRegs[Num].PPCReg * 16) + (ArmCRegs[Num].PS1 ? 8 : 0);
	emit->VSTR(ArmCRegs[Num].Reg, R9, offsetOld);
	
	s16 offsetNew = PPCSTATE_OFF(ps) + (preg * 16) + (PS1 ? 8 : 0);
	if (preLoad)
		emit->VLDR(ArmCRegs[Num].Reg, R9, offsetNew);
	ArmCRegs[Num].PPCReg = preg;
	ArmCRegs[Num].LastLoad = 0;
	ArmCRegs[Num].PS1 = PS1;
	ArmCRegs[Num].Away = !preLoad;
	return ArmCRegs[Num].Reg;		 

}

ARMReg ArmFPRCache::R0(u32 preg, bool preLoad)
{
	return GetPPCReg(preg, false, preLoad);
}

ARMReg ArmFPRCache::R1(u32 preg, bool preLoad)
{
	return GetPPCReg(preg, true, preLoad);
}

void ArmFPRCache::Flush()
{
	for(u8 a = 0; a < NUMPPCREG; ++a)
		if (ArmCRegs[a].PPCReg != 33)
		{
			s16 offset =  PPCSTATE_OFF(ps) + (ArmCRegs[a].PPCReg * 16) + (ArmCRegs[a].PS1 ? 8 : 0);
			emit->VSTR(ArmCRegs[a].Reg, R9, offset);
			ArmCRegs[a].PPCReg = 33;
			ArmCRegs[a].LastLoad = 0;
			ArmCRegs[a].Away = true;
		}
}

