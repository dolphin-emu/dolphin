// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::ComputeRC(ARM64Reg reg, int crf)
{
	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	SXTW(XA, reg);

	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[crf]));
	gpr.Unlock(WA);
}

void JitArm64::ComputeRC(u32 imm, int crf)
{
	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	MOVI2R(XA, imm);
	if (imm & 0x80000000)
		SXTW(XA, WA);

	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[crf]));
	gpr.Unlock(WA);
}

void JitArm64::ComputeCarry(bool Carry)
{
	if (Carry)
	{
		ARM64Reg WA = gpr.GetReg();
		MOVI2R(WA, 1);
		STRB(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(xer_ca));
		gpr.Unlock(WA);
		return;
	}

	STRB(INDEX_UNSIGNED, WSP, X29, PPCSTATE_OFF(xer_ca));
}

void JitArm64::ComputeCarry()
{
	ARM64Reg WA = gpr.GetReg();
	CSINC(WA, WSP, WSP, CC_CC);
	STRB(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(xer_ca));
	gpr.Unlock(WA);
}

// Following static functions are used in conjunction with reg_imm
static u32 Add(u32 a, u32 b)
{
	return a + b;
}

static u32 Or(u32 a, u32 b)
{
	return a | b;
}

static u32 And(u32 a, u32 b)
{
	return a & b;
}

static u32 Xor(u32 a, u32 b)
{
	return a ^ b;
}

void JitArm64::reg_imm(u32 d, u32 a, bool binary, u32 value, Operation do_op, void (ARM64XEmitter::*op)(ARM64Reg, ARM64Reg, ARM64Reg, ArithOption), bool Rc)
{
	if (a || binary)
	{
		if (gpr.IsImm(a))
		{
			gpr.SetImmediate(d, do_op(gpr.GetImm(a), value));
			if (Rc)
				ComputeRC(gpr.GetImm(d));
		}
		else
		{
			gpr.BindToRegister(d, d == a);
			ARM64Reg WA = gpr.GetReg();
			MOVI2R(WA, value);
			(this->*op)(gpr.R(d), gpr.R(a), WA, ArithOption(WA, ST_LSL, 0));
			gpr.Unlock(WA);

			if (Rc)
				ComputeRC(gpr.R(d), 0);
		}
	}
	else if (do_op == Add)
	{
		// a == 0, implies zero register
		gpr.SetImmediate(d, value);
		if (Rc)
			ComputeRC(value, 0);
	}
	else
	{
		_assert_msg_(DYNA_REC, false, "Hit impossible condition in reg_imm!");
	}
}

void JitArm64::arith_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 d = inst.RD, a = inst.RA, s = inst.RS;

	switch (inst.OPCD)
	{
	case 14: // addi
		reg_imm(d, a, false, (u32)(s32)inst.SIMM_16, Add, &ARM64XEmitter::ADD);
	break;
	case 15: // addis
		reg_imm(d, a, false, (u32)inst.SIMM_16 << 16, Add, &ARM64XEmitter::ADD);
	break;
	case 24: // ori
		if (a == 0 && s == 0 && inst.UIMM == 0 && !inst.Rc)  //check for nop
		{
			// NOP
			return;
		}
		reg_imm(a, s, true, inst.UIMM, Or, &ARM64XEmitter::ORR);
	break;
	case 25: // oris
		reg_imm(a, s, true, inst.UIMM << 16, Or,  &ARM64XEmitter::ORR);
	break;
	case 28: // andi
		reg_imm(a, s, true, inst.UIMM,       And, &ARM64XEmitter::AND, true);
	break;
	case 29: // andis
		reg_imm(a, s, true, inst.UIMM << 16, And, &ARM64XEmitter::AND, true);
	break;
	case 26: // xori
		reg_imm(a, s, true, inst.UIMM,       Xor, &ARM64XEmitter::EOR);
	break;
	case 27: // xoris
		reg_imm(a, s, true, inst.UIMM << 16, Xor, &ARM64XEmitter::EOR);
	break;
	}
}

void JitArm64::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS, b = inst.RB;

	if (gpr.IsImm(s) && gpr.IsImm(b))
	{
		if (inst.SUBOP10 == 28)       // andx
			gpr.SetImmediate(a, (u32)gpr.GetImm(s) & (u32)gpr.GetImm(b));
		else if (inst.SUBOP10 == 476) // nandx
			gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) & (u32)gpr.GetImm(b)));
		else if (inst.SUBOP10 == 60)  // andcx
			gpr.SetImmediate(a, (u32)gpr.GetImm(s) & (~(u32)gpr.GetImm(b)));
		else if (inst.SUBOP10 == 444) // orx
			gpr.SetImmediate(a, (u32)gpr.GetImm(s) | (u32)gpr.GetImm(b));
		else if (inst.SUBOP10 == 124) // norx
			gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) | (u32)gpr.GetImm(b)));
		else if (inst.SUBOP10 == 412) // orcx
			gpr.SetImmediate(a, (u32)gpr.GetImm(s) | (~(u32)gpr.GetImm(b)));
		else if (inst.SUBOP10 == 316) // xorx
			gpr.SetImmediate(a, (u32)gpr.GetImm(s) ^ (u32)gpr.GetImm(b));
		else if (inst.SUBOP10 == 284) // eqvx
			gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) ^ (u32)gpr.GetImm(b)));

		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);
	}
	else if (s == b)
	{
		if ((inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 444 /* orx */))
		{
			if (a != s)
			{
				gpr.BindToRegister(a, false);
				MOV(gpr.R(a), gpr.R(s));
			}
			if (inst.Rc)
				ComputeRC(gpr.R(a));
		}
		else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
		{
			gpr.BindToRegister(a, a == s);
			MVN(gpr.R(a), gpr.R(s));
			if (inst.Rc)
				ComputeRC(gpr.R(a));
		}
		else if ((inst.SUBOP10 == 412 /* orcx */) || (inst.SUBOP10 == 284 /* eqvx */))
		{
			gpr.SetImmediate(a, 0xFFFFFFFF);
			if (inst.Rc)
				ComputeRC(gpr.GetImm(a), 0);
		}
		else if ((inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 316 /* xorx */))
		{
			gpr.SetImmediate(a, 0);
			if (inst.Rc)
				ComputeRC(gpr.GetImm(a), 0);
		}
		else
		{
			PanicAlert("WTF!");
		}
	}
	else
	{
		gpr.BindToRegister(a, (a == s) || (a == b));
		if (inst.SUBOP10 == 28) // andx
		{
			AND(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else if (inst.SUBOP10 == 476) // nandx
		{
			AND(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
			MVN(gpr.R(a), gpr.R(a));
		}
		else if (inst.SUBOP10 == 60) // andcx
		{
			BIC(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else if (inst.SUBOP10 == 444) // orx
		{
			ORR(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else if (inst.SUBOP10 == 124) // norx
		{
			ORR(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
			MVN(gpr.R(a), gpr.R(a));
		}
		else if (inst.SUBOP10 == 412) // orcx
		{
			ORN(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else if (inst.SUBOP10 == 316) // xorx
		{
			EOR(gpr.R(a), gpr.R(s), gpr.R(b), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else if (inst.SUBOP10 == 284) // eqvx
		{
			EON(gpr.R(a), gpr.R(b), gpr.R(s), ArithOption(gpr.R(a), ST_LSL, 0));
		}
		else
		{
			PanicAlert("WTF!");
		}
		if (inst.Rc)
			ComputeRC(gpr.R(a), 0);
	}
}

void JitArm64::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		s32 i = (s32)gpr.GetImm(a), j = (s32)gpr.GetImm(b);
		gpr.SetImmediate(d, i + j);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else if (gpr.IsImm(a) || gpr.IsImm(b))
	{
		int imm_reg = gpr.IsImm(a) ? a : b;
		int in_reg = gpr.IsImm(a) ? b : a;
		gpr.BindToRegister(d, d == in_reg);
		if (gpr.GetImm(imm_reg) < 4096)
		{
			ADD(gpr.R(d), gpr.R(in_reg), gpr.GetImm(imm_reg));
		}
		else
		{
			ARM64Reg WA = gpr.GetReg();
			MOVI2R(WA, gpr.GetImm(imm_reg));
			ADD(gpr.R(d), gpr.R(in_reg), WA);
			gpr.Unlock(WA);
		}
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a || d == b);
		ADD(gpr.R(d), gpr.R(a), gpr.R(b));
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::extsXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;
	int size = inst.SUBOP10 == 922 ? 16 : 8;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, (u32)(s32)(size == 16 ? (s16)gpr.GetImm(s) : (s8)gpr.GetImm(s)));
		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);
	}
	else
	{
		gpr.BindToRegister(a, a == s);
		SBFM(gpr.R(a), gpr.R(s), 0, size - 1);
		if (inst.Rc)
			ComputeRC(gpr.R(a), 0);
	}
}

void JitArm64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, __builtin_clz(gpr.GetImm(s)));
		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);
	}
	else
	{
		gpr.BindToRegister(a, a == s);
		CLZ(gpr.R(a), gpr.R(s));
		if (inst.Rc)
			ComputeRC(gpr.R(a), 0);
	}
}

void JitArm64::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int d = inst.RD;

	FALLBACK_IF(inst.OE);

	if (gpr.IsImm(a))
	{
		gpr.SetImmediate(d, ~((u32)gpr.GetImm(a)) + 1);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a);
		SUB(gpr.R(d), WSP, gpr.R(a), ArithOption(gpr.R(a), ST_LSL, 0));
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::cmp(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int crf = inst.CRFD;
	u32 a = inst.RA, b = inst.RB;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		ComputeRC((s32)gpr.GetImm(a) - (s32)gpr.GetImm(b), crf);
		return;
	}

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg WB = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);
	ARM64Reg XB = EncodeRegTo64(WB);
	ARM64Reg RA = gpr.R(a);
	ARM64Reg RB = gpr.R(b);
	SXTW(XA, RA);
	SXTW(XB, RB);

	SUB(XA, XA, XB);
	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[0]) + (sizeof(PowerPC::ppcState.cr_val[0]) * crf));

	gpr.Unlock(WA, WB);
}

void JitArm64::cmpl(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int crf = inst.CRFD;
	u32 a = inst.RA, b = inst.RB;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		ComputeRC(gpr.GetImm(a) - gpr.GetImm(b), crf);
		return;
	}
	else if (gpr.IsImm(b) && !gpr.GetImm(b))
	{
		ComputeRC(gpr.R(a), crf);
		return;
	}

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);
	SUB(XA, EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(b)));
	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[0]) + (sizeof(PowerPC::ppcState.cr_val[0]) * crf));
	gpr.Unlock(WA);
}

void JitArm64::cmpi(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	u32 a = inst.RA;
	int crf = inst.CRFD;
	if (gpr.IsImm(a))
	{
		ComputeRC((s32)gpr.GetImm(a) - inst.SIMM_16, crf);
		return;
	}

	ARM64Reg WA = gpr.GetReg();

	if (inst.SIMM_16 >= 0 && inst.SIMM_16 < 4096)
	{
		SUB(WA, gpr.R(a), inst.SIMM_16);
	}
	else
	{
		MOVI2R(WA, inst.SIMM_16);
		SUB(WA, gpr.R(a), WA);
	}

	ComputeRC(WA, crf);

	gpr.Unlock(WA);
}

void JitArm64::cmpli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA;
	int crf = inst.CRFD;

	if (gpr.IsImm(a))
	{
		ComputeRC(gpr.GetImm(a) - inst.UIMM, crf);
		return;
	}

	if (!inst.UIMM)
	{
		ComputeRC(gpr.R(a), crf);
		return;
	}

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	if (inst.UIMM < 4096)
	{
		SUB(XA, EncodeRegTo64(gpr.R(a)), inst.UIMM);
	}
	else
	{
		MOVI2R(WA, inst.UIMM);
		SUB(XA, EncodeRegTo64(gpr.R(a)), XA);
	}

	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[0]) + (sizeof(PowerPC::ppcState.cr_val[0]) * crf));
	gpr.Unlock(WA);
}

void JitArm64::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA, s = inst.RS;

	u32 mask = Helper_Mask(inst.MB, inst.ME);
	if (gpr.IsImm(inst.RS))
	{
		gpr.SetImmediate(a, _rotl(gpr.GetImm(s), inst.SH) & mask);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);
		return;
	}

	gpr.BindToRegister(a, a == s);

	ARM64Reg WA = gpr.GetReg();
	ArithOption Shift(gpr.R(s), ST_ROR, 32 - inst.SH);
	MOVI2R(WA, mask);
	AND(gpr.R(a), WA, gpr.R(s), Shift);
	gpr.Unlock(WA);

	if (inst.Rc)
		ComputeRC(gpr.R(a), 0);
}

void JitArm64::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;

	if (gpr.IsImm(s))
	{
		s32 imm = (s32)gpr.GetImm(s);
		gpr.SetImmediate(a, imm >> amount);

		if (amount != 0 && (imm < 0) && (imm << (32 - amount)))
			ComputeCarry(true);
		else
			ComputeCarry(false);
	}
	else if (amount != 0)
	{
		gpr.BindToRegister(a, a == s);
		ARM64Reg RA = gpr.R(a);
		ARM64Reg RS = gpr.R(s);
		ARM64Reg WA = gpr.GetReg();

		ORR(WA, WSP, RS, ArithOption(RS, ST_LSL, 32 - amount));
		ORR(RA, WSP, RS, ArithOption(RS, ST_ASR, amount));
		if (inst.Rc)
			ComputeRC(RA, 0);

		ANDS(WSP, WA, RA, ArithOption(RA, ST_LSL, 0));
		CSINC(WA, WSP, WSP, CC_EQ);
		STRB(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(xer_ca));
		gpr.Unlock(WA);
	}
	else
	{
		gpr.BindToRegister(a, a == s);
		ARM64Reg RA = gpr.R(a);
		ARM64Reg RS = gpr.R(s);
		MOV(RA, RS);
		STRB(INDEX_UNSIGNED, WSP, X29, PPCSTATE_OFF(xer_ca));
	}
}

void JitArm64::addic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int a = inst.RA, d = inst.RD;
	bool rc = inst.OPCD == 13;
	s32 simm = inst.SIMM_16;
	u32 imm = (u32)simm;

	if (gpr.IsImm(a))
	{

		u32 i = gpr.GetImm(a);
		gpr.SetImmediate(d, i + imm);

		bool has_carry = Interpreter::Helper_Carry(i, imm);
		ComputeCarry(has_carry);
		if (rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a);
		if (imm < 4096)
		{
			ADDS(gpr.R(d), gpr.R(a), imm);
		}
		else if (simm > -4096 && simm < 0)
		{
			SUBS(gpr.R(d), gpr.R(a), std::abs(simm));
		}
		else
		{
			ARM64Reg WA = gpr.GetReg();
			MOVI2R(WA, imm);
			ADDS(gpr.R(d), gpr.R(a), WA);
			gpr.Unlock(WA);
		}

		ComputeCarry();
		if (rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, d = inst.RD;

	if (gpr.IsImm(a))
	{
		s32 i = (s32)gpr.GetImm(a);
		gpr.SetImmediate(d, i * inst.SIMM_16);
	}
	else
	{
		gpr.BindToRegister(d, d == a);
		ARM64Reg WA = gpr.GetReg();
		MOVI2R(WA, (u32)(s32)inst.SIMM_16);
		MUL(gpr.R(d), gpr.R(a), WA);
		gpr.Unlock(WA);
	}
}

void JitArm64::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		s32 i = (s32)gpr.GetImm(a), j = (s32)gpr.GetImm(b);
		gpr.SetImmediate(d, i * j);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a || d == b);
		MUL(gpr.R(d), gpr.R(a), gpr.R(b));
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::addzex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, d = inst.RD;

	gpr.BindToRegister(d, d == a);
	ARM64Reg WA = gpr.GetReg();
	LDRB(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(xer_ca));
	CMP(WA, 0);
	CSINC(gpr.R(d), gpr.R(a), gpr.R(a), CC_EQ);
	CMP(gpr.R(d), 0);
	gpr.Unlock(WA);
	ComputeCarry();
	if (inst.Rc)
		ComputeRC(gpr.R(d), 0);
}

void JitArm64::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
		gpr.SetImmediate(d, j - i);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a || d == b);
		SUB(gpr.R(d), gpr.R(b), gpr.R(a));
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::addcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	FALLBACK_IF(inst.OE);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
		gpr.SetImmediate(d, i + j);

		bool has_carry = Interpreter::Helper_Carry(i, j);
		ComputeCarry(has_carry);
		if (inst.Rc)
			ComputeRC(gpr.GetImm(d), 0);
	}
	else
	{
		gpr.BindToRegister(d, d == a || d == b);
		ADDS(gpr.R(d), gpr.R(a), gpr.R(b));

		ComputeCarry();
		if (inst.Rc)
			ComputeRC(gpr.R(d), 0);
	}
}

void JitArm64::slwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int a = inst.RA, b = inst.RB, s = inst.RS;

	if (gpr.IsImm(b) && gpr.IsImm(s))
	{
		u32 i = gpr.GetImm(s), j = gpr.GetImm(b);
		gpr.SetImmediate(a, (j & 0x20) ? 0 : i << (j & 0x1F));

		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);
	}
	else if (gpr.IsImm(b))
	{
		u32 i = gpr.GetImm(b);
		if (i & 0x20)
		{
			gpr.SetImmediate(a, 0);
			if (inst.Rc)
				ComputeRC(0, 0);
		}
		else
		{
			gpr.BindToRegister(a, a == s);
			LSL(gpr.R(a), gpr.R(s), i & 0x1F);
			if (inst.Rc)
				ComputeRC(gpr.R(a), 0);
		}
	}
	else
	{
		gpr.BindToRegister(a, a == b || a == s);

		// PowerPC any shift in the 32-63 register range results in zero
		// Since it has 32bit registers
		// AArch64 it will use a mask of the register size for determining what shift amount
		// So if we use a 64bit so the bits will end up in the high 32bits, and
		// Later instructions will just eat high 32bits since it'll run 32bit operations for everything.
		LSLV(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(s)), EncodeRegTo64(gpr.R(b)));

		if (inst.Rc)
			ComputeRC(gpr.R(a), 0);
	}
}

void JitArm64::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int a = inst.RA, s = inst.RS;
	u32 mask = Helper_Mask(inst.MB, inst.ME);

	if (gpr.IsImm(a) && gpr.IsImm(s))
	{
		u32 res = (gpr.GetImm(a) & ~mask) | (_rotl(gpr.GetImm(s), inst.SH) & mask);
		gpr.SetImmediate(a, res);
		if (inst.Rc)
			ComputeRC(res, 0);
	}
	else
	{
		if (mask == 0 || (a == s && inst.SH == 0))
		{
			// Do Nothing
		}
		else if (mask == 0xFFFFFFFF)
		{
			if (inst.SH || a != s)
				gpr.BindToRegister(a, a == s);

			if (inst.SH)
				ROR(gpr.R(a), gpr.R(s), 32 - inst.SH);
			else if (a != s)
				MOV(gpr.R(a), gpr.R(s));
		}
		else if (inst.SH == 0 && inst.MB <= inst.ME)
		{
			// No rotation
			// No mask inversion
			u32 lsb = 31 - inst.ME;
			u32 width = inst.ME - inst.MB + 1;

			gpr.BindToRegister(a, true);
			ARM64Reg WA = gpr.GetReg();
			UBFX(WA, gpr.R(s), lsb, width);
			BFI(gpr.R(a), WA, lsb, width);
			gpr.Unlock(WA);
		}
		else if (inst.SH && inst.MB <= inst.ME)
		{
			// No mask inversion
			u32 lsb = 31 - inst.ME;
			u32 width = inst.ME - inst.MB + 1;

			gpr.BindToRegister(a, true);
			ARM64Reg WA = gpr.GetReg();
			ROR(WA, gpr.R(s), 32 - inst.SH);
			UBFX(WA, WA, lsb, width);
			BFI(gpr.R(a), WA, lsb, width);
			gpr.Unlock(WA);
		}
		else
		{
			gpr.BindToRegister(a, true);
			ARM64Reg WA = gpr.GetReg();
			ARM64Reg WB = gpr.GetReg();

			MOVI2R(WA, mask);
			BIC(WB, gpr.R(a), WA);
			AND(WA, WA, gpr.R(s), ArithOption(gpr.R(s), ST_ROR, 32 - inst.SH));
			ORR(gpr.R(a), WB, WA);

			gpr.Unlock(WA, WB);
		}

		if (inst.Rc)
			ComputeRC(gpr.R(a), 0);
	}
}
