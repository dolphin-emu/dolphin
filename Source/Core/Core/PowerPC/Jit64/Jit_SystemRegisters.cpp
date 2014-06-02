// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

void Jit64::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	int d = inst.RD;

	switch (iIndex)
	{
	case SPR_DMAU:

	case SPR_SPRG0:
	case SPR_SPRG1:
	case SPR_SPRG2:
	case SPR_SPRG3:

	case SPR_SRR0:
	case SPR_SRR1:
		// These are safe to do the easy way, see the bottom of this function.
		break;

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
		// These are safe to do the easy way, see the bottom of this function.
		break;

	default:
		FallBackToInterpreter(inst);
		return;
	}

	// OK, this is easy.
	if (!gpr.R(d).IsImm())
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true, false);
	}
	MOV(32, M(&PowerPC::ppcState.spr[iIndex]), gpr.R(d));
	gpr.UnlockAll();
}

void Jit64::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	int d = inst.RD;
	switch (iIndex)
	{
	case SPR_WPAR:
	case SPR_DEC:
	case SPR_TL:
	case SPR_TU:
	case SPR_PMC1:
	case SPR_PMC2:
	case SPR_PMC3:
	case SPR_PMC4:
		FallBackToInterpreter(inst);
		return;
	default:
		gpr.Lock(d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), M(&PowerPC::ppcState.spr[iIndex]));
		gpr.UnlockAll();
		break;
	}
}

void Jit64::mtmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	// Don't interpret this, if we do we get thrown out
	//JITDISABLE(bJITSystemRegistersOff)
	if (!gpr.R(inst.RS).IsImm())
	{
		gpr.Lock(inst.RS);
		gpr.BindToRegister(inst.RS, true, false);
	}
	MOV(32, M(&MSR), gpr.R(inst.RS));
	gpr.UnlockAll();
	gpr.Flush();
	fpr.Flush();

	// If some exceptions are pending and EE are now enabled, force checking
	// external exceptions when going out of mtmsr in order to execute delayed
	// interrupts as soon as possible.
	TEST(32, M(&MSR), Imm32(0x8000));
	FixupBranch eeDisabled = J_CC(CC_Z);

	TEST(32, M((void*)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER));
	FixupBranch noExceptionsPending = J_CC(CC_Z);

	// Check if a CP interrupt is waiting and keep the GPU emulation in sync (issue 4336)
	TEST(32, M((void *)&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP));
	FixupBranch cpInt = J_CC(CC_NZ);

	MOV(32, M(&PC), Imm32(js.compilerPC + 4));
	WriteExternalExceptionExit();

	SetJumpTarget(cpInt);
	SetJumpTarget(noExceptionsPending);
	SetJumpTarget(eeDisabled);

	WriteExit(js.compilerPC + 4);

	js.firstFPInstructionFound = false;
}

void Jit64::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	//Privileged?
	gpr.Lock(inst.RD);
	gpr.BindToRegister(inst.RD, false, true);
	MOV(32, gpr.R(inst.RD), M(&MSR));
	gpr.UnlockAll();
}

void Jit64::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	mfspr(inst);
}

void Jit64::mfcr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	// USES_CR
	int d = inst.RD;
	gpr.Lock(d);
	gpr.KillImmediate(d, false, true);
	MOV(8, R(EAX), M(&PowerPC::ppcState.cr_fast[0]));

	for (int i = 1; i < 8; i++)
	{
		SHL(32, R(EAX), Imm8(4));
		OR(8, R(EAX), M(&PowerPC::ppcState.cr_fast[i]));
	}

	MOV(32, gpr.R(d), R(EAX));
	gpr.UnlockAll();
}

void Jit64::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

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
			gpr.Lock(inst.RS);
			gpr.BindToRegister(inst.RS, true, false);
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
			gpr.UnlockAll();
		}
	}
}

void Jit64::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)

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
	JITDISABLE(bJITSystemRegistersOff)

	// USES_CR

	// Copy XER[0-3] into CR[inst.CRFD]
	MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
	SHR(32, R(EAX), Imm8(28));
	MOV(8, M(&PowerPC::ppcState.cr_fast[inst.CRFD]), R(EAX));

	// Clear XER[0-3]
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(0x0FFFFFFF));
}

void Jit64::crXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff)
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 19, "Invalid crXXX");

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
	switch (inst.SUBOP10)
	{
	case 33:  // crnor
		OR(8, R(EAX), R(ECX));
		NOT(8, R(EAX));
		break;

	case 129: // crandc
		NOT(8, R(ECX));
		AND(8, R(EAX), R(ECX));
		break;

	case 193: // crxor
		XOR(8, R(EAX), R(ECX));
		break;

	case 225: // crnand
		AND(8, R(EAX), R(ECX));
		NOT(8, R(EAX));
		break;

	case 257: // crand
		AND(8, R(EAX), R(ECX));
		break;

	case 289: // creqv
		XOR(8, R(EAX), R(ECX));
		NOT(8, R(EAX));
		break;

	case 417: // crorc
		NOT(8, R(ECX));
		OR(8, R(EAX), R(ECX));
		break;

	case 449: // cror
		OR(8, R(EAX), R(ECX));
		break;
	}

	// Store result bit in CRBD
	AND(8, R(EAX), Imm8(0x8 >> (inst.CRBD & 3)));
	AND(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), Imm8(~(0x8 >> (inst.CRBD & 3))));
	OR(8, M(&PowerPC::ppcState.cr_fast[inst.CRBD >> 2]), R(EAX));

	gpr.UnlockAllX();
}
