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
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "ArmEmitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitAsm.h"

void JitArm::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

	switch (iIndex)
	{
	case SPR_LR:
	case SPR_CTR:
	case SPR_XER:
		// These are safe to do the easy way, see the bottom of this function.
		break;

	case SPR_GQR0:
	case SPR_GQR0 + 1:
	case SPR_GQR0 + 2:
	case SPR_GQR0 + 3:
	case SPR_GQR0 + 4:
	case SPR_GQR0 + 5:
	case SPR_GQR0 + 6:
	case SPR_GQR0 + 7:
		// Prevent recompiler from compiling in old quantizer values.
		// If the value changed, destroy all blocks using this quantizer
		// This will create a little bit of block churn, but hopefully not too bad.
		{
			/*
			MOV(32, R(EAX), M(&PowerPC::ppcState.spr[iIndex]));  // Load old value
			CMP(32, R(EAX), gpr.R(inst.RD));
			FixupBranch skip_destroy = J_CC(CC_E, false);
			int gqr = iIndex - SPR_GQR0;
			ABI_CallFunctionC(ProtectFunction(&Jit64::DestroyBlocksWithFlag, 1), (u32)BLOCK_USE_GQR0 << gqr);
			SetJumpTarget(skip_destroy);*/
		}
		// TODO - break block if quantizers are written to.
	default:
		Default(inst);
		return;
	}

	// OK, this is easy.
	ARMReg RD = gpr.R(inst.RD);
	STR(RD, R9,  PPCSTATE_OFF(spr) + iIndex * 4);
}
void JitArm::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	mfspr(inst);
}
void JitArm::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	ARMReg RD = gpr.R(inst.RD);
	switch (iIndex)
	{
	case SPR_WPAR:
	case SPR_DEC:
	case SPR_TL:
	case SPR_TU:
		Default(inst);
		return;
	default:
		LDR(RD, R9, PPCSTATE_OFF(spr) + iIndex * 4);
		break;
	}
}
void JitArm::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
 	// Don't interpret this, if we do we get thrown out
	//JITDISABLE(SystemRegisters)
	
	STR(gpr.R(inst.RS), R9, PPCSTATE_OFF(msr));
	
	gpr.Flush();
	fpr.Flush();

	WriteExit(js.compilerPC + 4, 0);
}
void JitArm::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	
	LDR(gpr.R(inst.RD), R9, PPCSTATE_OFF(msr));
}

