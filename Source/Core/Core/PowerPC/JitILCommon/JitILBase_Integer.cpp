// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _MSC_VER
#pragma warning(disable:4146)  // unary minus operator applied to unsigned type, result still unsigned
#endif

#include "Core/PowerPC/JitILCommon/JitILBase.h"

static void ComputeRC(IREmitter::IRBuilder& ibuild, IREmitter::InstLoc val)
{
	IREmitter::InstLoc res =
		ibuild.EmitICmpCRSigned(val, ibuild.EmitIntConst(0));
	ibuild.EmitStoreCR(res, 0);
}

void JitILBase::reg_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int d = inst.RD, a = inst.RA, s = inst.RS;
	IREmitter::InstLoc val, test, c;
	switch (inst.OPCD)
	{
	case 14: //addi
		val = ibuild.EmitIntConst(inst.SIMM_16);
		if (a)
			val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), val);
		ibuild.EmitStoreGReg(val, d);
		break;
	case 15: //addis
		val = ibuild.EmitIntConst(inst.SIMM_16 << 16);
		if (a)
			val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), val);
		ibuild.EmitStoreGReg(val, d);
		break;
	case 24: //ori
		val = ibuild.EmitIntConst(inst.UIMM);
		val = ibuild.EmitOr(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		break;
	case 25: //oris
		val = ibuild.EmitIntConst(inst.UIMM << 16);
		val = ibuild.EmitOr(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		break;
	case 28: //andi
		val = ibuild.EmitIntConst(inst.UIMM);
		val = ibuild.EmitAnd(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		ComputeRC(ibuild, val);
		break;
	case 29: //andis
		val = ibuild.EmitIntConst(inst.UIMM << 16);
		val = ibuild.EmitAnd(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		ComputeRC(ibuild, val);
		break;
	case 26: //xori
		val = ibuild.EmitIntConst(inst.UIMM);
		val = ibuild.EmitXor(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		break;
	case 27: //xoris
		val = ibuild.EmitIntConst(inst.UIMM << 16);
		val = ibuild.EmitXor(ibuild.EmitLoadGReg(s), val);
		ibuild.EmitStoreGReg(val, a);
		break;
	case 12: //addic
	case 13: //addic_rc
		c = ibuild.EmitIntConst(inst.SIMM_16);
		val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), c);
		ibuild.EmitStoreGReg(val, d);
		test = ibuild.EmitICmpUgt(c, val);
		ibuild.EmitStoreCarry(test);
		if (inst.OPCD == 13)
			ComputeRC(ibuild, val);
		break;
	default:
		FALLBACK_IF(true);
	}
}

void JitILBase::cmpXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc lhs, rhs, res;
	lhs = ibuild.EmitLoadGReg(inst.RA);

	if (inst.OPCD == 31)
	{
		rhs = ibuild.EmitLoadGReg(inst.RB);
		if (inst.SUBOP10 == 32)
		{
			res = ibuild.EmitICmpCRUnsigned(lhs, rhs);
		}
		else
		{
			res = ibuild.EmitICmpCRSigned(lhs, rhs);
		}
	}
	else if (inst.OPCD == 10)
	{
		rhs = ibuild.EmitIntConst(inst.UIMM);
		res = ibuild.EmitICmpCRUnsigned(lhs, rhs);
	}
	else // inst.OPCD == 11
	{
		rhs = ibuild.EmitIntConst(inst.SIMM_16);
		res = ibuild.EmitICmpCRSigned(lhs, rhs);
	}

	js.downcountAmount++; //TODO: should this be somewhere else?

	ibuild.EmitStoreCR(res, inst.CRFD);
}

void JitILBase::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	IREmitter::InstLoc a = nullptr;
	IREmitter::InstLoc s = ibuild.EmitLoadGReg(inst.RS);
	IREmitter::InstLoc b = ibuild.EmitLoadGReg(inst.RB);

	// FIXME: Some instructions does not work well in NSMBW, MP2, etc.
	//        Refer JitIL_Tables.cpp.
	if (inst.SUBOP10 == 28) /* andx */
	{
		a = ibuild.EmitAnd(s, b);
	}
	else if (inst.SUBOP10 == 476) /* nandx */
	{
		a = ibuild.EmitNot(ibuild.EmitAnd(s, b));
	}
	else if (inst.SUBOP10 == 60) /* andcx */
	{
		a = ibuild.EmitAnd(s, ibuild.EmitNot(b));
	}
	else if (inst.SUBOP10 == 444) /* orx */
	{
		a = ibuild.EmitOr(s, b);
	}
	else if (inst.SUBOP10 == 124) /* norx */
	{
		a = ibuild.EmitNot(ibuild.EmitOr(s, b));
	}
	else if (inst.SUBOP10 == 412) /* orcx */
	{
		a = ibuild.EmitOr(s, ibuild.EmitNot(b));
	}
	else if (inst.SUBOP10 == 316) /* xorx */
	{
		a = ibuild.EmitXor(s, b);
	}
	else if (inst.SUBOP10 == 284) /* eqvx */
	{
		a = ibuild.EmitNot(ibuild.EmitXor(s, b));
	}
	else
	{
		PanicAlert("WTF!");
	}

	ibuild.EmitStoreGReg(a, inst.RA);
	if (inst.Rc)
		ComputeRC(ibuild, a);
}

void JitILBase::extsbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitSExt8(val);
	ibuild.EmitStoreGReg(val, inst.RA);
	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::extshx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitSExt16(val);
	ibuild.EmitStoreGReg(val, inst.RA);
	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::subfic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc nota, lhs, val, test;
	nota = ibuild.EmitXor(ibuild.EmitLoadGReg(inst.RA),
	                      ibuild.EmitIntConst(-1));

	if (inst.SIMM_16 == -1)
	{
		val = nota;
		test = ibuild.EmitIntConst(1);
	}
	else
	{
		lhs = ibuild.EmitIntConst(inst.SIMM_16 + 1);
		val = ibuild.EmitAdd(nota, lhs);
		test = ibuild.EmitICmpUgt(lhs, val);
	}

	ibuild.EmitStoreGReg(val, inst.RD);
	ibuild.EmitStoreCarry(test);
}

void JitILBase::subfcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	if (inst.OE)
		PanicAlert("OE: subfcx");

	IREmitter::InstLoc val, test, lhs, rhs;
	lhs = ibuild.EmitLoadGReg(inst.RB);
	rhs = ibuild.EmitLoadGReg(inst.RA);
	val = ibuild.EmitSub(lhs, rhs);
	ibuild.EmitStoreGReg(val, inst.RD);
	test = ibuild.EmitICmpEq(rhs, ibuild.EmitIntConst(0));
	test = ibuild.EmitOr(test, ibuild.EmitICmpUgt(lhs, val));
	ibuild.EmitStoreCarry(test);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::subfex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	if (inst.OE)
		PanicAlert("OE: subfex");

	IREmitter::InstLoc val, test, lhs, rhs, carry;
	rhs = ibuild.EmitLoadGReg(inst.RA);
	carry = ibuild.EmitLoadCarry();
	rhs = ibuild.EmitXor(rhs, ibuild.EmitIntConst(-1));
	rhs = ibuild.EmitAdd(rhs, carry);
	test = ibuild.EmitICmpEq(rhs, ibuild.EmitIntConst(0));
	test = ibuild.EmitAnd(test, carry);
	lhs = ibuild.EmitLoadGReg(inst.RB);
	val = ibuild.EmitAdd(lhs, rhs);
	ibuild.EmitStoreGReg(val, inst.RD);
	test = ibuild.EmitOr(test, ibuild.EmitICmpUgt(lhs, val));
	ibuild.EmitStoreCarry(test);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	if (inst.OE)
		PanicAlert("OE: subfx");

	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
	val = ibuild.EmitSub(val, ibuild.EmitLoadGReg(inst.RA));
	ibuild.EmitStoreGReg(val, inst.RD);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RA);
	val = ibuild.EmitMul(val, ibuild.EmitIntConst(inst.SIMM_16));
	ibuild.EmitStoreGReg(val, inst.RD);
}

void JitILBase::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
	val = ibuild.EmitMul(ibuild.EmitLoadGReg(inst.RA), val);
	ibuild.EmitStoreGReg(val, inst.RD);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::mulhwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	IREmitter::InstLoc a = ibuild.EmitLoadGReg(inst.RA);
	IREmitter::InstLoc b = ibuild.EmitLoadGReg(inst.RB);
	IREmitter::InstLoc d = ibuild.EmitMulHighUnsigned(a, b);
	ibuild.EmitStoreGReg(d, inst.RD);

	if (inst.Rc)
		ComputeRC(ibuild, d);
}

// skipped some of the special handling in here - if we get crashes, let the interpreter handle this op
void JitILBase::divwux(UGeckoInstruction inst)
{
	// FIXME
	FALLBACK_IF(true);

#if 0
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.FlushLockX(RSCRATCH1);
	gpr.Lock(a, b, d);

	if (d != a && d != b)
	{
		gpr.LoadToX64(d, false, true);
	}
	else
	{
		gpr.LoadToX64(d, true, true);
	}

	MOV(32, R(RSCRATCH), gpr.R(a));
	XOR(32, R(RSCRATCH2), R(RSCRATCH));
	gpr.KillImmediate(b);
	DIV(32, gpr.R(b));
	MOV(32, gpr.R(d), R(RSCRATCH));
	gpr.UnlockAll();
	gpr.UnlockAllX();

	if (inst.Rc)
	{
		CALL((u8*)asm_routines.computeRc);
	}
#endif
}

void JitILBase::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
	val = ibuild.EmitAdd(ibuild.EmitLoadGReg(inst.RA), val);
	ibuild.EmitStoreGReg(val, inst.RD);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::addzex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc lhs = ibuild.EmitLoadGReg(inst.RA), val, newcarry;
	val = ibuild.EmitAdd(lhs, ibuild.EmitLoadCarry());
	ibuild.EmitStoreGReg(val, inst.RD);
	newcarry = ibuild.EmitICmpUlt(val, lhs);
	ibuild.EmitStoreCarry(newcarry);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::addex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	IREmitter::InstLoc a = ibuild.EmitLoadGReg(inst.RA);
	IREmitter::InstLoc b = ibuild.EmitLoadGReg(inst.RB);

	IREmitter::InstLoc ab = ibuild.EmitAdd(a, b);
	IREmitter::InstLoc new_carry = ibuild.EmitICmpUlt(ab, a);

	IREmitter::InstLoc previous_carry = ibuild.EmitLoadCarry();
	IREmitter::InstLoc abc = ibuild.EmitAdd(ab, previous_carry);
	new_carry = ibuild.EmitOr(new_carry, ibuild.EmitICmpUlt(abc, ab));

	ibuild.EmitStoreGReg(abc, inst.RD);
	ibuild.EmitStoreCarry(new_carry);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(ibuild, abc);
}

void JitILBase::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	unsigned mask = Helper_Mask(inst.MB, inst.ME);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitRol(val, ibuild.EmitIntConst(inst.SH));
	val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}


void JitILBase::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	unsigned mask = Helper_Mask(inst.MB, inst.ME);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitRol(val, ibuild.EmitIntConst(inst.SH));
	val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
	IREmitter::InstLoc ival = ibuild.EmitLoadGReg(inst.RA);
	ival = ibuild.EmitAnd(ival, ibuild.EmitIntConst(~mask));
	val = ibuild.EmitOr(ival, val);
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	unsigned int mask = Helper_Mask(inst.MB, inst.ME);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitRol(val, ibuild.EmitLoadGReg(inst.RB));
	val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RA);
	val = ibuild.EmitSub(ibuild.EmitIntConst(0), val);
	ibuild.EmitStoreGReg(val, inst.RD);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::srwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	IREmitter::InstLoc samt = ibuild.EmitLoadGReg(inst.RB);
	IREmitter::InstLoc corr;

	// FIXME: We can do better with a cmov
	// FIXME: We can do better on 64-bit
	val = ibuild.EmitShrl(val, samt);
	corr = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
	corr = ibuild.EmitSarl(corr, ibuild.EmitIntConst(31));
	corr = ibuild.EmitXor(corr, ibuild.EmitIntConst(-1));
	val = ibuild.EmitAnd(corr, val);
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::slwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	IREmitter::InstLoc samt = ibuild.EmitLoadGReg(inst.RB);
	IREmitter::InstLoc corr;

	// FIXME: We can do better with a cmov
	// FIXME: We can do better on 64-bit
	val = ibuild.EmitShl(val, samt);
	corr = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
	corr = ibuild.EmitSarl(corr, ibuild.EmitIntConst(31));
	corr = ibuild.EmitXor(corr, ibuild.EmitIntConst(-1));
	val = ibuild.EmitAnd(corr, val);
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::srawx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	// FIXME: We can do a lot better on 64-bit
	IREmitter::InstLoc val, samt, mask, mask2, test;
	val = ibuild.EmitLoadGReg(inst.RS);
	samt = ibuild.EmitLoadGReg(inst.RB);
	mask = ibuild.EmitIntConst(-1);
	val = ibuild.EmitSarl(val, samt);
	mask = ibuild.EmitShl(mask, samt);
	samt = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
	samt = ibuild.EmitSarl(samt, ibuild.EmitIntConst(31));
	samt = ibuild.EmitAnd(samt, ibuild.EmitIntConst(31));
	val = ibuild.EmitSarl(val, samt);
	ibuild.EmitStoreGReg(val, inst.RA);
	mask = ibuild.EmitShl(mask, samt);
	mask2 = ibuild.EmitAnd(mask, ibuild.EmitIntConst(0x7FFFFFFF));
	test = ibuild.EmitOr(val, mask2);
	test = ibuild.EmitICmpUgt(test, mask);
	ibuild.EmitStoreCarry(test);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}

void JitILBase::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	// Shift right by two
	IREmitter::InstLoc input = ibuild.EmitLoadGReg(inst.RS);
	IREmitter::InstLoc output = ibuild.EmitSarl(input, ibuild.EmitIntConst(inst.SH));
	ibuild.EmitStoreGReg(output, inst.RA);
	// Check whether the input is negative and any bits got shifted out.
	unsigned int mask = -1u << inst.SH;
	IREmitter::InstLoc test = ibuild.EmitOr(input, ibuild.EmitIntConst(mask & 0x7FFFFFFF));
	test = ibuild.EmitICmpUgt(test, ibuild.EmitIntConst(mask));

	ibuild.EmitStoreCarry(test);
	if (inst.Rc)
		ComputeRC(ibuild, output);
}

// count leading zeroes
void JitILBase::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
	val = ibuild.EmitCntlzw(val);
	ibuild.EmitStoreGReg(val, inst.RA);

	if (inst.Rc)
		ComputeRC(ibuild, val);
}
