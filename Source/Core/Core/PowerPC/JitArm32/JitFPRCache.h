// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ArmEmitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

#define ARMFPUREGS 32
using namespace ArmGen;

class ArmFPRCache
{
private:
	OpArg _regs[32][2]; // One for each FPR reg
	JRCPPC ArmCRegs[ARMFPUREGS];
	JRCReg ArmRegs[ARMFPUREGS];

	int NUMPPCREG;
	int NUMARMREG;

	ARMReg *GetAllocationOrder(int &count);
	ARMReg *GetPPCAllocationOrder(int &count);

	ARMReg GetPPCReg(u32 preg, bool PS1, bool preLoad);

	u32 GetLeastUsedRegister(bool increment);
	bool FindFreeRegister(u32 &regindex);
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
