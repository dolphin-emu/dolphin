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

void JitArm::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(FloatingPoint)
	Default(inst); return;

	ARMReg vD = fpr.R0(inst.FD);
	ARMReg vB = fpr.R0(inst.FB);

	VMOV(vD, vB);
	
	if (inst.Rc) Helper_UpdateCR1(vD);
}

