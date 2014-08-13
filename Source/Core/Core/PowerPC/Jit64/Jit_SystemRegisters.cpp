// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

void Jit64::GetCRFieldBit(int field, int bit, Gen::X64Reg out)
{
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		MOV(64, R(ABI_PARAM1), Imm64(1ull << 61));
		TEST(64, M(&PowerPC::ppcState.cr_val[field]), R(ABI_PARAM1));
		SETcc(CC_NZ, R(out));
		break;

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, M(&PowerPC::ppcState.cr_val[field]), Imm32(0));
		SETcc(CC_Z, R(out));
		break;

	case CR_GT_BIT:  // check val > 0
		MOV(64, R(ABI_PARAM1), M(&PowerPC::ppcState.cr_val[field]));
		TEST(64, R(ABI_PARAM1), R(ABI_PARAM1));
		SETcc(CC_G, R(out));
		break;

	case CR_LT_BIT:  // check bit 62 set
		MOV(64, R(ABI_PARAM1), Imm64(1ull << 62));
		TEST(64, M(&PowerPC::ppcState.cr_val[field]), R(ABI_PARAM1));
		SETcc(CC_NZ, R(out));
		break;

	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}
}

void Jit64::SetCRFieldBit(int field, int bit, Gen::X64Reg in)
{
	MOV(64, R(ABI_PARAM2), M(&PowerPC::ppcState.cr_val[field]));
	TEST(8, R(in), Imm8(1));
	FixupBranch input_is_set = J_CC(CC_NZ, false);

	// New value is 0.
	switch (bit)
	{
	case CR_SO_BIT:  // unset bit 61
		MOV(64, R(ABI_PARAM1), Imm64(~(1ull << 61)));
		AND(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;

	case CR_EQ_BIT:  // set bit 0 to 1
		OR(8, R(ABI_PARAM2), Imm8(1));
		break;

	case CR_GT_BIT:  // !GT, set bit 63
		MOV(64, R(ABI_PARAM1), Imm64(1ull << 63));
		OR(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;

	case CR_LT_BIT:  // !LT, unset bit 62
		MOV(64, R(ABI_PARAM1), Imm64(~(1ull << 62)));
		AND(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;
	}

	FixupBranch end = J();
	SetJumpTarget(input_is_set);

	switch (bit)
	{
	case CR_SO_BIT:  // set bit 61
		MOV(64, R(ABI_PARAM1), Imm64(1ull << 61));
		OR(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;

	case CR_EQ_BIT:  // set bits 31-0 to 0
		MOV(64, R(ABI_PARAM1), Imm64(0xFFFFFFFF00000000));
		AND(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;

	case CR_GT_BIT:  // unset bit 63
		MOV(64, R(ABI_PARAM1), Imm64(~(1ull << 63)));
		AND(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;

	case CR_LT_BIT:  // set bit 62
		MOV(64, R(ABI_PARAM1), Imm64(1ull << 62));
		OR(64, R(ABI_PARAM2), R(ABI_PARAM1));
		break;
	}

	SetJumpTarget(end);
	MOV(64, R(ABI_PARAM1), Imm64(1ull << 32));
	OR(64, R(ABI_PARAM2), R(ABI_PARAM1));
	MOV(64, M(&PowerPC::ppcState.cr_val[field]), R(ABI_PARAM2));
}

FixupBranch Jit64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		MOV(64, R(RAX), Imm64(1ull << 61));
		TEST(64, M(&PowerPC::ppcState.cr_val[field]), R(RAX));
		return J_CC(jump_if_set ? CC_NZ : CC_Z, true);

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, M(&PowerPC::ppcState.cr_val[field]), Imm32(0));
		return J_CC(jump_if_set ? CC_Z : CC_NZ, true);

	case CR_GT_BIT:  // check val > 0
		MOV(64, R(RAX), M(&PowerPC::ppcState.cr_val[field]));
		TEST(64, R(RAX), R(RAX));
		return J_CC(jump_if_set ? CC_G : CC_LE, true);

	case CR_LT_BIT:  // check bit 62 set
		MOV(64, R(RAX), Imm64(1ull << 62));
		TEST(64, M(&PowerPC::ppcState.cr_val[field]), R(RAX));
		return J_CC(jump_if_set ? CC_NZ : CC_Z, true);

	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}

	// Should never happen.
	return FixupBranch();
}

void Jit64::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
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
		FALLBACK_IF(true);
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
	JITDISABLE(bJITSystemRegistersOff);
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
		FALLBACK_IF(true);
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
	//JITDISABLE(bJITSystemRegistersOff);
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
	JITDISABLE(bJITSystemRegistersOff);
	//Privileged?
	gpr.Lock(inst.RD);
	gpr.BindToRegister(inst.RD, false, true);
	MOV(32, gpr.R(inst.RD), M(&MSR));
	gpr.UnlockAll();
}

void Jit64::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	mfspr(inst);
}

void Jit64::mfcr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	// USES_CR
	int d = inst.RD;
	gpr.Lock(d);
	gpr.KillImmediate(d, false, true);
	XOR(32, R(EAX), R(EAX));

	gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
	X64Reg cr_val = ABI_PARAM1;
	X64Reg tmp = ABI_PARAM2;
	for (int i = 0; i < 8; i++)
	{
		if (i != 0)
			SHL(32, R(EAX), Imm8(4));

		MOV(64, R(cr_val), M(&PowerPC::ppcState.cr_val[i]));

		// SO: Bit 61 set.
		MOV(64, R(tmp), R(cr_val));
		SHR(64, R(tmp), Imm8(61));
		AND(32, R(tmp), Imm8(1));
		OR(32, R(EAX), R(tmp));

		// EQ: Bits 31-0 == 0.
		XOR(32, R(tmp), R(tmp));
		TEST(32, R(cr_val), R(cr_val));
		SETcc(CC_Z, R(tmp));
		SHL(32, R(tmp), Imm8(1));
		OR(32, R(EAX), R(tmp));

		// GT: Value > 0.
		TEST(64, R(cr_val), R(cr_val));
		SETcc(CC_G, R(tmp));
		SHL(32, R(tmp), Imm8(2));
		OR(32, R(EAX), R(tmp));

		// LT: Bit 62 set.
		MOV(64, R(tmp), R(cr_val));
		SHR(64, R(tmp), Imm8(62 - 3));
		AND(32, R(tmp), Imm8(0x8));
		OR(32, R(EAX), R(tmp));
	}

	MOV(32, gpr.R(d), R(EAX));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

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
					MOV(64, R(RAX), Imm64(PPCCRToInternal(newcr)));
					MOV(64, M(&PowerPC::ppcState.cr_val[i]), R(RAX));
				}
			}
		}
		else
		{
			gpr.Lock(inst.RS);
			gpr.BindToRegister(inst.RS, true, false);
			gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
			for (int i = 0; i < 8; i++)
			{
				if ((crm & (0x80 >> i)) != 0)
				{
					MOVZX(64, 32, EAX, gpr.R(inst.RS));
					SHR(64, R(EAX), Imm8(28 - (i * 4)));
					AND(64, R(EAX), Imm32(0xF));

					X64Reg cr_val = ABI_PARAM1;
					X64Reg tmp = ABI_PARAM2;

					MOV(64, R(cr_val), Imm64(1ull << 32));

					// SO
					MOV(64, R(tmp), R(EAX));
					SHL(64, R(tmp), Imm8(63));
					SHR(64, R(tmp), Imm8(63 - 61));
					OR(64, R(cr_val), R(tmp));

					// EQ
					MOV(64, R(tmp), R(EAX));
					NOT(64, R(tmp));
					AND(64, R(tmp), Imm8(CR_EQ));
					OR(64, R(cr_val), R(tmp));

					// GT
					MOV(64, R(tmp), R(EAX));
					NOT(64, R(tmp));
					AND(64, R(tmp), Imm8(CR_GT));
					SHL(64, R(tmp), Imm8(63 - 2));
					OR(64, R(cr_val), R(tmp));

					// LT
					MOV(64, R(tmp), R(EAX));
					AND(64, R(tmp), Imm8(CR_LT));
					SHL(64, R(tmp), Imm8(62 - 3));
					OR(64, R(cr_val), R(tmp));

					MOV(64, M(&PowerPC::ppcState.cr_val[i]), R(cr_val));
				}
			}
			gpr.UnlockAll();
			gpr.UnlockAllX();
		}
	}
}

void Jit64::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	// USES_CR
	if (inst.CRFS != inst.CRFD)
	{
		MOV(64, R(EAX), M(&PowerPC::ppcState.cr_val[inst.CRFS]));
		MOV(64, M(&PowerPC::ppcState.cr_val[inst.CRFD]), R(EAX));
	}
}

void Jit64::mcrxr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	// USES_CR

	// Copy XER[0-3] into CR[inst.CRFD]
	MOVZX(64, 32, EAX, M(&PowerPC::ppcState.spr[SPR_XER]));
	SHR(64, R(EAX), Imm8(28));

	gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
	X64Reg cr_val = ABI_PARAM1;
	X64Reg tmp = ABI_PARAM2;

	MOV(64, R(cr_val), Imm64(1ull << 32));

	// SO
	MOV(64, R(tmp), R(EAX));
	SHL(64, R(tmp), Imm8(63));
	SHR(64, R(tmp), Imm8(63 - 61));
	OR(64, R(cr_val), R(tmp));

	// EQ
	MOV(64, R(tmp), R(EAX));
	AND(64, R(tmp), Imm8(0x2));
	OR(64, R(cr_val), R(tmp));

	// GT
	MOV(64, R(tmp), R(EAX));
	NOT(64, R(tmp));
	AND(64, R(tmp), Imm8(0x4));
	SHL(64, R(tmp), Imm8(63 - 2));
	OR(64, R(cr_val), R(tmp));

	// LT
	MOV(64, R(tmp), R(EAX));
	AND(64, R(tmp), Imm8(0x8));
	SHL(64, R(tmp), Imm8(62 - 3));
	OR(64, R(cr_val), R(tmp));

	MOV(64, M(&PowerPC::ppcState.cr_val[inst.CRFD]), R(cr_val));
	gpr.UnlockAllX();

	// Clear XER[0-3]
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(0x0FFFFFFF));
}

void Jit64::crXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 19, "Invalid crXXX");

	// TODO(delroth): Potential optimizations could be applied here. For
	// instance, if the two CR bits being loaded are the same, two loads are
	// not required.

	// USES_CR

	gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
	GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), ABI_PARAM2);
	GetCRFieldBit(inst.CRBB >> 2, 3 - (inst.CRBB & 3), EAX);

	// Compute combined bit
	switch (inst.SUBOP10)
	{
	case 33:  // crnor
		OR(8, R(EAX), R(ABI_PARAM2));
		NOT(8, R(EAX));
		break;

	case 129: // crandc
		NOT(8, R(ABI_PARAM2));
		AND(8, R(EAX), R(ABI_PARAM2));
		break;

	case 193: // crxor
		XOR(8, R(EAX), R(ABI_PARAM2));
		break;

	case 225: // crnand
		AND(8, R(EAX), R(ABI_PARAM2));
		NOT(8, R(EAX));
		break;

	case 257: // crand
		AND(8, R(EAX), R(ABI_PARAM2));
		break;

	case 289: // creqv
		XOR(8, R(EAX), R(ABI_PARAM2));
		NOT(8, R(EAX));
		break;

	case 417: // crorc
		NOT(8, R(ABI_PARAM2));
		OR(8, R(EAX), R(ABI_PARAM2));
		break;

	case 449: // cror
		OR(8, R(EAX), R(ABI_PARAM2));
		break;
	}

	// Store result bit in CRBD
	SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), EAX);

	gpr.UnlockAllX();
}
