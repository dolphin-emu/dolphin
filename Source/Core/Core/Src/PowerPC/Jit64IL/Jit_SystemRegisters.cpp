// Copyright (C) 2003-2009 Dolphin Project.

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

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

	void Jit64::mtspr(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(SystemRegisters)
		u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
		switch(iIndex) {
		case SPR_LR:
			ibuild.EmitStoreLink(ibuild.EmitLoadGReg(inst.RD));
			return;
		case SPR_CTR:
			ibuild.EmitStoreCTR(ibuild.EmitLoadGReg(inst.RD));
			return;
		case SPR_GQR0:
		case SPR_GQR0 + 1:
		case SPR_GQR0 + 2:
		case SPR_GQR0 + 3:
		case SPR_GQR0 + 4:
		case SPR_GQR0 + 5:
		case SPR_GQR0 + 6:
		case SPR_GQR0 + 7:
			ibuild.EmitStoreGQR(ibuild.EmitLoadGReg(inst.RD), iIndex - SPR_GQR0);
			return;
		case SPR_SRR0:
		case SPR_SRR1:
			ibuild.EmitStoreSRR(ibuild.EmitLoadGReg(inst.RD), iIndex - SPR_SRR0);
			return;
		default:
			Default(inst);
			return;
		}
	}

	void Jit64::mfspr(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(SystemRegisters)
		u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
		switch (iIndex)
		{
		case SPR_LR:
			ibuild.EmitStoreGReg(ibuild.EmitLoadLink(), inst.RD);
			return;
		case SPR_CTR:
			ibuild.EmitStoreGReg(ibuild.EmitLoadCTR(), inst.RD);
			return;
		case SPR_GQR0:
		case SPR_GQR0 + 1:
		case SPR_GQR0 + 2:
		case SPR_GQR0 + 3:
		case SPR_GQR0 + 4:
		case SPR_GQR0 + 5:
		case SPR_GQR0 + 6:
		case SPR_GQR0 + 7:
			ibuild.EmitStoreGReg(ibuild.EmitLoadGQR(iIndex - SPR_GQR0), inst.RD);
			return;
		default:
			Default(inst);
			return;
		}
	}


	// =======================================================================================
	// Don't interpret this, if we do we get thrown out
	// --------------
	void Jit64::mtmsr(UGeckoInstruction inst)
	{
		ibuild.EmitStoreMSR(ibuild.EmitLoadGReg(inst.RS));
		ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
	}
	// ==============


	void Jit64::mfmsr(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(SystemRegisters)
		ibuild.EmitStoreGReg(ibuild.EmitLoadMSR(), inst.RD);
	}

	void Jit64::mftb(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		JITDISABLE(SystemRegisters)
		mfspr(inst);
	}

	void Jit64::mfcr(UGeckoInstruction inst)
	{
		Default(inst); return;
#if 0
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;
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
#endif
	}

	void Jit64::mtcrf(UGeckoInstruction inst)
	{
		Default(inst); return;
#if 0
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITSystemRegistersOff)
			{Default(inst); return;} // turn off from debugger
		INSTRUCTION_START;

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
#endif
	}
