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

#ifndef _JITARMFPRCACHE_H
#define _JITARMFPRCACHE_H

#include "ArmEmitter.h"
#include "../Gekko.h"
#include "../PPCAnalyst.h"
#include "JitRegCache.h"

#define ARMFPUREGS 32
using namespace ArmGen;

class ArmFPRCache
{
private:
	JRCPPC ArmCRegs[ARMFPUREGS];
	JRCReg ArmRegs[ARMFPUREGS]; 
	
	int NUMPPCREG;
	int NUMARMREG;

	ARMReg *GetAllocationOrder(int &count);
	ARMReg *GetPPCAllocationOrder(int &count);

	ARMReg GetPPCReg(u32 preg, bool PS1, bool preLoad);

protected:
	ARMXEmitter *emit;
	
public:
	ArmFPRCache();
	~ArmFPRCache() {}

	void Init(ARMXEmitter *emitter);
	void Start(PPCAnalyst::BlockRegStats &stats);

	void SetEmitter(ARMXEmitter *emitter) {emit = emitter;}
	
	ARMReg GetReg(bool AutoLock = true); // Return a ARM register we can use.
	void Unlock(ARMReg V0);
	void Flush();
	ARMReg R0(u32 preg, bool preLoad = true); // Returns a cached register
	ARMReg R1(u32 preg, bool preLoad = true);
};
#endif
