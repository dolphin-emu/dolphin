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

#include "../../Core.h"
#include "../../CoreTiming.h"
#include "../../HW/SystemTimers.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "Thunk.h"

#include "Jit.h"
#include "JitRegCache.h"

void Jit64::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	int d = inst.RD;

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
		break;
		// TODO - break block if quantizers are written to.
	default:
		Default(inst);
		return;
	}

	// OK, this is easy.
	gpr.Lock(d);
	gpr.LoadToX64(d, true);
	MOV(32, M(&PowerPC::ppcState.spr[iIndex]), gpr.R(d));
	gpr.UnlockAll();
}

void Jit64::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	int d = inst.RD;
	switch (iIndex)
	{
	case SPR_WPAR:
		Default(inst);
		return;
		//		case SPR_DEC:
		//MessageBox(NULL, "Read from DEC", "????", MB_OK);
		//break;
	case SPR_TL:
	case SPR_TU:
		//CALL((void Jit64::*)&CoreTiming::Advance);
		// fall through
	default:
		gpr.Lock(d);
		gpr.LoadToX64(d, false);
		MOV(32, gpr.R(d), M(&PowerPC::ppcState.spr[iIndex]));
		gpr.UnlockAll();
		break;
	}
}


// =======================================================================================
// Don't interpret this, if we do we get thrown out
// --------------
void Jit64::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	gpr.LoadToX64(inst.RS, true, false);
	MOV(32, M(&MSR), gpr.R(inst.RS));
	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	WriteExit(js.compilerPC + 4, 0);
}
// ==============


void Jit64::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	//Privileged?
	gpr.LoadToX64(inst.RD, false);
	MOV(32, gpr.R(inst.RD), M(&MSR));
}

void Jit64::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	mfspr(inst);
}

void Jit64::mfcr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)
	// USES_CR
	int d = inst.RD;
	gpr.LoadToX64(d, false, true);
	MOV(32, R(EAX), M(&PowerPC::ppcState.cr_fast_u32));
	BSWAP(32, EAX);
	MOV(32, gpr.R(d), R(EAX));
}

void Jit64::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	u32 crm = inst.CRM;
	if (crm == 0xFF) {
		MOV(32, R(EAX), gpr.R(inst.RS));
		BSWAP(32, EAX);
		MOV(32, M(&PowerPC::ppcState.cr_fast_u32), R(EAX));
	}
	else if (crm != 0) {
		u32 mask = 0;
		for (int i = 0; i < 8; i++) {
			if (crm & (1 << i))
				mask |= 0xF << (i*4);
		}

		MOV(32, R(EAX), gpr.R(inst.RS));
		AND(32, R(EAX), Imm32(mask));
		BSWAP(32, EAX);
		MOV(32, M(&PowerPC::ppcState.cr_fast_u32), R(EAX));
	}
}
