// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>
#include <vector>

#include "Common/MathUtil.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

void Jit64::GenerateConstantOverflow(s64 val)
{
	GenerateConstantOverflow(val > std::numeric_limits<s32>::max() || val < std::numeric_limits<s32>::min());
}

void Jit64::GenerateConstantOverflow(bool overflow)
{
	if (overflow)
	{
		//XER[OV/SO] = 1
		MOV(8, PPCSTATE(xer_so_ov), Imm8(XER_OV_MASK | XER_SO_MASK));
	}
	else
	{
		//XER[OV] = 0
		AND(8, PPCSTATE(xer_so_ov), Imm8(~XER_OV_MASK));
	}
}

// We could do overflow branchlessly, but unlike carry it seems to be quite a bit rarer.
void Jit64::GenerateOverflow()
{
	FixupBranch jno = J_CC(CC_NO);
	//XER[OV/SO] = 1
	MOV(8, PPCSTATE(xer_so_ov), Imm8(XER_OV_MASK | XER_SO_MASK));
	FixupBranch exit = J();
	SetJumpTarget(jno);
	//XER[OV] = 0
	//We need to do this without modifying flags so as not to break stuff that assumes flags
	//aren't clobbered (carry, branch merging): speed doesn't really matter here (this is really
	//rare).
	auto scratch = regs.gpr.Borrow();
	static const u8 ovtable[4] = {0, 0, XER_SO_MASK, XER_SO_MASK};
	MOVZX(32, 8, scratch, PPCSTATE(xer_so_ov));
	MOV(8, scratch, MDisp(scratch, (u32)(u64)ovtable));
	MOV(8, PPCSTATE(xer_so_ov), scratch);
	SetJumpTarget(exit);
}

void Jit64::FinalizeCarry(CCFlags cond)
{
	js.carryFlagSet = false;
	js.carryFlagInverted = false;
	if (js.op->wantsCA)
	{
		// Not actually merging instructions, but the effect is equivalent (we can't have breakpoints/etc in between).
		if (MergeAllowedNextInstructions(1) && js.op[1].wantsCAInFlags)
		{
			if (cond == CC_C || cond == CC_NC)
			{
				js.carryFlagInverted = cond == CC_NC;
			}
			else
			{
				// convert the condition to a carry flag (is there a better way?)
				auto scratch = regs.gpr.Borrow();
				SETcc(cond, scratch);
				SHR(8, scratch, Imm8(1));
			}
			LockFlags();
			js.carryFlagSet = true;
		}
		else
		{
			JitSetCAIf(cond);
		}
	}
}

// Unconditional version
void Jit64::FinalizeCarry(bool ca)
{
	js.carryFlagSet = false;
	js.carryFlagInverted = false;
	if (js.op->wantsCA)
	{
		if (MergeAllowedNextInstructions(1) && js.op[1].wantsCAInFlags)
		{
			if (ca)
				STC();
			else
				CLC();
			LockFlags();
			js.carryFlagSet = true;
		}
		else if (ca)
		{
			JitSetCA();
		}
		else
		{
			JitClearCA();
		}
	}
}

void Jit64::FinalizeCarryOverflow(bool oe, bool inv)
{
	if (oe)
	{
		// Make sure not to lose the carry flags (not a big deal, this path is rare).
		PUSHF();
		//XER[OV] = 0
		AND(8, PPCSTATE(xer_so_ov), Imm8(~XER_OV_MASK));
		FixupBranch jno = J_CC(CC_NO);
		//XER[OV/SO] = 1
		MOV(8, PPCSTATE(xer_so_ov), Imm8(XER_SO_MASK | XER_OV_MASK));
		SetJumpTarget(jno);
		POPF();
	}
	// Do carry
	FinalizeCarry(inv ? CC_NC : CC_C);
}

// Be careful; only set needs_test to false if we can be absolutely sure flags don't need
// to be recalculated and haven't been clobbered. Keep in mind not all instructions set
// sufficient flags -- for example, the flags from SHL/SHR are *not* sufficient for LT/GT
// branches, only EQ.
void Jit64::ComputeRC(const OpArg& arg, bool needs_test, bool needs_sext)
{
	_assert_msg_(DYNA_REC, arg.IsSimpleReg() || arg.IsImm(), "Invalid ComputeRC operand");
	if (arg.IsImm())
	{
		MOV(64, PPCSTATE(cr_val[0]), Imm32(arg.SImm32()));
	}
	else if (needs_sext)
	{
		auto scratch = regs.gpr.Borrow();
		MOVSX(64, 32, scratch, arg);
		MOV(64, PPCSTATE(cr_val[0]), scratch);
	}
	else
	{
		MOV(64, PPCSTATE(cr_val[0]), arg);
	}
	if (CheckMergedBranch(0))
	{
		if (arg.IsImm())
		{
			DoMergedBranchImmediate(arg.SImm32());
		}
		else
		{
			if (needs_test)
			{
				TEST(32, arg, arg);
			}
			else
			{
				// If an operand to the cmp/rc op we're merging with the branch isn't used anymore, it'd be
				// better to flush it here so that we don't have to flush it on both sides of the branch.
				// We don't want to do this if a test is needed though, because it would interrupt macro-op
				// fusion.
				regs.gpr.FlushBatch(~js.op->gprInUse);
			}
			DoMergedBranchCondition();
		}
	}
}

OpArg Jit64::ExtractFromReg(Jit64Reg::Any<Jit64Reg::GPR>& src, int offset)
{
	// store to load forwarding should handle this case efficiently
	// TODO: BEXTR
	if (offset)
	{
		OpArg loc = src.Sync();
		loc.AddMemOffset(offset);
		return loc;
	}

	return src;
}

// we can't do this optimization in the emitter because MOVZX and AND have different effects on flags.
void Jit64::AndWithMask(X64Reg reg, u32 mask)
{
	if (mask == 0xff)
		MOVZX(32, 8, reg, R(reg));
	else if (mask == 0xffff)
		MOVZX(32, 16, reg, R(reg));
	else
		AND(32, R(reg), Imm32(mask));
}

// Following static functions are used in conjunction with regimmop
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

void Jit64::regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const OpArg&, const OpArg&), bool Rc, bool carry)
{
	bool needs_test = false;

	auto rd = regs.gpr.Lock(d);

	// Be careful; addic treats r0 as r0, but addi treats r0 as zero.
	if (a || binary || carry)
	{
		auto ra = regs.gpr.Lock(a);
		carry &= js.op->wantsCA;
		if (ra.IsImm() && !carry)
		{
			rd.SetImm32(doop(ra.Imm32(), value));
		}
		else if (a == d)
		{
			auto xd = rd.Bind(Jit64Reg::ReadWrite);
			(this->*op)(32, xd, Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
		}
		else
		{
			if (doop == Add && ra.IsRegBound() && !carry)
			{
				auto xd = rd.Bind(Jit64Reg::Write);
				auto xa = ra.Bind(Jit64Reg::Reuse);
				needs_test = true;
				LEA(32, xd, MDisp(xa, value));
			}
			else
			{
				MOV(32, rd, ra);
				(this->*op)(32, rd, Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			}
		}
		if (carry)
			FinalizeCarry(CC_C);
	}
	else if (doop == Add)
	{
		// a == 0, which for these instructions imply value = 0
		rd.SetImm32(value);
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "WTF regimmop");
	}
	if (Rc)
		ComputeRC(rd, needs_test, doop != And || (value & 0x80000000));
}

void Jit64::reg_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 d = inst.RD, a = inst.RA, s = inst.RS;
	switch (inst.OPCD)
	{
	case 14: // addi
	{
		auto ra = regs.gpr.Lock(a);
		// occasionally used as MOV - emulate, with immediate propagation
		if (ra.IsImm() && d != a && a != 0)
		{
			auto rd = regs.gpr.Lock(d);
			rd.SetImm32(ra.Imm32() + (u32)(s32)inst.SIMM_16);
		}
		else if (inst.SIMM_16 == 0 && d != a && a != 0)
		{
			auto xd = regs.gpr.Lock(d).Bind(Jit64Reg::Write);
			MOV(32, xd, ra);
		}
		else
		{
			regimmop(d, a, false, (u32)(s32)inst.SIMM_16,  Add, &XEmitter::ADD); //addi
		}
		break;
	}
	case 15: // addis
		regimmop(d, a, false, (u32)inst.SIMM_16 << 16, Add, &XEmitter::ADD);
		break;
	case 24: // ori
		if (a == 0 && s == 0 && inst.UIMM == 0 && !inst.Rc)  //check for nop
		{
			// Make the nop visible in the generated code. not much use but interesting if we see one.
			NOP();
			return;
		}
		regimmop(a, s, true, inst.UIMM, Or, &XEmitter::OR);
		break;
	case 25: // oris
		regimmop(a, s, true, inst.UIMM << 16, Or,  &XEmitter::OR, false);
		break;
	case 28: // andi
		regimmop(a, s, true, inst.UIMM,       And, &XEmitter::AND, true);
		break;
	case 29: // andis
		regimmop(a, s, true, inst.UIMM << 16, And, &XEmitter::AND, true);
		break;
	case 26: // xori
		regimmop(a, s, true, inst.UIMM,       Xor, &XEmitter::XOR, false);
		break;
	case 27: // xoris
		regimmop(a, s, true, inst.UIMM << 16, Xor, &XEmitter::XOR, false);
		break;
	case 12: // addic
		regimmop(d, a, false, (u32)(s32)inst.SIMM_16, Add, &XEmitter::ADD, false, true);
		break;
	case 13: // addic_rc
		regimmop(d, a, true, (u32)(s32)inst.SIMM_16, Add, &XEmitter::ADD, true, true);
		break;
	default:
		FALLBACK_IF(true);
	}
}

bool Jit64::CheckMergedBranch(int crf)
{
	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE))
		return false;

	if (!MergeAllowedNextInstructions(1))
		return false;

	const UGeckoInstruction& next = js.op[1].inst;
	return (((next.OPCD == 16 /* bcx */) ||
	        ((next.OPCD == 19) && (next.SUBOP10 == 528) /* bcctrx */) ||
	        ((next.OPCD == 19) && (next.SUBOP10 == 16) /* bclrx */)) &&
	         (next.BO & BO_DONT_DECREMENT_FLAG) &&
	        !(next.BO & BO_DONT_CHECK_CONDITION) &&
	         (next.BI >> 2) == crf);
}

void Jit64::DoMergedBranch()
{
	// Code that handles successful PPC branching.
	const UGeckoInstruction& next = js.op[1].inst;
	const u32 nextPC = js.op[1].address;
	if (next.OPCD == 16) // bcx
	{
		if (next.LK)
			MOV(32, M(&LR), Imm32(nextPC + 4));

		u32 destination;
		if (next.AA)
			destination = SignExt16(next.BD << 2);
		else
			destination = nextPC + SignExt16(next.BD << 2);
		WriteExit(destination, next.LK, nextPC + 4);
	}
	else if ((next.OPCD == 19) && (next.SUBOP10 == 528)) // bcctrx
	{
		if (next.LK)
			MOV(32, M(&LR), Imm32(nextPC + 4));
		auto scratch = regs.gpr.Borrow();
		MOV(32, scratch, M(&CTR));
		AND(32, scratch, Imm32(0xFFFFFFFC));
		WriteExitDestInRSCRATCH(scratch, next.LK, nextPC + 4);
	}
	else if ((next.OPCD == 19) && (next.SUBOP10 == 16)) // bclrx
	{
		auto scratch = regs.gpr.Borrow();
		MOV(32, scratch, M(&LR));
		if (!m_enable_blr_optimization)
			AND(32, scratch, Imm32(0xFFFFFFFC));
		if (next.LK)
			MOV(32, M(&LR), Imm32(nextPC + 4));
		WriteBLRExit(scratch);
	}
	else
	{
		PanicAlert("WTF invalid branch");
	}
}

void Jit64::DoMergedBranchCondition()
{
	js.downcountAmount++;
	js.skipInstructions = 1;
	const UGeckoInstruction& next = js.op[1].inst;
	int test_bit = 8 >> (next.BI & 3);
	bool condition = !!(next.BO & BO_BRANCH_IF_TRUE);
	const u32 nextPC = js.op[1].address;

	FixupBranch pDontBranch;
	if (test_bit & 8)
		pDontBranch = J_CC(condition ? CC_GE : CC_L, true);  // Test < 0, so jump over if >= 0.
	else if (test_bit & 4)
		pDontBranch = J_CC(condition ? CC_LE : CC_G, true);  // Test > 0, so jump over if <= 0.
	else if (test_bit & 2)
		pDontBranch = J_CC(condition ? CC_NE : CC_E, true);  // Test = 0, so jump over if != 0.
	else  // SO bit, do not branch (we don't emulate SO for cmp).
		pDontBranch = J(true);

	Jit64Reg::Registers branch = regs.Branch();
	branch.Flush();

	DoMergedBranch();

	SetJumpTarget(pDontBranch);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		regs.Flush();
		WriteExit(nextPC + 4);
	}
}

void Jit64::DoMergedBranchImmediate(s64 val)
{
	js.downcountAmount++;
	js.skipInstructions = 1;
	const UGeckoInstruction& next = js.op[1].inst;
	int test_bit = 8 >> (next.BI & 3);
	bool condition = !!(next.BO & BO_BRANCH_IF_TRUE);
	const u32 nextPC = js.op[1].address;

	bool branch;
	if (test_bit & 8)
		branch = condition ? val < 0 : val >= 0;
	else if (test_bit & 4)
		branch = condition ? val > 0 : val <= 0;
	else if (test_bit & 2)
		branch = condition ? val == 0 : val != 0;
	else  // SO bit, do not branch (we don't emulate SO for cmp).
		branch = false;

	if (branch)
	{
		regs.Flush();
		DoMergedBranch();
	}
	else if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		regs.Flush();
		WriteExit(nextPC + 4);
	}
}

void Jit64::cmpXX(UGeckoInstruction inst)
{
	// USES_CR
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int crf = inst.CRFD;
	bool merge_branch = CheckMergedBranch(crf);

	u32 immediate;
	bool useImmediate = true;
	bool signedCompare;

	if (inst.OPCD == 31)
	{
		// cmp / cmpl
		useImmediate = false;
		signedCompare = (inst.SUBOP10 == 0);
	}
	else
	{
		if (inst.OPCD == 10)
		{
			//cmpli
			immediate = (u32)inst.UIMM;
			signedCompare = false;
		}
		else if (inst.OPCD == 11)
		{
			//cmpi
			immediate = (u32)(s32)(s16)inst.UIMM;
			signedCompare = true;
		}
		else
		{
			signedCompare = false; // silence compiler warning
			PanicAlert("cmpXX");
		}
	}

	auto ra = regs.gpr.Lock(a);
	auto rb = useImmediate ? regs.gpr.Imm32(immediate) : regs.gpr.Lock(b);

	if (ra.IsImm() && rb.IsImm())
	{
		OpArg comparand = rb;
	
		// Both registers contain immediate values, so we can pre-compile the compare result
		s64 compareResult = signedCompare ? (s64)ra.SImm32() - (s64)comparand.SImm32() :
		                                    (u64)ra.Imm32() - (u64)comparand.Imm32();
		if (compareResult == (s32)compareResult)
		{
			MOV(64, PPCSTATE(cr_val[crf]), Imm32((u32)compareResult));
		}
		else
		{
			auto scratch = regs.gpr.Borrow();
			MOV(64, scratch, Imm64(compareResult));
			MOV(64, PPCSTATE(cr_val[crf]), scratch);
		}

		if (merge_branch)
			DoMergedBranchImmediate(compareResult);

		return;
	}

	if (signedCompare)
	{
		auto input = regs.gpr.Borrow();

		if (ra.IsImm())
			MOV(64, input, Imm32(ra.SImm32()));
		else
			MOVSX(64, 32, input, ra);

		if (!rb.IsImm())
		{
			auto scratch2 = regs.gpr.Borrow();
			MOVSX(64, 32, scratch2, rb);
			Compare(crf, merge_branch, input, scratch2);
		}
		else
		{
			Compare(crf, merge_branch, input, rb);
		}
	}
	else
	{
		// If we're comparing against zero we know that the input won't be clobbered
		auto input = rb.IsZero() ? ra.Bind(Jit64Reg::Read) : regs.gpr.Borrow();

		if (ra.IsImm())
		{
			MOV(32, input, Imm32(ra.Imm32()));
		}
		else if (!rb.IsZero())
		{
			MOVZX(64, 32, input, ra);
		}

		if (rb.IsImm())
		{
			// sign extension will ruin this, so store it in a register
			if (rb.Imm32() & 0x80000000U)
			{
				auto scratch2 = regs.gpr.Borrow();
				MOV(32, scratch2, rb);
				Compare(crf, merge_branch, input, scratch2);
			}
			else
			{
				Compare(crf, merge_branch, input, rb);
			}
		}
		else
		{
			auto xb = rb.Bind(Jit64Reg::Read);
			Compare(crf, merge_branch, input, xb);
		}
	}

	if (merge_branch)
		DoMergedBranchCondition();
}

void Jit64::Compare(int crf, bool merge_branch, Jit64Reg::Native<Jit64Reg::GPR>& input, Jit64Reg::Any<Jit64Reg::GPR>& comparand)
{
	if (comparand.IsZero())
	{
		MOV(64, PPCSTATE(cr_val[crf]), input);
		// Place the comparison next to the branch for macro-op fusion
		if (merge_branch)
			TEST(64, input, input);
	}
	else
	{
		SUB(64, input, comparand);
		MOV(64, PPCSTATE(cr_val[crf]), input);
	}
}

void Jit64::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS, b = inst.RB;
	bool needs_test = false;
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 31, "Invalid boolX");

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	// TODO: optimize rs.IsImm() ^ rb.IsImm() and a==b==s as well

	if (rs.IsImm() && rb.IsImm())
	{
		// reg.a = constant.s op constant.b
		const u32 rs_offset = rs.Imm32();
		const u32 rb_offset = rb.Imm32();

		if (inst.SUBOP10 == 28)       // andx
			ra.SetImm32(rs_offset & rb_offset);
		else if (inst.SUBOP10 == 476) // nandx
			ra.SetImm32(~(rs_offset & rb_offset));
		else if (inst.SUBOP10 == 60)  // andcx
			ra.SetImm32(rs_offset & (~rb_offset));
		else if (inst.SUBOP10 == 444) // orx
			ra.SetImm32(rs_offset | rb_offset);
		else if (inst.SUBOP10 == 124) // norx
			ra.SetImm32(~(rs_offset | rb_offset));
		else if (inst.SUBOP10 == 412) // orcx
			ra.SetImm32(rs_offset | (~rb_offset));
		else if (inst.SUBOP10 == 316) // xorx
			ra.SetImm32(rs_offset ^ rb_offset);
		else if (inst.SUBOP10 == 284) // eqvx
			ra.SetImm32(~(rs_offset ^ rb_offset));
	}
	else if (s == b)
	{
		// reg.a = unary reg.b
		if ((inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 444 /* orx */))
		{
			if (a != s)
			{
				auto xa = ra.Bind(Jit64Reg::Write);
				MOV(32, xa, rs);
			}
			needs_test = true;
		}
		else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
		{
			if (a != s)
			{
				auto xa = ra.Bind(Jit64Reg::Write);
				MOV(32, xa, rs);
				NOT(32, ra);
			}
			else if (inst.Rc)
			{
				if (ra.IsImm())
				{
					u32 imm = ~ra.Imm32();
					auto xa = ra.Bind(Jit64Reg::Write);
					MOV(32, ra, Imm32(imm));
				}
				else
				{
					// We're going to need this later so may as well load it now
					auto xa = ra.Bind(Jit64Reg::ReadWrite);
					NOT(32, xa);
				}
			}
			else if (ra.IsImm())
			{
				MOV(32, ra, Imm32(~ra.Imm32()));
			}
			else
			{
				NOT(32, ra);
			}
			needs_test = true;
		}
		else if ((inst.SUBOP10 == 412 /* orcx */) || (inst.SUBOP10 == 284 /* eqvx */))
		{
			ra.SetImm32(0xFFFFFFFF);
		}
		else if ((inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 316 /* xorx */))
		{
			ra.SetImm32(0);
		}
		else
		{
			PanicAlert("WTF!");
		}
	}
	else if ((a == s) || (a == b))
	{
		// reg.a = reg.a op b or reg.a = reg.a op reg.s
		OpArg operand = ((a == s) ? rb : rs);
		auto xa = ra.Bind(Jit64Reg::ReadWrite);

		if (inst.SUBOP10 == 28) // andx
		{
			AND(32, ra, operand);
		}
		else if (inst.SUBOP10 == 476) // nandx
		{
			AND(32, ra, operand);
			NOT(32, ra);
			needs_test = true;
		}
		else if (inst.SUBOP10 == 60) // andcx
		{
			if (cpu_info.bBMI1 && rb.IsRegBound() && !rs.IsImm())
			{
				auto xb = rb.Bind(Jit64Reg::Reuse);
				ANDN(32, xa, xb, rs);
			}
			else if (a == b)
			{
				NOT(32, ra);
				AND(32, ra, operand);
			}
			else
			{
				auto scratch = regs.gpr.Borrow();
				MOV(32, scratch, operand);
				NOT(32, scratch);
				AND(32, ra, scratch);
			}
		}
		else if (inst.SUBOP10 == 444) // orx
		{
			OR(32, ra, operand);
		}
		else if (inst.SUBOP10 == 124) // norx
		{
			OR(32, ra, operand);
			NOT(32, ra);
			needs_test = true;
		}
		else if (inst.SUBOP10 == 412) // orcx
		{
			if (a == b)
			{
				NOT(32, ra);
				OR(32, ra, operand);
			}
			else
			{
				auto scratch = regs.gpr.Borrow();
				MOV(32, scratch, operand);
				NOT(32, scratch);
				OR(32, ra, scratch);
			}
		}
		else if (inst.SUBOP10 == 316) // xorx
		{
			XOR(32, ra, operand);
		}
		else if (inst.SUBOP10 == 284) // eqvx
		{
			NOT(32, ra);
			XOR(32, ra, operand);
		}
		else
		{
			PanicAlert("WTF");
		}
	}
	else
	{
		// reg.a = reg.b op reg.s
		auto xa = ra.Bind(Jit64Reg::Write);

		if (inst.SUBOP10 == 28) // andx
		{
			MOV(32, xa, rs);
			AND(32, xa, rb);
		}
		else if (inst.SUBOP10 == 476) // nandx
		{
			MOV(32, xa, rs);
			AND(32, xa, rb);
			NOT(32, xa);
			needs_test = true;
		}
		else if (inst.SUBOP10 == 60) // andcx
		{
			if (cpu_info.bBMI1 && rb.IsRegBound() && !rs.IsImm())
			{
				auto xb = rb.Bind(Jit64Reg::Reuse);
				ANDN(32, xa, xb, rs);
			}
			else
			{
				MOV(32, xa, rb);
				NOT(32, xa);
				AND(32, xa, rs);
			}
		}
		else if (inst.SUBOP10 == 444) // orx
		{
			MOV(32, xa, rs);
			OR(32, xa, rb);
		}
		else if (inst.SUBOP10 == 124) // norx
		{
			MOV(32, xa, rs);
			OR(32, xa, rb);
			NOT(32, xa);
			needs_test = true;
		}
		else if (inst.SUBOP10 == 412) // orcx
		{
			MOV(32, xa, rb);
			NOT(32, xa);
			OR(32, xa, rs);
		}
		else if (inst.SUBOP10 == 316) // xorx
		{
			MOV(32, xa, rs);
			XOR(32, xa, rb);
		}
		else if (inst.SUBOP10 == 284) // eqvx
		{
			MOV(32, xa, rs);
			NOT(32, xa);
			XOR(32, xa, rb);
		}
		else
		{
			PanicAlert("WTF!");
		}
	}

	if (inst.Rc)
	{
		auto xa = ra.Bind(Jit64Reg::Read);
		ComputeRC(xa, needs_test);
	}
}

void Jit64::extsXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;
	int size = inst.SUBOP10 == 922 ? 16 : 8;

	auto ra = regs.gpr.Lock(a);
	auto rs = regs.gpr.Lock(s);

	if (rs.IsImm())
	{
		ra.SetImm32((u32)(s32)(size == 16 ? (s16)rs.Imm32() : (s8)rs.Imm32()));
	}
	else
	{
		auto xa = ra.BindWriteAndReadIf(a == s);
		MOVSX(32, size, xa, rs);
	}
	if (inst.Rc)
		ComputeRC(ra);
}

void Jit64::subfic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;
	auto ra = regs.gpr.Lock(a);
	auto rd = regs.gpr.Lock(d);

	auto xd = rd.BindWriteAndReadIf(a == d);
	int imm = inst.SIMM_16;
	if (d == a)
	{
		if (imm == 0)
		{
			// Flags act exactly like subtracting from 0
			NEG(32, xd);
			// Output carry is inverted
			FinalizeCarry(CC_NC);
		}
		else if (imm == -1)
		{
			NOT(32, xd);
			// CA is always set in this case
			FinalizeCarry(true);
		}
		else
		{
			NOT(32, xd);
			ADD(32, xd, Imm32(imm+1));
			// Output carry is normal
			FinalizeCarry(CC_C);
		}
	}
	else
	{
		MOV(32, xd, Imm32(imm));
		SUB(32, xd, ra);
		// Output carry is inverted
		FinalizeCarry(CC_NC);
	}
	// This instruction has no RC flag
}

void Jit64::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		s32 i = rb.SImm32(), j = ra.SImm32();
		rd.SetImm32(i - j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i - (s64)j);
	}
	else
	{
		auto xd = rd.BindWriteAndReadIf((d == a || d == b));
		auto scratch = regs.gpr.Borrow();
		if (d == b)
		{
			SUB(32, xd, ra);
		}
		else if (d == a)
		{
			MOV(32, scratch, ra);
			MOV(32, xd, rb);
			SUB(32, xd, scratch);
		}
		else
		{
			MOV(32, xd, rb);
			SUB(32, xd, ra);
		}
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(rd, false);
}

void Jit64::MultiplyImmediate(u32 imm, Jit64Reg::Any<Jit64Reg::GPR>& ra, Jit64Reg::Native<Jit64Reg::GPR>& xd, bool overflow)
{
	// simplest cases first
	if (imm == 0)
	{
		XOR(32, xd, xd);
		return;
	}

	if (imm == (u32)-1)
	{
		if (xd.Register() != ra.Register())
			MOV(32, xd, ra);
		NEG(32, xd);
		return;
	}

	// skip these if we need to check overflow flag
	if (!overflow)
	{
		// power of 2; just a shift
		if (MathUtil::IsPow2(imm))
		{
			u32 shift = IntLog2(imm);
			// use LEA if it saves an op
			if (xd.Register() != ra.Register() && shift <= 3 && shift >= 1 && ra.IsRegBound())
			{
				auto xa = ra.Bind(Jit64Reg::Reuse);
				LEA(32, xd, MScaled(xa, SCALE_1 << shift, 0));
			}
			else
			{
				if (xd.Register() != ra.Register())
					MOV(32, xd, ra);
				if (shift)
					SHL(32, xd, Imm8(shift));
			}
			return;
		}

		// We could handle factors of 2^N*3, 2^N*5, and 2^N*9 using lea+shl, but testing shows
		// it seems to be slower overall.
		static u8 lea_scales[3] = { 3, 5, 9 };
		for (int i = 0; i < 3; i++)
		{
			if (imm == lea_scales[i])
			{
				// If 'a' is the same PPC register as 'd' it will re-use the same binding
				auto xa = ra.Bind(Jit64Reg::Read);
				LEA(32, xd, MComplex(xa, xa, SCALE_2 << i, 0));
				return;
			}
		}
	}

	// if we didn't find any better options
	IMUL(32, xd, ra, Imm32(imm));
}

void Jit64::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;
	u32 imm = inst.SIMM_16;

	auto ra = regs.gpr.Lock(a);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm())
	{
		rd.SetImm32(ra.Imm32() * imm);
	}
	else
	{
		auto xd = rd.BindWriteAndReadIf(d == a);
		MultiplyImmediate(imm, ra, xd, false);
	}
}

void Jit64::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		s32 i = ra.SImm32(), j = rb.SImm32();
		rd.SetImm32(i * j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i * (s64)j);
	}
	else
	{
		auto xd = rd.BindWriteAndReadIf((d == a || d == b));
		if (ra.IsImm() || rb.IsImm())
		{
			u32 imm = ra.IsImm() ? ra.Imm32() : rb.Imm32();
			auto& src = ra.IsImm() ? rb : ra;
			MultiplyImmediate(imm, src, xd, inst.OE);
		}
		else if (d == a)
		{
			IMUL(32, xd, rb);
		}
		else if (d == b)
		{
			IMUL(32, xd, ra);
		}
		else
		{
			MOV(32, xd, rb);
			IMUL(32, xd, ra);
		}
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(rd);
}

void Jit64::mulhwXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	bool sign = inst.SUBOP10 == 75;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		if (sign)
			rd.SetImm32((u32)((u64)(((s64)ra.SImm32() * (s64)rb.SImm32())) >> 32));
		else
			rd.SetImm32((u32)(((u64)ra.Imm32() * (u64)rb.Imm32()) >> 32));
	}
	else if (sign)
	{
		// no register choice
		auto eax = regs.gpr.Borrow(EAX);
		auto edx = regs.gpr.Borrow(EDX);

		auto xd = rd.BindWriteAndReadIf(d == a || d == b);
		MOV(32, eax, ra);
		rb.RealizeImmediate();
		IMUL(32, rb);
		MOV(32, rd, edx);
	}
	else
	{
		// Not faster for signed because we'd need two movsx.
		// We need to bind everything to registers since the top 32 bits need to be zero.
		auto xd = rd.BindWriteAndReadIf(d == a || d == b);
		auto src = d == b ? ra.Bind(Jit64Reg::Read) : rb.Bind(Jit64Reg::Read);
		if (d != a && d != b)
			MOV(32, xd, ra);
		IMUL(64, xd, src);
		SHR(64, xd, Imm8(32));
	}
	if (inst.Rc)
		ComputeRC(rd);
}

void Jit64::divwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		if (rb.Imm32() == 0)
		{
			rd.SetImm32(0);
			if (inst.OE)
				GenerateConstantOverflow(true);
		}
		else
		{
			rd.SetImm32(ra.Imm32() / rb.Imm32());
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else if (rb.IsImm())
	{
		u32 divisor = rb.Imm32();
		if (divisor == 0)
		{
			rd.SetImm32(0);
			if (inst.OE)
				GenerateConstantOverflow(true);
		}
		else
		{
			u32 shift = 31;
			while (!(divisor & (1 << shift)))
				shift--;

			if (divisor == (u32)(1 << shift))
			{
				auto xd = rd.BindWriteAndReadIf(d == a);
				if (d != a)
					MOV(32, xd, ra);
				if (shift)
					SHR(32, xd, Imm8(shift));
			}
			else
			{
				u64 magic_dividend = 0x100000000ULL << shift;
				u32 magic = (u32)(magic_dividend / divisor);
				u32 max_quotient = magic >> shift;

				// Test for failure in round-up method
				if (((u64)(magic+1) * (max_quotient*divisor-1)) >> (shift + 32) != max_quotient-1)
				{
					// If failed, use slower round-down method
					auto xd = rd.BindWriteAndReadIf(d == a);
					auto scratch = regs.gpr.Borrow();
					MOV(32, scratch, Imm32(magic));
					if (d != a)
						MOV(32, xd, ra);
					IMUL(64, xd, scratch);
					ADD(64, xd, scratch);
					SHR(64, xd, Imm8(shift+32));
				}
				else
				{
					// If success, use faster round-up method
					auto xa = ra.Bind(Jit64Reg::Read);
					auto xd = rd.Bind(Jit64Reg::Write);
					if (d == a)
					{
						auto scratch = regs.gpr.Borrow();
						MOV(32, scratch, Imm32(magic+1));
						IMUL(64, xd, scratch);
					}
					else
					{
						MOV(32, rd, Imm32(magic+1));
						IMUL(64, xd, ra);
					}
					SHR(64, rd, Imm8(shift+32));
				}
			}
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else
	{
		// no register choice (do we need to do this?)
		auto eax = regs.gpr.Borrow(EAX);
		auto edx = regs.gpr.Borrow(EDX);

		auto xd = rd.BindWriteAndReadIf(d == a || d == b);
		MOV(32, eax, ra);
		XOR(32, edx, edx);
		rb.RealizeImmediate();
		CMP_or_TEST(32, rb, Imm32(0));
		FixupBranch not_div_by_zero = J_CC(CC_NZ);
		MOV(32, rd, edx);
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		FixupBranch end = J();
		SetJumpTarget(not_div_by_zero);
		DIV(32, rb);
		MOV(32, rd, eax);
		if (inst.OE)
		{
			GenerateConstantOverflow(false);
		}
		SetJumpTarget(end);
	}
	if (inst.Rc)
		ComputeRC(rd);
}

void Jit64::divwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		s32 i = ra.SImm32(), j = rb.SImm32();
		if (j == 0 || (i == (s32)0x80000000 && j == -1))
		{
			rd.SetImm32((i >> 31) ^ j);
			if (inst.OE)
				GenerateConstantOverflow(true);
		}
		else
		{
			rd.SetImm32(i / j);
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else
	{
		// no register choice
		auto eax = regs.gpr.Borrow(EAX);
		auto edx = regs.gpr.Borrow(EDX);
		auto xd = rd.BindWriteAndReadIf(d == a || d == b);
		MOV(32, eax, ra);
		CDQ();
		auto xb = rb.Bind(Jit64Reg::Read);
		TEST(32, xb, xb);
		FixupBranch not_div_by_zero = J_CC(CC_NZ);
		MOV(32, rd, edx);
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		FixupBranch end1 = J();
		SetJumpTarget(not_div_by_zero);
		CMP(32, xb, edx);
		FixupBranch not_div_by_neg_one = J_CC(CC_NZ);
		MOV(32, rd, eax);
		NEG(32, rd);
		FixupBranch no_overflow = J_CC(CC_NO);
		XOR(32, rd, rd);
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		FixupBranch end2 = J();
		SetJumpTarget(not_div_by_neg_one);
		IDIV(32, xb);
		MOV(32, rd, eax);
		SetJumpTarget(no_overflow);
		if (inst.OE)
		{
			GenerateConstantOverflow(false);
		}
		SetJumpTarget(end1);
		SetJumpTarget(end2);
	}
	if (inst.Rc)
		ComputeRC(rd);
}

void Jit64::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	bool needs_test = false;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm() && rb.IsImm())
	{
		s32 i = ra.SImm32(), j = rb.SImm32();
		rd.SetImm32(i + j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i + (s64)j);
	}
	else if ((d == a) || (d == b))
	{
		auto xd = rd.Bind(Jit64Reg::ReadWrite);
		ADD(32, rd, d == a ? rb : ra);
		if (inst.OE)
			GenerateOverflow();
	}
	else if (ra.IsRegBound() && rb.IsRegBound() && !inst.OE)
	{
		auto xa = ra.Bind(Jit64Reg::Reuse);
		auto xb = rb.Bind(Jit64Reg::Reuse);
		auto xd = rd.Bind(Jit64Reg::Write);
		LEA(32, xd, MRegSum(xa, xb));
		needs_test = true;
	}
	else
	{
		auto xd = rd.Bind(Jit64Reg::Write);
		MOV(32, rd, ra);
		ADD(32, rd, rb);
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(rd, needs_test);
}

void Jit64::arithXex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	bool regsource = !(inst.SUBOP10 & 64); // addex or subfex
	bool mex = !!(inst.SUBOP10 & 32);      // addmex/subfmex or addzex/subfzex
	bool add = !!(inst.SUBOP10 & 2);       // add or sub
	int a = inst.RA;
	int b = regsource ? inst.RB : a;
	int d = inst.RD;
	bool same_input_sub = !add && regsource && a == b;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	auto xd = rd.BindWriteAndReadIf(!same_input_sub && (d == a || d == b));
	if (!js.carryFlagSet)
		JitGetAndClearCAOV(inst.OE);
	else
		UnlockFlags();

	bool invertedCarry = false;
	// Special case: subfe A, B, B is a common compiler idiom
	if (same_input_sub)
	{
		// Convert carry to borrow
		if (!js.carryFlagInverted)
			CMC();
		SBB(32, rd, rd);
		invertedCarry = true;
	}
	else if (!add && regsource && d == b)
	{
		if (!js.carryFlagInverted)
			CMC();
		if (d != b)
			MOV(32, rd, rb);
		SBB(32, rd, ra);
		invertedCarry = true;
	}
	else
	{
		OpArg source = regsource ? (d == b ? (OpArg)ra : (OpArg)rb) : Imm32(mex ? 0xFFFFFFFF : 0);
		if (d != a && d != b)
			MOV(32, rd, ra);
		if (!add)
			NOT(32, rd);
		// if the source is an immediate, we can invert carry by going from add -> sub and doing src = -1 - src
		if (js.carryFlagInverted && source.IsImm())
		{
			source = Imm32(-1 - source.SImm32());
			SBB(32, rd, source);
			invertedCarry = true;
		}
		else
		{
			if (js.carryFlagInverted)
				CMC();
			ADC(32, rd, source);
		}
	}
	FinalizeCarryOverflow(inst.OE, invertedCarry);
	if (inst.Rc)
		ComputeRC(rd, false);
}

void Jit64::arithcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	bool add = !!(inst.SUBOP10 & 2); // add or sub
	int a = inst.RA, b = inst.RB, d = inst.RD;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);
	auto xd = rd.BindWriteAndReadIf(d == a || d == b);

	if (d == a && d != b)
	{
		if (add)
		{
			ADD(32, rd, rb);
		}
		else
		{
			// special case, because sub isn't reversible
			auto scratch = regs.gpr.Borrow();
			MOV(32, scratch, ra);
			MOV(32, rd, rb);
			SUB(32, rd, scratch);
		}
	}
	else
	{
		if (d != b)
			MOV(32, rd, rb);
		if (add)
			ADD(32, rd, ra);
		else
			SUB(32, rd, ra);
	}

	FinalizeCarryOverflow(inst.OE, !add);
	if (inst.Rc)
		ComputeRC(rd, false);
}

void Jit64::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rs = regs.gpr.Lock(s);

	if (rs.IsImm())
	{
		u32 result = rs.Imm32();
		if (inst.SH != 0)
			result = _rotl(result, inst.SH);
		result &= Helper_Mask(inst.MB, inst.ME);
		ra.SetImm32(result);
		if (inst.Rc)
			ComputeRC(ra);
	}
	else
	{
		bool left_shift = inst.SH && inst.MB == 0 && inst.ME == 31 - inst.SH;
		bool right_shift = inst.SH && inst.ME == 31 && inst.MB == 32 - inst.SH;
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		bool simple_mask = mask == 0xff || mask == 0xffff;
		// In case of a merged branch, track whether or not we've set flags.
		// If not, we need to do a test later to get them.
		bool needs_test = true;
		// If we know the high bit can't be set, we can avoid doing a sign extend for flag storage.
		bool needs_sext = true;
		int mask_size = inst.ME - inst.MB + 1;

		auto xa = ra.BindWriteAndReadIf(a == s);
		if (a != s && left_shift && rs.IsRegBound() && inst.SH <= 3)
		{
			auto xs = rs.Bind(Jit64Reg::Reuse);
			LEA(32, xa, MScaled(xs, SCALE_1 << inst.SH, 0));
		}
		// common optimized case: byte/word extract
		else if (simple_mask && !(inst.SH & (mask_size - 1)))
		{
			MOVZX(32, mask_size, xa, ExtractFromReg(rs, inst.SH ? (32 - inst.SH) >> 3 : 0));
			needs_sext = false;
		}
		// another optimized special case: byte/word extract plus shift
		else if (((mask >> inst.SH) << inst.SH) == mask && !left_shift &&
		         ((mask >> inst.SH) == 0xff || (mask >> inst.SH) == 0xffff))
		{
			MOVZX(32, mask_size, xa, rs);
			SHL(32, ra, Imm8(inst.SH));
			needs_sext = inst.SH + mask_size >= 32;
		}
		else
		{
			if (a != s)
				MOV(32, ra, rs);

			if (left_shift)
			{
				SHL(32, ra, Imm8(inst.SH));
			}
			else if (right_shift)
			{
				SHR(32, ra, Imm8(inst.MB));
				needs_sext = false;
			}
			else
			{
				if (inst.SH != 0)
					ROL(32, ra, Imm8(inst.SH));
				if (!(inst.MB == 0 && inst.ME == 31))
				{
					// we need flags if we're merging the branch
					if (inst.Rc && CheckMergedBranch(0))
						AND(32, ra, Imm32(mask));
					else
						AndWithMask(xa, mask);
					needs_sext = inst.MB == 0;
					needs_test = false;
				}
			}
		}
		if (inst.Rc)
			ComputeRC(ra, needs_test, needs_sext);
	}
}


void Jit64::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rs = regs.gpr.Lock(s);

	if (ra.IsImm() && rs.IsImm())
	{
		u32 mask = Helper_Mask(inst.MB,inst.ME);
		ra.SetImm32((ra.Imm32() & ~mask) | (_rotl(rs.Imm32(),inst.SH) & mask));
		if (inst.Rc)
			ComputeRC(ra);
	}
	else
	{
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		bool needs_test = false;
		if (mask == 0 || (a == s && inst.SH == 0))
		{
			needs_test = true;
		}
		else if (mask == 0xFFFFFFFF)
		{
			auto xa = ra.BindWriteAndReadIf(a == s);
			if (a != s)
				MOV(32, ra, rs);
			if (inst.SH)
				ROL(32, ra, Imm8(inst.SH));
			needs_test = true;
		}
		else if(rs.IsImm())
		{
			auto xa = ra.Bind(Jit64Reg::ReadWrite);
			AndWithMask(xa, ~mask);
			OR(32, ra, Imm32(_rotl(rs.Imm32(), inst.SH) & mask));
		}
		else if (inst.SH)
		{
			bool isLeftShift = mask == 0U - (1U << inst.SH);
			bool isRightShift = mask == (1U << inst.SH) - 1;
			if (ra.IsImm())
			{
				u32 maskA = ra.Imm32() & ~mask;
				auto xa = ra.Bind(Jit64Reg::Write);
				MOV(32, xa, rs);
				if (isLeftShift)
				{
					SHL(32, xa, Imm8(inst.SH));
				}
				else if (isRightShift)
				{
					SHR(32, xa, Imm8(32 - inst.SH));
				}
				else
				{
					ROL(32, xa, Imm8(inst.SH));
					AND(32, xa, Imm32(mask));
				}
				OR(32, xa, Imm32(maskA));
			}
			else
			{
				// TODO: common cases of this might be faster with pinsrb or abuse of AH
				auto scratch = regs.gpr.Borrow();
				MOV(32, scratch, rs);
				if (isLeftShift)
				{
					auto xa = ra.Bind(Jit64Reg::ReadWrite);
					SHL(32, scratch, Imm8(inst.SH));
					AndWithMask(xa, ~mask);
					OR(32, ra, scratch);
				}
				else if (isRightShift)
				{
					auto xa = ra.Bind(Jit64Reg::ReadWrite);
					SHR(32, scratch, Imm8(32 - inst.SH));
					AndWithMask(xa, ~mask);
					OR(32, ra, scratch);
				}
				else
				{
					ROL(32, scratch, Imm8(inst.SH));
					XOR(32, scratch, ra);
					AndWithMask(scratch, mask);
					XOR(32, ra, scratch);
				}
			}
		}
		else
		{
			auto xa = ra.Bind(Jit64Reg::ReadWrite);
			XOR(32, ra, rs);
			AndWithMask(xa, ~mask);
			XOR(32, ra, rs);
		}
		if (inst.Rc)
			ComputeRC(ra, needs_test);
	}
}

void Jit64::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	u32 mask = Helper_Mask(inst.MB, inst.ME);
	if (rb.IsImm() && rs.IsImm())
	{
		ra.SetImm32(_rotl(rs.Imm32(), rb.Imm32() & 0x1F) & mask);
	}
	else
	{
		// no register choice
		auto ecx = regs.gpr.Borrow(ECX);
		MOV(32, ecx, rb);
		auto xa = ra.BindWriteAndReadIf(a == s);
		if (a != s)
		{
			MOV(32, ra, rs);
		}
		ROL(32, ra, ecx);
		// we need flags if we're merging the branch
		if (inst.Rc && CheckMergedBranch(0))
			AND(32, ra, Imm32(mask));
		else
			AndWithMask(xa, mask);
	}
	if (inst.Rc)
		ComputeRC(ra, false);
}

void Jit64::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int d = inst.RD;
	auto ra = regs.gpr.Lock(a);
	auto rd = regs.gpr.Lock(d);

	if (ra.IsImm())
	{
		rd.SetImm32(~(ra.Imm32()) + 1);
		if (inst.OE)
			GenerateConstantOverflow(rd.Imm32() == 0x80000000);
	}
	else
	{
		auto xd = rd.BindWriteAndReadIf(a == d);
		if (a != d)
			MOV(32, rd, ra);
		NEG(32, rd);
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(rd, false);
}

void Jit64::srwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	if (rb.IsImm() && rs.IsImm())
	{
		u32 amount = rb.Imm32();
		ra.SetImm32((amount & 0x20) ? 0 : (rs.Imm32() >> (amount & 0x1f)));
	}
	else
	{
		// no register choice
		auto ecx = regs.gpr.Borrow(ECX);
		MOV(32, ecx, rb);
		auto xa = ra.BindWriteAndReadIf(a == s);
		if (a != s)
		{
			MOV(32, ra, rs);
		}
		SHR(64, ra, ecx);
	}
	// Shift of 0 doesn't update flags, so we need to test just in case
	if (inst.Rc)
		ComputeRC(ra);
}

void Jit64::slwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	if (rb.IsImm() && rs.IsImm())
	{
		u32 amount = rb.Imm32();
		ra.SetImm32((amount & 0x20) ? 0 : rs.Imm32() << (amount & 0x1f));
		if (inst.Rc)
			ComputeRC(ra);
	}
	else
	{
		// no register choice
		auto ecx = regs.gpr.Borrow(ECX);
		MOV(32, ecx, rb);
		auto xa = ra.BindWriteAndReadIf(a == s);
		if (a != s)
			MOV(32, ra, rs);
		SHL(64, ra, ecx);
		if (inst.Rc)
		{
			AND(32, ra, ra);
			ComputeRC(ra, false);
		}
		else
		{
			MOVZX(64, 32, xa, ra);
		}
	}
}

void Jit64::srawx(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;
	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	auto ecx = regs.gpr.Borrow(ECX);
	auto scratch = regs.gpr.Borrow();
	auto xa = ra.BindWriteAndReadIf(a == s || a == b);
	MOV(32, ecx, rb);
	if (a != s)
		MOV(32, ra, rs);
	SHL(64, ra, Imm8(32));
	SAR(64, ra, ecx);
	if (js.op->wantsCA)
	{
		MOV(32, scratch, ra);
		SHR(64, ra, Imm8(32));
		TEST(32, ra, scratch);
	}
	else
	{
		SHR(64, ra, Imm8(32));
	}
	FinalizeCarry(CC_NZ);
	if (inst.Rc)
		ComputeRC(ra);
}

void Jit64::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;
	auto ra = regs.gpr.Lock(a);
	auto rs = regs.gpr.Lock(s);

	if (amount != 0)
	{
		auto xa = ra.BindWriteAndReadIf(a == s);
		if (!js.op->wantsCA)
		{
			if (a != s)
				MOV(32, xa, rs);
			SAR(32, xa, Imm8(amount));
		}
		else
		{
			auto scratch = regs.gpr.Borrow();
			MOV(32, scratch, rs);
			if (a != s)
				MOV(32, xa, scratch);
			// some optimized common cases that can be done in slightly fewer ops
			if (amount == 1)
			{
				SHR(32, scratch, Imm8(31));         // sign
				AND(32, scratch, xa);         // (sign && carry)
				SAR(32, xa, Imm8(1));
				MOV(8, PPCSTATE(xer_ca), scratch);  // XER.CA = sign && carry, aka (input&0x80000001) == 0x80000001
			}
			else
			{
				SAR(32, xa, Imm8(amount));
				SHL(32, scratch, Imm8(32 - amount));
				TEST(32, scratch, xa);
				FinalizeCarry(CC_NZ);
			}
		}
	}
	else
	{
		FinalizeCarry(false);
		auto xa = ra.BindWriteAndReadIf(a == s);

		if (a != s)
			MOV(32, xa, rs);
	}
	if (inst.Rc)
		ComputeRC(ra);
}

// count leading zeroes
void Jit64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	bool needs_test = false;
	auto ra = regs.gpr.Lock(a);
	auto rs = regs.gpr.Lock(s);

	if (rs.IsImm())
	{
		u32 mask = 0x80000000;
		u32 i = 0;
		for (; i < 32; i++, mask >>= 1)
		{
			if (rs.Imm32() & mask)
				break;
		}
		ra.SetImm32(i);
	}
	else
	{
		auto xa = ra.BindWriteAndReadIf(a == s);
		if (cpu_info.bLZCNT)
		{
			LZCNT(32, xa, rs);
			needs_test = true;
		}
		else
		{
			BSR(32, xa, rs);
			FixupBranch gotone = J_CC(CC_NZ);
			MOV(32, ra, Imm32(63));
			SetJumpTarget(gotone);
			XOR(32, ra, Imm8(0x1f));  // flip order
		}
	}

	if (inst.Rc)
		ComputeRC(ra, needs_test, false);
}

void Jit64::twX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	s32 a = inst.RA;
	auto ra = regs.gpr.Lock(a);

	if (inst.OPCD == 3) // twi
	{
		ra.RealizeImmediate();
		CMP(32, ra, Imm32((s32)(s16)inst.SIMM_16));
	}
	else // tw
	{
		auto xa = ra.Bind(Jit64Reg::Read);
		auto rb = regs.gpr.Lock(inst.RB);
		CMP(32, xa, rb);
	}

	std::vector<FixupBranch> fixups;
	CCFlags conditions[] = { CC_A, CC_B, CC_E, CC_G, CC_L };

	for (int i = 0; i < 5; i++)
	{
		if (inst.TO & (1 << i))
		{
			FixupBranch f = J_CC(conditions[i], true);
			fixups.push_back(f);
		}
	}
	FixupBranch dont_trap = J();
	Jit64Reg::Registers branch = regs.Branch();

	for (const FixupBranch& fixup : fixups)
	{
		SetJumpTarget(fixup);
	}
	LOCK();
	OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_PROGRAM));

	branch.Flush();

	WriteExceptionExit();

	SetJumpTarget(dont_trap);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		regs.Flush();
		WriteExit(js.compilerPC + 4);
	}
}
