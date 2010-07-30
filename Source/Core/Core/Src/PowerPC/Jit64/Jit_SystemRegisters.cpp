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
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[0]));
	for (int i = 1; i < 8; i++) {
		SHL(32, R(EAX), Imm8(4));
		OR(8, R(EAX), M(&PowerPC::ppcState.cr_fast[i]));
	}
	MOV(32, gpr.R(d), R(EAX));
}

void Jit64::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR
	u32 crm = inst.CRM;
	if (crm != 0)
	{
		if (gpr.R(inst.RS).IsImm())
		{
			for (int i = 0; i < 8; i++)
			{
				if ((crm & (0x80 >> i)) != 0)
				{
					u8 newcr = (gpr.R(inst.RS).offset >> (28 - (i * 4))) & 0xF;
					MOV(8, M(&PowerPC::ppcState.cr_fast[i]), Imm8(newcr));
				}
			}
		}
		else
		{
			gpr.LoadToX64(inst.RS, true);
			for (int i = 0; i < 8; i++)
			{
				if ((crm & (0x80 >> i)) != 0)
				{
					MOV(32, R(EAX), gpr.R(inst.RS));
					SHR(32, R(EAX), Imm8(28 - (i * 4)));
					AND(32, R(EAX), Imm32(0xF));
					MOV(8, M(&PowerPC::ppcState.cr_fast[i]), R(EAX));
				}
			}
		}
	}
}

void Jit64::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR
	if (inst.CRFS != inst.CRFD)
	{
		MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRFS]));
		MOV(8, M(&PowerPC::ppcState.cr_fast[inst.CRFD]), R(EAX));
	}
}

void Jit64::mcrxr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR

	// Copy XER[0-3] into CR[inst.CRFD]
	MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
	SHR(32, R(EAX), Imm8(28));
	MOV(8, M(&PowerPC::ppcState.cr_fast[inst.CRFD]), R(EAX));

	// Clear XER[0-3]
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(0x0FFFFFFF));
}

void Jit64::crand(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	AND(8, R(EAX), R(ECX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::crandc(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	NOT(8, R(ECX));
	AND(8, R(EAX), R(ECX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::creqv(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	XOR(8, R(EAX), R(ECX));
	NOT(8, R(EAX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::crnand(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	AND(8, R(EAX), R(ECX));
	NOT(8, R(EAX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::crnor(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	OR(8, R(EAX), R(ECX));
	NOT(8, R(EAX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::cror(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	OR(8, R(EAX), R(ECX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::crorc(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	NOT(8, R(ECX));
	OR(8, R(EAX), R(ECX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}

void Jit64::crxor(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(SystemRegisters)

	// USES_CR 

	// Get bit CRBA in EAX aligned with bit CRBD
	int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[inst.CRBA >> 2]));
	if (shiftA < 0)
		SHL(8, R(EAX), Imm8(-shiftA));
	else if (shiftA > 0)
		SHR(8, R(EAX), Imm8(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	gpr.FlushLockX(ECX);
	int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	MOV(8, R(ECX), M(&PowerPC::ppcState.cr_fast[inst.CRBB >> 2]));
	if (shiftB < 0)
		SHL(8, R(ECX), Imm8(-shiftB));
	else if (shiftB > 0)
		SHR(8, R(ECX), Imm8(shiftB));

	// Compute combined bit
	XOR(8, R(EAX), R(ECX));

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}
