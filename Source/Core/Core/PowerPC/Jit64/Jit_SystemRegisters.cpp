// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

void Jit64::GetCRFieldBit(int field, int bit, Gen::X64Reg out, bool negate)
{
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		BT(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(61));
		SETcc(negate ? CC_NC : CC_C, R(out));
		break;

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, M(&PowerPC::ppcState.cr_val[field]), Imm8(0));
		SETcc(negate ? CC_NZ : CC_Z, R(out));
		break;

	case CR_GT_BIT:  // check val > 0
		CMP(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(0));
		SETcc(negate ? CC_NG : CC_G, R(out));
		break;

	case CR_LT_BIT:  // check bit 62 set
		BT(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(62));
		SETcc(negate ? CC_NC : CC_C, R(out));
		break;

	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}
}

void Jit64::SetCRFieldBit(int field, int bit, Gen::X64Reg in)
{
	MOV(64, R(ABI_PARAM1), M(&PowerPC::ppcState.cr_val[field]));
	MOVZX(32, 8, in, R(in));

	switch (bit)
	{
	case CR_SO_BIT:  // set bit 61 to input
		BTR(64, R(ABI_PARAM1), Imm8(61));
		SHL(64, R(in), Imm8(61));
		OR(64, R(ABI_PARAM1), R(in));
		break;

	case CR_EQ_BIT:  // clear low 32 bits, set bit 0 to !input
		SHR(64, R(ABI_PARAM1), Imm8(32));
		SHL(64, R(ABI_PARAM1), Imm8(32));
		XOR(32, R(in), Imm8(1));
		OR(64, R(ABI_PARAM1), R(in));
		break;

	case CR_GT_BIT:  // set bit 63 to !input
		BTR(64, R(ABI_PARAM1), Imm8(63));
		NOT(32, R(in));
		SHL(64, R(in), Imm8(63));
		OR(64, R(ABI_PARAM1), R(in));
		break;

	case CR_LT_BIT:  // set bit 62 to input
		BTR(64, R(ABI_PARAM1), Imm8(62));
		SHL(64, R(in), Imm8(62));
		OR(64, R(ABI_PARAM1), R(in));
		break;
	}

	BTS(64, R(ABI_PARAM1), Imm8(32));
	MOV(64, M(&PowerPC::ppcState.cr_val[field]), R(ABI_PARAM1));
}

FixupBranch Jit64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		BT(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(61));
		return J_CC(jump_if_set ? CC_C : CC_NC, true);

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, M(&PowerPC::ppcState.cr_val[field]), Imm8(0));
		return J_CC(jump_if_set ? CC_Z : CC_NZ, true);

	case CR_GT_BIT:  // check val > 0
		CMP(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(0));
		return J_CC(jump_if_set ? CC_G : CC_LE, true);

	case CR_LT_BIT:  // check bit 62 set
		BT(64, M(&PowerPC::ppcState.cr_val[field]), Imm8(62));
		return J_CC(jump_if_set ? CC_C : CC_NC, true);

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
	gpr.BindToRegister(d, false, true);
	XOR(32, gpr.R(d), gpr.R(d));

	gpr.FlushLockX(ABI_PARAM1);
	X64Reg cr_val = ABI_PARAM1;
	// we only need to zero the high bits of EAX once
	XOR(32, R(EAX), R(EAX));
	for (int i = 0; i < 8; i++)
	{
		static const u8 m_flagTable[8] = {0x0,0x1,0x8,0x9,0x0,0x1,0x8,0x9};
		if (i != 0)
			SHL(32, gpr.R(d), Imm8(4));

		MOV(64, R(cr_val), M(&PowerPC::ppcState.cr_val[i]));

		// EQ: Bits 31-0 == 0; set flag bit 1
		TEST(32, R(cr_val), R(cr_val));
		SETcc(CC_Z, R(EAX));
		LEA(32, gpr.RX(d), MComplex(gpr.RX(d), EAX, SCALE_2, 0));

		// GT: Value > 0; set flag bit 2
		TEST(64, R(cr_val), R(cr_val));
		SETcc(CC_G, R(EAX));
		LEA(32, gpr.RX(d), MComplex(gpr.RX(d), EAX, SCALE_4, 0));

		// SO: Bit 61 set; set flag bit 0
		// LT: Bit 62 set; set flag bit 3
		SHR(64, R(cr_val), Imm8(61));
		MOVZX(32, 8, EAX, MDisp(cr_val, (u32)(u64)m_flagTable));
		OR(32, gpr.R(d), R(EAX));
	}

	gpr.UnlockAll();
	gpr.UnlockAllX();
}

// convert flags into 64-bit CR values with a lookup table
static const u64 m_crTable[16] =
{
	PPCCRToInternal(0x0), PPCCRToInternal(0x1), PPCCRToInternal(0x2), PPCCRToInternal(0x3),
	PPCCRToInternal(0x4), PPCCRToInternal(0x5), PPCCRToInternal(0x6), PPCCRToInternal(0x7),
	PPCCRToInternal(0x8), PPCCRToInternal(0x9), PPCCRToInternal(0xA), PPCCRToInternal(0xB),
	PPCCRToInternal(0xC), PPCCRToInternal(0xD), PPCCRToInternal(0xE), PPCCRToInternal(0xF),
};

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
					u64 newcrval = PPCCRToInternal(newcr);
					if ((s64)newcrval == (s32)newcrval)
					{
						MOV(64, M(&PowerPC::ppcState.cr_val[i]), Imm32((s32)newcrval));
					}
					else
					{
						MOV(64, R(RAX), Imm64(newcrval));
						MOV(64, M(&PowerPC::ppcState.cr_val[i]), R(RAX));
					}
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
					if (i != 7)
						SHR(32, R(EAX), Imm8(28 - (i * 4)));
					if (i != 0)
						AND(32, R(EAX), Imm8(0xF));
					MOV(64, R(EAX), MScaled(EAX, SCALE_8, (u32)(u64)m_crTable));
					MOV(64, M(&PowerPC::ppcState.cr_val[i]), R(EAX));
				}
			}
			gpr.UnlockAll();
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
	MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
	SHR(32, R(EAX), Imm8(28));

	MOV(64, R(EAX), MScaled(EAX, SCALE_8, (u32)(u64)m_crTable));
	MOV(64, M(&PowerPC::ppcState.cr_val[inst.CRFD]), R(EAX));

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
	// crandc or crorc or creqv or crnand or crnor
	bool negateA = inst.SUBOP10 == 129 || inst.SUBOP10 == 417 || inst.SUBOP10 == 289 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;
	// crnand or crnor
	bool negateB = inst.SUBOP10 == 225 || inst.SUBOP10 == 33;

	gpr.FlushLockX(ABI_PARAM1);
	GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), ABI_PARAM1, negateA);
	GetCRFieldBit(inst.CRBB >> 2, 3 - (inst.CRBB & 3), EAX, negateB);

	// Compute combined bit
	switch (inst.SUBOP10)
	{
	case 33:  // crnor: ~(A || B) == (~A && ~B)
	case 129: // crandc
	case 257: // crand
		AND(8, R(EAX), R(ABI_PARAM1));
		break;

	case 193: // crxor
	case 289: // creqv
		XOR(8, R(EAX), R(ABI_PARAM1));
		break;

	case 225: // crnand: ~(A && B) == (~A || ~B)
	case 417: // crorc
	case 449: // cror
		OR(8, R(EAX), R(ABI_PARAM1));
		break;
	}

	// Store result bit in CRBD
	SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), EAX);

	gpr.UnlockAllX();
}
