// Copyright (C) 2003-2008 Dolphin Project.

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
#include "JitCache.h"
#include "JitRegCache.h"

#define INSTRUCTION_START
// #define INSTRUCTION_START Default(inst); return;

namespace Jit64
{
	void mtspr(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
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
			js.blockSetsQuantizers = true;
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

	void mfspr(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
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
			//CALL((void *)&CoreTiming::Advance);
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
	void mtmsr(UGeckoInstruction inst)
	{
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
		//	{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
		gpr.LoadToX64(inst.RS, true, false);
		MOV(32, M(&MSR), gpr.R(inst.RS));
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		WriteExit(js.compilerPC + 4, 0);
	}
	// ==============


	void mfmsr(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		//Privileged?
		gpr.LoadToX64(inst.RD, false);
		MOV(32, gpr.R(inst.RD), M(&MSR));
	}

	void mftb(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		mfspr(inst);
	}

	void mfcr(UGeckoInstruction inst)
	{
		// USES_CR
		int d = inst.RD;
		gpr.LoadToX64(d, false, true);
		MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[0]));
		SHL(32, R(EAX), Imm8(4));
		for (int i = 1; i < 7; i++) {
			OR(8, R(EAX), M(&PowerPC::ppcState.cr_fast[i]));
			SHL(32, R(EAX), Imm8(4));
		}
		OR(8, R(EAX), M(&PowerPC::ppcState.cr_fast[7]));
		MOV(32, gpr.R(d), R(EAX));
	}

	void mtcrf(UGeckoInstruction inst)
	{
		//Default(inst);
		//return;

		// USES_CR
		u32 mask = 0;
		u32 crm = inst.CRM;
		if (crm == 0xFF) {
			gpr.FlushLockX(ECX);			
			MOV(32, R(EAX), gpr.R(inst.RS));
			for (int i = 0; i < 8; i++) {
				MOV(32, R(ECX), R(EAX));
				SHR(32, R(ECX), Imm8(28 - (i * 4)));
				AND(32, R(ECX), Imm32(0xF));
				MOV(8, M(&PowerPC::ppcState.cr_fast[i]), R(ECX));
			}
			gpr.UnlockAllX();
		} else {
			Default(inst);
			return;

			// TODO: translate this to work in new CR model.
			for (int i = 0; i < 8; i++) {
				if (crm & (1 << i))
					mask |= 0xF << (i*4);
			}
			MOV(32, R(EAX), gpr.R(inst.RS));
			MOV(32, R(ECX), M(&PowerPC::ppcState.cr));
			AND(32, R(EAX), Imm32(mask));
			AND(32, R(ECX), Imm32(~mask));
			OR(32, R(EAX), R(ECX));
			MOV(32, M(&PowerPC::ppcState.cr), R(EAX));
		}
	}

}  // namespace
