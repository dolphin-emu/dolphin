// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
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

void JitArm64::ComputeRC(u32 d)
{
	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);

	if (gpr.IsImm(d))
	{
		MOVI2R(XA, gpr.GetImm(d));
		SXTW(XA, XA);
	}
	else
	{
		SXTW(XA, gpr.R(d));
	}

	STR(INDEX_UNSIGNED, XA, X29, PPCSTATE_OFF(cr_val[0]));
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
		}
		else
		{
			ARM64Reg WA = gpr.GetReg();
			MOVI2R(WA, value);
			(this->*op)(gpr.R(d), gpr.R(a), WA, ArithOption(WA, ST_LSL, 0));
			gpr.Unlock(WA);
		}

		if (Rc)
			ComputeRC(d);
	}
	else if (do_op == Add)
	{
		// a == 0, implies zero register
		gpr.SetImmediate(d, value);
		if (Rc)
			ComputeRC(d);
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
			ComputeRC(a);
	}
	else if (s == b)
	{
		if ((inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 444 /* orx */))
		{
			if (a != s)
			{
				MOV(gpr.R(a), gpr.R(s));
			}
		}
		else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
		{
			MVN(gpr.R(a), gpr.R(s));
		}
		else if ((inst.SUBOP10 == 412 /* orcx */) || (inst.SUBOP10 == 284 /* eqvx */))
		{
			gpr.SetImmediate(a, 0xFFFFFFFF);
		}
		else if ((inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 316 /* xorx */))
		{
			gpr.SetImmediate(a, 0);
		}
		else
		{
			PanicAlert("WTF!");
		}
		if (inst.Rc)
			ComputeRC(a);
	}
	else
	{
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
			ComputeRC(a);
	}
}

void JitArm64::extsXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;
	int size = inst.SUBOP10 == 922 ? 16 : 8;

	if (gpr.IsImm(s))
		gpr.SetImmediate(a, (u32)(s32)(size == 16 ? (s16)gpr.GetImm(s) : (s8)gpr.GetImm(s)));
	else
		SBFM(gpr.R(a), gpr.R(s), 0, size - 1);

	if (inst.Rc)
		ComputeRC(a);
}

void JitArm64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.IsImm(s))
	{
		u32 mask = 0x80000000;
		u32 i = 0;
		for (; i < 32; i++, mask >>= 1)
		{
			if ((u32)gpr.GetImm(s) & mask)
				break;
		}
		gpr.SetImmediate(a, i);
	}
	else
	{
		CLZ(gpr.R(a), gpr.R(s));
	}

	if (inst.Rc)
		ComputeRC(a);
}
