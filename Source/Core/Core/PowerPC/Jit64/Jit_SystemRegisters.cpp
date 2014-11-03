// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

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
		BT(64, PPCSTATE(cr_val[field]), Imm8(61));
		SETcc(negate ? CC_NC : CC_C, R(out));
		break;

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, PPCSTATE(cr_val[field]), Imm8(0));
		SETcc(negate ? CC_NZ : CC_Z, R(out));
		break;

	case CR_GT_BIT:  // check val > 0
		CMP(64, PPCSTATE(cr_val[field]), Imm8(0));
		SETcc(negate ? CC_NG : CC_G, R(out));
		break;

	case CR_LT_BIT:  // check bit 62 set
		BT(64, PPCSTATE(cr_val[field]), Imm8(62));
		SETcc(negate ? CC_NC : CC_C, R(out));
		break;

	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
	}
}

void Jit64::SetCRFieldBit(int field, int bit, Gen::X64Reg in)
{
	MOV(64, R(RSCRATCH2), PPCSTATE(cr_val[field]));
	MOVZX(32, 8, in, R(in));

	// Gross but necessary; if the input is totally zero and we set SO or LT,
	// or even just add the (1<<32), GT will suddenly end up set without us
	// intending to. This can break actual games, so fix it up.
	if (bit != CR_GT_BIT)
	{
		TEST(64, R(RSCRATCH2), R(RSCRATCH2));
		FixupBranch dont_clear_gt = J_CC(CC_NZ);
		BTS(64, R(RSCRATCH2), Imm8(63));
		SetJumpTarget(dont_clear_gt);
	}

	switch (bit)
	{
	case CR_SO_BIT:  // set bit 61 to input
		BTR(64, R(RSCRATCH2), Imm8(61));
		SHL(64, R(in), Imm8(61));
		OR(64, R(RSCRATCH2), R(in));
		break;

	case CR_EQ_BIT:  // clear low 32 bits, set bit 0 to !input
		SHR(64, R(RSCRATCH2), Imm8(32));
		SHL(64, R(RSCRATCH2), Imm8(32));
		XOR(32, R(in), Imm8(1));
		OR(64, R(RSCRATCH2), R(in));
		break;

	case CR_GT_BIT:  // set bit 63 to !input
		BTR(64, R(RSCRATCH2), Imm8(63));
		NOT(32, R(in));
		SHL(64, R(in), Imm8(63));
		OR(64, R(RSCRATCH2), R(in));
		break;

	case CR_LT_BIT:  // set bit 62 to input
		BTR(64, R(RSCRATCH2), Imm8(62));
		SHL(64, R(in), Imm8(62));
		OR(64, R(RSCRATCH2), R(in));
		break;
	}

	BTS(64, R(RSCRATCH2), Imm8(32));
	MOV(64, PPCSTATE(cr_val[field]), R(RSCRATCH2));
}

void Jit64::ClearCRFieldBit(int field, int bit)
{
	switch (bit)
	{
	case CR_SO_BIT:
		BTR(64, PPCSTATE(cr_val[field]), Imm8(61));
		break;

	case CR_EQ_BIT:
		OR(64, PPCSTATE(cr_val[field]), Imm8(1));
		break;

	case CR_GT_BIT:
		BTS(64, PPCSTATE(cr_val[field]), Imm8(63));
		break;

	case CR_LT_BIT:
		BTR(64, PPCSTATE(cr_val[field]), Imm8(62));
		break;
	}
	// We don't need to set bit 32; the cases where that's needed only come up when setting bits, not clearing.
}

FixupBranch Jit64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		BT(64, PPCSTATE(cr_val[field]), Imm8(61));
		return J_CC(jump_if_set ? CC_C : CC_NC, true);

	case CR_EQ_BIT:  // check bits 31-0 == 0
		CMP(32, PPCSTATE(cr_val[field]), Imm8(0));
		return J_CC(jump_if_set ? CC_Z : CC_NZ, true);

	case CR_GT_BIT:  // check val > 0
		CMP(64, PPCSTATE(cr_val[field]), Imm8(0));
		return J_CC(jump_if_set ? CC_G : CC_LE, true);

	case CR_LT_BIT:  // check bit 62 set
		BT(64, PPCSTATE(cr_val[field]), Imm8(62));
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

	case SPR_XER:
		gpr.Lock(d);
		gpr.BindToRegister(d, true, false);
		MOV(32, R(RSCRATCH), gpr.R(d));
		AND(32, R(RSCRATCH), Imm32(0xff7f));
		MOV(16, PPCSTATE(xer_stringctrl), R(RSCRATCH));

		MOV(32, R(RSCRATCH), gpr.R(d));
		SHR(32, R(RSCRATCH), Imm8(XER_CA_SHIFT));
		AND(8, R(RSCRATCH), Imm8(1));
		MOV(8, PPCSTATE(xer_ca), R(RSCRATCH));

		MOV(32, R(RSCRATCH), gpr.R(d));
		SHR(32, R(RSCRATCH), Imm8(XER_OV_SHIFT));
		MOV(8, PPCSTATE(xer_so_ov), R(RSCRATCH));
		gpr.UnlockAll();
		return;

	default:
		FALLBACK_IF(true);
	}

	// OK, this is easy.
	if (!gpr.R(d).IsImm())
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true, false);
	}
	MOV(32, PPCSTATE(spr[iIndex]), gpr.R(d));
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
	case SPR_TL:
	case SPR_TU:
	{
		// TODO: we really only need to call GetFakeTimeBase once per JIT block; this matters because
		// typical use of this instruction is to call it three times, e.g. mftbu/mftbl/mftbu/cmpw/bne
		// to deal with possible timer wraparound. This makes the second two (out of three) completely
		// redundant for the JIT.
		// no register choice

		gpr.FlushLockX(RDX, RAX);

		// An inline implementation of CoreTiming::GetFakeTimeBase, since in timer-heavy games the
		// cost of calling out to C for this is actually significant.
		MOV(64, R(RAX), M(&CoreTiming::globalTimer));
		SUB(64, R(RAX), M(&CoreTiming::fakeTBStartTicks));
		// The timer can change within a long block, so add in any difference
		if (js.downcountAmount)
			ADD(64, R(RAX), Imm32(js.downcountAmount));
		// a / 12 = (a * 0xAAAAAAAAAAAAAAAB) >> 67
		MOV(64, R(RDX), Imm64(0xAAAAAAAAAAAAAAABULL));
		MUL(64, R(RDX));
		MOV(64, R(RAX), M(&CoreTiming::fakeTBStartValue));
		SHR(64, R(RDX), Imm8(3));
		ADD(64, R(RAX), R(RDX));
		MOV(64, PPCSTATE(spr[SPR_TL]), R(RAX));

		// Two calls of TU/TL next to each other are extremely common in typical usage, so merge them
		// if we can.
		u32 nextIndex = (js.next_inst.SPRU << 5) | (js.next_inst.SPRL & 0x1F);
		// Be careful; the actual opcode is for mftb (371), not mfspr (339)
		int n = js.next_inst.RD;
		if (js.next_inst.OPCD == 31 && js.next_inst.SUBOP10 == 371 && (nextIndex == SPR_TU || nextIndex == SPR_TL) &&
			PowerPC::GetState() != PowerPC::CPU_STEPPING && n != d)
		{
			js.downcountAmount++;
			js.skipnext = true;
			gpr.Lock(d, n);
			gpr.BindToRegister(d, false);
			gpr.BindToRegister(n, false);
			if (iIndex == SPR_TL)
				MOV(32, gpr.R(d), R(RAX));
			if (nextIndex == SPR_TL)
				MOV(32, gpr.R(n), R(RAX));
			SHR(64, R(RAX), Imm8(32));
			if (iIndex == SPR_TU)
				MOV(32, gpr.R(d), R(RAX));
			if (nextIndex == SPR_TU)
				MOV(32, gpr.R(n), R(RAX));
		}
		else
		{
			gpr.Lock(d);
			gpr.BindToRegister(d, false);
			if (iIndex == SPR_TU)
				SHR(64, R(RAX), Imm8(32));
			MOV(32, gpr.R(d), R(RAX));
		}
		gpr.UnlockAllX();
		break;
	}
	case SPR_XER:
		gpr.Lock(d);
		gpr.BindToRegister(d, false);
		MOVZX(32, 16, gpr.RX(d), PPCSTATE(xer_stringctrl));
		MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_ca));
		SHL(32, R(RSCRATCH), Imm8(XER_CA_SHIFT));
		OR(32, gpr.R(d), R(RSCRATCH));

		MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_so_ov));
		SHL(32, R(RSCRATCH), Imm8(XER_OV_SHIFT));
		OR(32, gpr.R(d), R(RSCRATCH));
		break;
	case SPR_WPAR:
	case SPR_DEC:
	case SPR_PMC1:
	case SPR_PMC2:
	case SPR_PMC3:
	case SPR_PMC4:
		FALLBACK_IF(true);
	default:
		gpr.Lock(d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), PPCSTATE(spr[iIndex]));
		break;
	}
	gpr.UnlockAll();
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
	MOV(32, PPCSTATE(msr), gpr.R(inst.RS));
	gpr.UnlockAll();
	gpr.Flush();
	fpr.Flush();

	// If some exceptions are pending and EE are now enabled, force checking
	// external exceptions when going out of mtmsr in order to execute delayed
	// interrupts as soon as possible.
	TEST(32, PPCSTATE(msr), Imm32(0x8000));
	FixupBranch eeDisabled = J_CC(CC_Z);

	TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER));
	FixupBranch noExceptionsPending = J_CC(CC_Z);

	// Check if a CP interrupt is waiting and keep the GPU emulation in sync (issue 4336)
	TEST(32, M((void *)&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP));
	FixupBranch cpInt = J_CC(CC_NZ);

	MOV(32, PPCSTATE(pc), Imm32(js.compilerPC + 4));
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
	MOV(32, gpr.R(inst.RD), PPCSTATE(msr));
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

	X64Reg cr_val = RSCRATCH2;
	// we only need to zero the high bits of RSCRATCH once
	XOR(32, R(RSCRATCH), R(RSCRATCH));
	for (int i = 0; i < 8; i++)
	{
		static const u8 m_flagTable[8] = {0x0,0x1,0x8,0x9,0x0,0x1,0x8,0x9};
		if (i != 0)
			SHL(32, gpr.R(d), Imm8(4));

		MOV(64, R(cr_val), PPCSTATE(cr_val[i]));

		// EQ: Bits 31-0 == 0; set flag bit 1
		TEST(32, R(cr_val), R(cr_val));
		SETcc(CC_Z, R(RSCRATCH));
		LEA(32, gpr.RX(d), MComplex(gpr.RX(d), RSCRATCH, SCALE_2, 0));

		// GT: Value > 0; set flag bit 2
		TEST(64, R(cr_val), R(cr_val));
		SETcc(CC_G, R(RSCRATCH));
		LEA(32, gpr.RX(d), MComplex(gpr.RX(d), RSCRATCH, SCALE_4, 0));

		// SO: Bit 61 set; set flag bit 0
		// LT: Bit 62 set; set flag bit 3
		SHR(64, R(cr_val), Imm8(61));
		MOVZX(32, 8, RSCRATCH, MDisp(cr_val, (u32)(u64)m_flagTable));
		OR(32, gpr.R(d), R(RSCRATCH));
	}

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
					u64 newcrval = PPCCRToInternal(newcr);
					if ((s64)newcrval == (s32)newcrval)
					{
						MOV(64, PPCSTATE(cr_val[i]), Imm32((s32)newcrval));
					}
					else
					{
						MOV(64, R(RSCRATCH), Imm64(newcrval));
						MOV(64, PPCSTATE(cr_val[i]), R(RSCRATCH));
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
					MOV(32, R(RSCRATCH), gpr.R(inst.RS));
					if (i != 7)
						SHR(32, R(RSCRATCH), Imm8(28 - (i * 4)));
					if (i != 0)
						AND(32, R(RSCRATCH), Imm8(0xF));
					MOV(64, R(RSCRATCH), MScaled(RSCRATCH, SCALE_8, (u32)(u64)m_crTable));
					MOV(64, PPCSTATE(cr_val[i]), R(RSCRATCH));
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
		MOV(64, R(RSCRATCH), PPCSTATE(cr_val[inst.CRFS]));
		MOV(64, PPCSTATE(cr_val[inst.CRFD]), R(RSCRATCH));
	}
}

void Jit64::mcrxr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	// Copy XER[0-3] into CR[inst.CRFD]
	MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_ca));
	MOVZX(32, 8, RSCRATCH2, PPCSTATE(xer_so_ov));
	// [0 SO OV CA]
	LEA(32, RSCRATCH, MComplex(RSCRATCH, RSCRATCH2, SCALE_2, 0));
	// [SO OV CA 0] << 3
	SHL(32, R(RSCRATCH), Imm8(4));

	MOV(64, R(RSCRATCH), MDisp(RSCRATCH, (u32)(u64)m_crTable));
	MOV(64, PPCSTATE(cr_val[inst.CRFD]), R(RSCRATCH));

	// Clear XER[0-3]
	MOV(8, PPCSTATE(xer_ca), Imm8(0));
	MOV(8, PPCSTATE(xer_so_ov), Imm8(0));
}

void Jit64::crXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 19, "Invalid crXXX");

	// Special case: crclr
	if (inst.CRBA == inst.CRBB && inst.CRBA == inst.CRBD && inst.SUBOP10 == 193)
	{
		ClearCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3));
		return;
	}

	// TODO(delroth): Potential optimizations could be applied here. For
	// instance, if the two CR bits being loaded are the same, two loads are
	// not required.

	// creqv or crnand or crnor
	bool negateA = inst.SUBOP10 == 289 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;
	// crandc or crorc or crnand or crnor
	bool negateB = inst.SUBOP10 == 129 || inst.SUBOP10 == 417 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;

	GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), RSCRATCH, negateA);
	GetCRFieldBit(inst.CRBB >> 2, 3 - (inst.CRBB & 3), RSCRATCH2, negateB);

	// Compute combined bit
	switch (inst.SUBOP10)
	{
	case 33:  // crnor: ~(A || B) == (~A && ~B)
	case 129: // crandc: A && ~B
	case 257: // crand:  A && B
		AND(8, R(RSCRATCH), R(RSCRATCH2));
		break;

	case 193: // crxor: A ^ B
	case 289: // creqv: ~(A ^ B) = ~A ^ B
		XOR(8, R(RSCRATCH), R(RSCRATCH2));
		break;

	case 225: // crnand: ~(A && B) == (~A || ~B)
	case 417: // crorc: A || ~B
	case 449: // cror:  A || B
		OR(8, R(RSCRATCH), R(RSCRATCH2));
		break;
	}

	// Store result bit in CRBD
	SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), RSCRATCH);
}
