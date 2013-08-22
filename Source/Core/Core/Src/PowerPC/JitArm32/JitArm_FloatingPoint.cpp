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

#include "Common.h"
#include "Thunk.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../../ConfigManager.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "ArmEmitter.h"
#include "../../HW/Memmap.h"


#include "Jit.h"
#include "JitRegCache.h"
#include "JitFPRCache.h"
#include "JitAsm.h"

void JitArm::Helper_UpdateCR1(ARMReg value)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

void JitArm::fabsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vD = fpr.R0(inst.FD);
	ARMReg vB = fpr.R0(inst.FB);

	VABS(vD, vB);

	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::faddsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD0 = fpr.R0(inst.FD);
	ARMReg vD1 = fpr.R1(inst.FD);

	VADD(vD0, vA, vB);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}

void JitArm::faddx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vD = fpr.R0(inst.FD);
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);

	VADD(vD, vA, vB);
	if (inst.Rc) Helper_UpdateCR1(vD);
}

void JitArm::fsubsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
	
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);
	ARMReg vD0 = fpr.R0(inst.FD);
	ARMReg vD1 = fpr.R1(inst.FD);

	VSUB(vD0, vA, vB);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}

void JitArm::fsubx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vD = fpr.R0(inst.FD);
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vB = fpr.R0(inst.FB);

	VSUB(vD, vA, vB);
	if (inst.Rc) Helper_UpdateCR1(vD);
}

// Breaks Animal Crossing
void JitArm::fmulsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
	
	Default(inst); return;
	
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vC = fpr.R0(inst.FC);
	ARMReg vD0 = fpr.R0(inst.FD);
	ARMReg vD1 = fpr.R1(inst.FD);

	VMUL(vD0, vA, vC);
	VMOV(vD1, vD0);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}
void JitArm::fmulx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vD0 = fpr.R0(inst.FD);
	ARMReg vA = fpr.R0(inst.FA);
	ARMReg vC = fpr.R0(inst.FC);

	VMUL(vD0, vA, vC);
	if (inst.Rc) Helper_UpdateCR1(vD0);
}
void JitArm::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)

	ARMReg vD = fpr.R0(inst.FD);
	ARMReg vB = fpr.R0(inst.FB);

	VMOV(vD, vB);
	
	if (inst.Rc) Helper_UpdateCR1(vD);
}

