// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
	static const u8 ovtable[4] = {0, 0, XER_SO_MASK, XER_SO_MASK};
	MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_so_ov));
	MOV(8, R(RSCRATCH), MDisp(RSCRATCH, (u32)(u64)ovtable));
	MOV(8, PPCSTATE(xer_so_ov), R(RSCRATCH));
	SetJumpTarget(exit);
}

void Jit64::FinalizeCarry(CCFlags cond)
{
	js.carryFlagSet = false;
	js.carryFlagInverted = false;
	if (js.op->wantsCA)
	{
		// Be careful: a breakpoint kills flags in between instructions
		if (js.next_op->wantsCAInFlags && !js.next_inst_bp)
		{
			if (cond == CC_C || cond == CC_NC)
			{
				js.carryFlagInverted = cond == CC_NC;
			}
			else
			{
				// convert the condition to a carry flag (is there a better way?)
				SETcc(cond, R(RSCRATCH));
				SHR(8, R(RSCRATCH), Imm8(1));
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
		if (js.next_op->wantsCAInFlags)
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
void Jit64::ComputeRC(const Gen::OpArg & arg, bool needs_test, bool needs_sext)
{
	_assert_msg_(DYNA_REC, arg.IsSimpleReg() || arg.IsImm(), "Invalid ComputeRC operand");
	if (arg.IsImm())
	{
		MOV(64, PPCSTATE(cr_val[0]), Imm32((s32)arg.offset));
	}
	else if (needs_sext)
	{
		MOVSX(64, 32, RSCRATCH, arg);
		MOV(64, PPCSTATE(cr_val[0]), R(RSCRATCH));
	}
	else
	{
		MOV(64, PPCSTATE(cr_val[0]), arg);
	}
	if (CheckMergedBranch(0))
	{
		if (arg.IsImm())
		{
			DoMergedBranchImmediate((s32)arg.offset);
		}
		else
		{
			if (needs_test)
				TEST(32, arg, arg);
			DoMergedBranchCondition();
		}
	}
}

OpArg Jit64::ExtractFromReg(int reg, int offset)
{
	OpArg src = gpr.R(reg);
	// store to load forwarding should handle this case efficiently
	if (offset)
	{
		gpr.StoreFromRegister(reg, FLUSH_MAINTAIN_STATE);
		src = gpr.GetDefaultLocation(reg);
		src.offset += offset;
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

void Jit64::regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc, bool carry)
{
	bool needs_test = false;
	gpr.Lock(d, a);
	// Be careful; addic treats r0 as r0, but addi treats r0 as zero.
	if (a || binary || carry)
	{
		carry &= js.op->wantsCA;
		if (gpr.R(a).IsImm() && !carry)
		{
			gpr.SetImmediate32(d, doop((u32)gpr.R(a).offset, value));
		}
		else if (a == d)
		{
			gpr.BindToRegister(d, true);
			(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
		}
		else
		{
			gpr.BindToRegister(d, false);
			if (doop == Add && gpr.R(a).IsSimpleReg() && !carry)
			{
				needs_test = true;
				LEA(32, gpr.RX(d), MDisp(gpr.RX(a), value));
			}
			else
			{
				MOV(32, gpr.R(d), gpr.R(a));
				(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			}
		}
		if (carry)
			FinalizeCarry(CC_C);
	}
	else if (doop == Add)
	{
		// a == 0, which for these instructions imply value = 0
		gpr.SetImmediate32(d, value);
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "WTF regimmop");
	}
	if (Rc)
		ComputeRC(gpr.R(d), needs_test, doop != And || (value & 0x80000000));
	gpr.UnlockAll();
}

void Jit64::reg_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 d = inst.RD, a = inst.RA, s = inst.RS;
	switch (inst.OPCD)
	{
	case 14: // addi
		// occasionally used as MOV - emulate, with immediate propagation
		if (gpr.R(a).IsImm() && d != a && a != 0)
		{
			gpr.SetImmediate32(d, (u32)gpr.R(a).offset + (u32)(s32)(s16)inst.SIMM_16);
		}
		else if (inst.SIMM_16 == 0 && d != a && a != 0)
		{
			gpr.Lock(a, d);
			gpr.BindToRegister(d, false, true);
			MOV(32, gpr.R(d), gpr.R(a));
			gpr.UnlockAll();
		}
		else
		{
			regimmop(d, a, false, (u32)(s32)inst.SIMM_16,  Add, &XEmitter::ADD); //addi
		}
		break;
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

	const UGeckoInstruction& next = js.next_inst;
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
	if (js.next_inst.OPCD == 16) // bcx
	{
		if (js.next_inst.LK)
			MOV(32, M(&LR), Imm32(js.next_compilerPC + 4));

		u32 destination;
		if (js.next_inst.AA)
			destination = SignExt16(js.next_inst.BD << 2);
		else
			destination = js.next_compilerPC + SignExt16(js.next_inst.BD << 2);
		WriteExit(destination, js.next_inst.LK, js.next_compilerPC + 4);
	}
	else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 528)) // bcctrx
	{
		if (js.next_inst.LK)
			MOV(32, M(&LR), Imm32(js.next_compilerPC + 4));
		MOV(32, R(RSCRATCH), M(&CTR));
		AND(32, R(RSCRATCH), Imm32(0xFFFFFFFC));
		WriteExitDestInRSCRATCH(js.next_inst.LK, js.next_compilerPC + 4);
	}
	else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 16)) // bclrx
	{
		MOV(32, R(RSCRATCH), M(&LR));
		if (!m_enable_blr_optimization)
			AND(32, R(RSCRATCH), Imm32(0xFFFFFFFC));
		if (js.next_inst.LK)
			MOV(32, M(&LR), Imm32(js.next_compilerPC + 4));
		WriteBLRExit();
	}
	else
	{
		PanicAlert("WTF invalid branch");
	}
}

void Jit64::DoMergedBranchCondition()
{
	js.downcountAmount++;
	js.skipnext = true;
	int test_bit = 8 >> (js.next_inst.BI & 3);
	bool condition = !!(js.next_inst.BO & BO_BRANCH_IF_TRUE);

	gpr.UnlockAll();
	gpr.UnlockAllX();
	FixupBranch pDontBranch;
	if (test_bit & 8)
		pDontBranch = J_CC(condition ? CC_GE : CC_L, true);  // Test < 0, so jump over if >= 0.
	else if (test_bit & 4)
		pDontBranch = J_CC(condition ? CC_LE : CC_G, true);  // Test > 0, so jump over if <= 0.
	else if (test_bit & 2)
		pDontBranch = J_CC(condition ? CC_NE : CC_E, true);  // Test = 0, so jump over if != 0.
	else  // SO bit, do not branch (we don't emulate SO for cmp).
		pDontBranch = J(true);

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);

	DoMergedBranch();

	SetJumpTarget(pDontBranch);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(js.next_compilerPC + 4);
	}
}

void Jit64::DoMergedBranchImmediate(s64 val)
{
	js.downcountAmount++;
	js.skipnext = true;
	int test_bit = 8 >> (js.next_inst.BI & 3);
	bool condition = !!(js.next_inst.BO & BO_BRANCH_IF_TRUE);

	gpr.UnlockAll();
	gpr.UnlockAllX();
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
		gpr.Flush();
		fpr.Flush();
		DoMergedBranch();
	}
	else if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(js.next_compilerPC + 4);
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

	OpArg comparand;
	bool signedCompare;
	if (inst.OPCD == 31)
	{
		// cmp / cmpl
		gpr.Lock(a, b);
		comparand = gpr.R(b);
		signedCompare = (inst.SUBOP10 == 0);
	}
	else
	{
		gpr.Lock(a);
		if (inst.OPCD == 10)
		{
			//cmpli
			comparand = Imm32((u32)inst.UIMM);
			signedCompare = false;
		}
		else if (inst.OPCD == 11)
		{
			//cmpi
			comparand = Imm32((u32)(s32)(s16)inst.UIMM);
			signedCompare = true;
		}
		else
		{
			signedCompare = false; // silence compiler warning
			PanicAlert("cmpXX");
		}
	}

	if (gpr.R(a).IsImm() && comparand.IsImm())
	{
		// Both registers contain immediate values, so we can pre-compile the compare result
		s64 compareResult = signedCompare ? (s64)(s32)gpr.R(a).offset - (s64)(s32)comparand.offset :
		                                    (u64)(u32)gpr.R(a).offset - (u64)(u32)comparand.offset;
		if (compareResult == (s32)compareResult)
		{
			MOV(64, PPCSTATE(cr_val[crf]), Imm32((u32)compareResult));
		}
		else
		{
			MOV(64, R(RSCRATCH), Imm64(compareResult));
			MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
		}

		if (merge_branch)
			DoMergedBranchImmediate(compareResult);
	}
	else
	{
		X64Reg input = RSCRATCH;
		if (signedCompare)
		{
			if (gpr.R(a).IsImm())
				MOV(64, R(input), Imm32((s32)gpr.R(a).offset));
			else
				MOVSX(64, 32, input, gpr.R(a));

			if (!comparand.IsImm())
			{
				MOVSX(64, 32, RSCRATCH2, comparand);
				comparand = R(RSCRATCH2);
			}
		}
		else
		{
			if (gpr.R(a).IsImm())
			{
				MOV(32, R(input), Imm32((u32)gpr.R(a).offset));
			}
			else if (comparand.IsImm() && !comparand.offset)
			{
				gpr.BindToRegister(a, true, false);
				input = gpr.RX(a);
			}
			else
			{
				MOVZX(64, 32, input, gpr.R(a));
			}

			if (comparand.IsImm())
			{
				// sign extension will ruin this, so store it in a register
				if (comparand.offset & 0x80000000U)
				{
					MOV(32, R(RSCRATCH2), comparand);
					comparand = R(RSCRATCH2);
				}
			}
			else
			{
				gpr.BindToRegister(b, true, false);
				comparand = gpr.R(b);
			}
		}
		if (comparand.IsImm() && !comparand.offset)
		{
			MOV(64, PPCSTATE(cr_val[crf]), R(input));
			// Place the comparison next to the branch for macro-op fusion
			if (merge_branch)
			{
				// We only need to do a 32-bit compare, since the flags set will be the same as a sign-extended
				// result.
				// We should also test against gpr.R(a) if it's bound, since that's one less cycle of latency
				// (the CPU doesn't have to wait for the movsxd to finish to resolve the branch).
				if (gpr.R(a).IsSimpleReg())
					TEST(32, gpr.R(a), gpr.R(a));
				else
					TEST(32, R(input), R(input));
			}
		}
		else
		{
			SUB(64, R(input), comparand);
			MOV(64, PPCSTATE(cr_val[crf]), R(input));
		}

		if (merge_branch)
			DoMergedBranchCondition();
	}

	gpr.UnlockAll();
}

void Jit64::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS, b = inst.RB;
	bool needs_test = false;
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 31, "Invalid boolX");

	if (gpr.R(s).IsImm() && gpr.R(b).IsImm())
	{
		const u32 rs_offset = static_cast<u32>(gpr.R(s).offset);
		const u32 rb_offset = static_cast<u32>(gpr.R(b).offset);

		if (inst.SUBOP10 == 28)       // andx
			gpr.SetImmediate32(a, rs_offset & rb_offset);
		else if (inst.SUBOP10 == 476) // nandx
			gpr.SetImmediate32(a, ~(rs_offset & rb_offset));
		else if (inst.SUBOP10 == 60)  // andcx
			gpr.SetImmediate32(a, rs_offset & (~rb_offset));
		else if (inst.SUBOP10 == 444) // orx
			gpr.SetImmediate32(a, rs_offset | rb_offset);
		else if (inst.SUBOP10 == 124) // norx
			gpr.SetImmediate32(a, ~(rs_offset | rb_offset));
		else if (inst.SUBOP10 == 412) // orcx
			gpr.SetImmediate32(a, rs_offset | (~rb_offset));
		else if (inst.SUBOP10 == 316) // xorx
			gpr.SetImmediate32(a, rs_offset ^ rb_offset);
		else if (inst.SUBOP10 == 284) // eqvx
			gpr.SetImmediate32(a, ~(rs_offset ^ rb_offset));
	}
	else if (s == b)
	{
		if ((inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 444 /* orx */))
		{
			if (a != s)
			{
				gpr.Lock(a,s);
				gpr.BindToRegister(a, false, true);
				MOV(32, gpr.R(a), gpr.R(s));
			}
			else if (inst.Rc)
			{
				gpr.BindToRegister(a, true, false);
			}
			needs_test = true;
		}
		else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
		{
			if (a != s)
			{
				gpr.Lock(a,s);
				gpr.BindToRegister(a, false, true);
				MOV(32, gpr.R(a), gpr.R(s));
			}
			else if (inst.Rc)
			{
				gpr.BindToRegister(a, true, true);
			}
			else
			{
				gpr.KillImmediate(a, true, true);
			}
			NOT(32, gpr.R(a));
			needs_test = true;
		}
		else if ((inst.SUBOP10 == 412 /* orcx */) || (inst.SUBOP10 == 284 /* eqvx */))
		{
			gpr.SetImmediate32(a, 0xFFFFFFFF);
		}
		else if ((inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 316 /* xorx */))
		{
			gpr.SetImmediate32(a, 0);
		}
		else
		{
			PanicAlert("WTF!");
		}
	}
	else if ((a == s) || (a == b))
	{
		gpr.Lock(a,((a == s) ? b : s));
		OpArg operand = ((a == s) ? gpr.R(b) : gpr.R(s));
		gpr.BindToRegister(a, true, true);

		if (inst.SUBOP10 == 28) // andx
		{
			AND(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 476) // nandx
		{
			AND(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
			needs_test = true;
		}
		else if (inst.SUBOP10 == 60) // andcx
		{
			if (a == b)
			{
				NOT(32, gpr.R(a));
				AND(32, gpr.R(a), operand);
			}
			else
			{
				MOV(32, R(RSCRATCH), operand);
				NOT(32, R(RSCRATCH));
				AND(32, gpr.R(a), R(RSCRATCH));
			}
		}
		else if (inst.SUBOP10 == 444) // orx
		{
			OR(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 124) // norx
		{
			OR(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
			needs_test = true;
		}
		else if (inst.SUBOP10 == 412) // orcx
		{
			if (a == b)
			{
				NOT(32, gpr.R(a));
				OR(32, gpr.R(a), operand);
			}
			else
			{
				MOV(32, R(RSCRATCH), operand);
				NOT(32, R(RSCRATCH));
				OR(32, gpr.R(a), R(RSCRATCH));
			}
		}
		else if (inst.SUBOP10 == 316) // xorx
		{
			XOR(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 284) // eqvx
		{
			NOT(32, gpr.R(a));
			XOR(32, gpr.R(a), operand);
		}
		else
		{
			PanicAlert("WTF");
		}
	}
	else
	{
		gpr.Lock(a,s,b);
		gpr.BindToRegister(a, false, true);

		if (inst.SUBOP10 == 28) // andx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), gpr.R(b));
		}
		else if (inst.SUBOP10 == 476) // nandx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			needs_test = true;
		}
		else if (inst.SUBOP10 == 60) // andcx
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			AND(32, gpr.R(a), gpr.R(s));
		}
		else if (inst.SUBOP10 == 444) // orx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
		}
		else if (inst.SUBOP10 == 124) // norx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			needs_test = true;
		}
		else if (inst.SUBOP10 == 412) // orcx
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			OR(32, gpr.R(a), gpr.R(s));
		}
		else if (inst.SUBOP10 == 316) // xorx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			XOR(32, gpr.R(a), gpr.R(b));
		}
		else if (inst.SUBOP10 == 284) // eqvx
		{
			MOV(32, gpr.R(a), gpr.R(s));
			NOT(32, gpr.R(a));
			XOR(32, gpr.R(a), gpr.R(b));
		}
		else
		{
			PanicAlert("WTF!");
		}
	}
	if (inst.Rc)
		ComputeRC(gpr.R(a), needs_test);
	gpr.UnlockAll();
}

void Jit64::extsXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;
	int size = inst.SUBOP10 == 922 ? 16 : 8;

	if (gpr.R(s).IsImm())
	{
		gpr.SetImmediate32(a, (u32)(s32)(size == 16 ? (s16)gpr.R(s).offset : (s8)gpr.R(s).offset));
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s, true);
		MOVSX(32, size, gpr.RX(a), gpr.R(s));
	}
	if (inst.Rc)
		ComputeRC(gpr.R(a));
	gpr.UnlockAll();
}

void Jit64::subfic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;
	gpr.Lock(a, d);
	gpr.BindToRegister(d, a == d, true);
	int imm = inst.SIMM_16;
	if (d == a)
	{
		if (imm == 0)
		{
			// Flags act exactly like subtracting from 0
			NEG(32, gpr.R(d));
			// Output carry is inverted
			FinalizeCarry(CC_NC);
		}
		else if (imm == -1)
		{
			NOT(32, gpr.R(d));
			// CA is always set in this case
			FinalizeCarry(true);
		}
		else
		{
			NOT(32, gpr.R(d));
			ADD(32, gpr.R(d), Imm32(imm+1));
			// Output carry is normal
			FinalizeCarry(CC_C);
		}
	}
	else
	{
		MOV(32, gpr.R(d), Imm32(imm));
		SUB(32, gpr.R(d), gpr.R(a));
		// Output carry is inverted
		FinalizeCarry(CC_NC);
	}
	gpr.UnlockAll();
	// This instruction has no RC flag
}

void Jit64::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		s32 i = (s32)gpr.R(b).offset, j = (s32)gpr.R(a).offset;
		gpr.SetImmediate32(d, i - j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i - (s64)j);
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		if (d == b)
		{
			SUB(32, gpr.R(d), gpr.R(a));
		}
		else if (d == a)
		{
			MOV(32, R(RSCRATCH), gpr.R(a));
			MOV(32, gpr.R(d), gpr.R(b));
			SUB(32, gpr.R(d), R(RSCRATCH));
		}
		else
		{
			MOV(32, gpr.R(d), gpr.R(b));
			SUB(32, gpr.R(d), gpr.R(a));
		}
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d), false);
	gpr.UnlockAll();
}

void Jit64::MultiplyImmediate(u32 imm, int a, int d, bool overflow)
{
	// simplest cases first
	if (imm == 0)
	{
		XOR(32, gpr.R(d), gpr.R(d));
		return;
	}

	if (imm == (u32)-1)
	{
		if (d != a)
			MOV(32, gpr.R(d), gpr.R(a));
		NEG(32, gpr.R(d));
		return;
	}

	// skip these if we need to check overflow flag
	if (!overflow)
	{
		// power of 2; just a shift
		if (IsPow2(imm))
		{
			u32 shift = IntLog2(imm);
			// use LEA if it saves an op
			if (d != a && shift <= 3 && shift >= 1 && gpr.R(a).IsSimpleReg())
			{
				LEA(32, gpr.RX(d), MScaled(gpr.RX(a), SCALE_1 << shift, 0));
			}
			else
			{
				if (d != a)
					MOV(32, gpr.R(d), gpr.R(a));
				if (shift)
					SHL(32, gpr.R(d), Imm8(shift));
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
				if (d != a)
					gpr.BindToRegister(a, true, false);
				LEA(32, gpr.RX(d), MComplex(gpr.RX(a), gpr.RX(a), SCALE_2 << i, 0));
				return;
			}
		}
	}

	// if we didn't find any better options
	IMUL(32, gpr.RX(d), gpr.R(a), Imm32(imm));
}

void Jit64::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;
	u32 imm = inst.SIMM_16;

	if (gpr.R(a).IsImm())
	{
		gpr.SetImmediate32(d, (u32)gpr.R(a).offset * imm);
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, (d == a), true);
		MultiplyImmediate(imm, a, d, false);
		gpr.UnlockAll();
	}
}

void Jit64::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		s32 i = (s32)gpr.R(a).offset, j = (s32)gpr.R(b).offset;
		gpr.SetImmediate32(d, i * j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i * (s64)j);
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		if (gpr.R(a).IsImm() || gpr.R(b).IsImm())
		{
			u32 imm = gpr.R(a).IsImm() ? (u32)gpr.R(a).offset : (u32)gpr.R(b).offset;
			int src = gpr.R(a).IsImm() ? b : a;
			MultiplyImmediate(imm, src, d, inst.OE);
		}
		else if (d == a)
		{
			IMUL(32, gpr.RX(d), gpr.R(b));
		}
		else if (d == b)
		{
			IMUL(32, gpr.RX(d), gpr.R(a));
		}
		else
		{
			MOV(32, gpr.R(d), gpr.R(b));
			IMUL(32, gpr.RX(d), gpr.R(a));
		}
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d));
	gpr.UnlockAll();
}

void Jit64::mulhwXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	bool sign = inst.SUBOP10 == 75;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		if (sign)
			gpr.SetImmediate32(d, (u32)((u64)(((s64)(s32)gpr.R(a).offset * (s64)(s32)gpr.R(b).offset)) >> 32));
		else
			gpr.SetImmediate32(d, (u32)((gpr.R(a).offset * gpr.R(b).offset) >> 32));
	}
	else
	{
		gpr.Lock(a, b, d);
		// no register choice
		gpr.FlushLockX(EDX, EAX);
		gpr.BindToRegister(d, (d == a || d == b), true);
		MOV(32, R(EAX), gpr.R(a));
		gpr.KillImmediate(b, true, false);
		if (sign)
			IMUL(32, gpr.R(b));
		else
			MUL(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EDX));
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::divwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		if (gpr.R(b).offset == 0)
		{
			gpr.SetImmediate32(d, 0);
			if (inst.OE)
				GenerateConstantOverflow(true);
		}
		else
		{
			gpr.SetImmediate32(d, (u32)gpr.R(a).offset / (u32)gpr.R(b).offset);
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else if (gpr.R(b).IsImm())
	{
		u32 divisor = (u32)gpr.R(b).offset;
		if (divisor == 0)
		{
			gpr.SetImmediate32(d, 0);
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
				gpr.Lock(a, b, d);
				gpr.BindToRegister(d, d == a, true);
				if (d != a)
					MOV(32, gpr.R(d), gpr.R(a));
				if (shift)
					SHR(32, gpr.R(d), Imm8(shift));
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
					gpr.Lock(a, b, d);
					gpr.BindToRegister(d, d == a, true);
					MOV(32, R(RSCRATCH), Imm32(magic));
					if (d != a)
						MOV(32, gpr.R(d), gpr.R(a));
					IMUL(64, gpr.RX(d), R(RSCRATCH));
					ADD(64, gpr.R(d), R(RSCRATCH));
					SHR(64, gpr.R(d), Imm8(shift+32));
				}
				else
				{
					// If success, use faster round-up method
					gpr.Lock(a, b, d);
					gpr.BindToRegister(a, true, false);
					gpr.BindToRegister(d, false, true);
					if (d == a)
					{
						MOV(32, R(RSCRATCH), Imm32(magic+1));
						IMUL(64, gpr.RX(d), R(RSCRATCH));
					}
					else
					{
						MOV(32, gpr.R(d), Imm32(magic+1));
						IMUL(64, gpr.RX(d), gpr.R(a));
					}
					SHR(64, gpr.R(d), Imm8(shift+32));
				}
			}
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else
	{
		gpr.Lock(a, b, d);
		// no register choice (do we need to do this?)
		gpr.FlushLockX(EAX, EDX);
		gpr.BindToRegister(d, (d == a || d == b), true);
		MOV(32, R(EAX), gpr.R(a));
		XOR(32, R(EDX), R(EDX));
		gpr.KillImmediate(b, true, false);
		CMP(32, gpr.R(b), Imm32(0));
		FixupBranch not_div_by_zero = J_CC(CC_NZ);
		MOV(32, gpr.R(d), R(EDX));
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		//MOV(32, R(RAX), gpr.R(d));
		FixupBranch end = J();
		SetJumpTarget(not_div_by_zero);
		DIV(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		if (inst.OE)
		{
			GenerateConstantOverflow(false);
		}
		SetJumpTarget(end);
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::divwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		s32 i = (s32)gpr.R(a).offset, j = (s32)gpr.R(b).offset;
		if (j == 0 || (i == (s32)0x80000000 && j == -1))
		{
			gpr.SetImmediate32(d, (i >> 31) ^ j);
			if (inst.OE)
				GenerateConstantOverflow(true);
		}
		else
		{
			gpr.SetImmediate32(d, i / j);
			if (inst.OE)
				GenerateConstantOverflow(false);
		}
	}
	else
	{
		gpr.Lock(a, b, d);
		// no register choice
		gpr.FlushLockX(EAX, EDX);
		gpr.BindToRegister(d, (d == a || d == b), true);
		MOV(32, R(EAX), gpr.R(a));
		CDQ();
		gpr.BindToRegister(b, true, false);
		TEST(32, gpr.R(b), gpr.R(b));
		FixupBranch not_div_by_zero = J_CC(CC_NZ);
		MOV(32, gpr.R(d), R(EDX));
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		FixupBranch end1 = J();
		SetJumpTarget(not_div_by_zero);
		CMP(32, gpr.R(b), R(EDX));
		FixupBranch not_div_by_neg_one = J_CC(CC_NZ);
		MOV(32, gpr.R(d), R(EAX));
		NEG(32, gpr.R(d));
		FixupBranch no_overflow = J_CC(CC_NO);
		XOR(32, gpr.R(d), gpr.R(d));
		if (inst.OE)
		{
			GenerateConstantOverflow(true);
		}
		FixupBranch end2 = J();
		SetJumpTarget(not_div_by_neg_one);
		IDIV(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		SetJumpTarget(no_overflow);
		if (inst.OE)
		{
			GenerateConstantOverflow(false);
		}
		SetJumpTarget(end1);
		SetJumpTarget(end2);
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	bool needs_test = false;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		s32 i = (s32)gpr.R(a).offset, j = (s32)gpr.R(b).offset;
		gpr.SetImmediate32(d, i + j);
		if (inst.OE)
			GenerateConstantOverflow((s64)i + (s64)j);
	}
	else if ((d == a) || (d == b))
	{
		int operand = ((d == a) ? b : a);
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);
		ADD(32, gpr.R(d), gpr.R(operand));
		if (inst.OE)
			GenerateOverflow();
	}
	else if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && !inst.OE)
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		LEA(32, gpr.RX(d), MComplex(gpr.RX(a), gpr.RX(b), 1, 0));
		needs_test = true;
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), gpr.R(a));
		ADD(32, gpr.R(d), gpr.R(b));
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d), needs_test);
	gpr.UnlockAll();
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

	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, !same_input_sub && (d == a || d == b));
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
		SBB(32, gpr.R(d), gpr.R(d));
		invertedCarry = true;
	}
	else if (!add && regsource && d == b)
	{
		if (!js.carryFlagInverted)
			CMC();
		if (d != b)
			MOV(32, gpr.R(d), gpr.R(b));
		SBB(32, gpr.R(d), gpr.R(a));
		invertedCarry = true;
	}
	else
	{
		OpArg source = regsource ? gpr.R(d == b ? a : b) : Imm32(mex ? 0xFFFFFFFF : 0);
		if (d != a && d != b)
			MOV(32, gpr.R(d), gpr.R(a));
		if (!add)
			NOT(32, gpr.R(d));
		// if the source is an immediate, we can invert carry by going from add -> sub and doing src = -1 - src
		if (js.carryFlagInverted && source.IsImm())
		{
			source.offset = -1 - (s32)source.offset;
			SBB(32, gpr.R(d), source);
			invertedCarry = true;
		}
		else
		{
			if (js.carryFlagInverted)
				CMC();
			ADC(32, gpr.R(d), source);
		}
	}
	FinalizeCarryOverflow(inst.OE, invertedCarry);
	if (inst.Rc)
		ComputeRC(gpr.R(d), false);
	gpr.UnlockAll();
}

void Jit64::arithcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	bool add = !!(inst.SUBOP10 & 2); // add or sub
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, d == a || d == b, true);

	if (d == a && d != b)
	{
		if (add)
		{
			ADD(32, gpr.R(d), gpr.R(b));
		}
		else
		{
			// special case, because sub isn't reversible
			MOV(32, R(RSCRATCH), gpr.R(a));
			MOV(32, gpr.R(d), gpr.R(b));
			SUB(32, gpr.R(d), R(RSCRATCH));
		}
	}
	else
	{
		if (d != b)
			MOV(32, gpr.R(d), gpr.R(b));
		if (add)
			ADD(32, gpr.R(d), gpr.R(a));
		else
			SUB(32, gpr.R(d), gpr.R(a));
	}

	FinalizeCarryOverflow(inst.OE, !add);
	if (inst.Rc)
		ComputeRC(gpr.R(d), false);
	gpr.UnlockAll();
}

void Jit64::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.R(s).IsImm())
	{
		u32 result = (int)gpr.R(s).offset;
		if (inst.SH != 0)
			result = _rotl(result, inst.SH);
		result &= Helper_Mask(inst.MB, inst.ME);
		gpr.SetImmediate32(a, result);
		if (inst.Rc)
			ComputeRC(gpr.R(a));
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

		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s);
		if (a != s && left_shift && gpr.R(s).IsSimpleReg() && inst.SH <= 3)
		{
			LEA(32, gpr.RX(a), MScaled(gpr.RX(s), SCALE_1 << inst.SH, 0));
		}
		// common optimized case: byte/word extract
		else if (simple_mask && !(inst.SH & (mask_size - 1)))
		{
			MOVZX(32, mask_size, gpr.RX(a), ExtractFromReg(s, inst.SH ? (32 - inst.SH) >> 3 : 0));
			needs_sext = false;
		}
		// another optimized special case: byte/word extract plus shift
		else if (((mask >> inst.SH) << inst.SH) == mask && !left_shift &&
		         ((mask >> inst.SH) == 0xff || (mask >> inst.SH) == 0xffff))
		{
			MOVZX(32, mask_size, gpr.RX(a), gpr.R(s));
			SHL(32, gpr.R(a), Imm8(inst.SH));
			needs_sext = inst.SH + mask_size >= 32;
		}
		else
		{
			if (a != s)
				MOV(32, gpr.R(a), gpr.R(s));

			if (left_shift)
			{
				SHL(32, gpr.R(a), Imm8(inst.SH));
			}
			else if (right_shift)
			{
				SHR(32, gpr.R(a), Imm8(inst.MB));
				needs_sext = false;
			}
			else
			{
				if (inst.SH != 0)
					ROL(32, gpr.R(a), Imm8(inst.SH));
				if (!(inst.MB == 0 && inst.ME == 31))
				{
					// we need flags if we're merging the branch
					if (inst.Rc && CheckMergedBranch(0))
						AND(32, gpr.R(a), Imm32(mask));
					else
						AndWithMask(gpr.RX(a), mask);
					needs_sext = inst.MB == 0;
					needs_test = false;
				}
			}
		}
		if (inst.Rc)
			ComputeRC(gpr.R(a), needs_test, needs_sext);
		gpr.UnlockAll();
	}
}


void Jit64::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.R(a).IsImm() && gpr.R(s).IsImm())
	{
		u32 mask = Helper_Mask(inst.MB,inst.ME);
		gpr.SetImmediate32(a, ((u32)gpr.R(a).offset & ~mask) | (_rotl((u32)gpr.R(s).offset,inst.SH) & mask));
		if (inst.Rc)
			ComputeRC(gpr.R(a));
	}
	else
	{
		gpr.Lock(a, s);
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		bool needs_test = false;
		if (mask == 0 || (a == s && inst.SH == 0))
		{
			needs_test = true;
		}
		else if (mask == 0xFFFFFFFF)
		{
			gpr.BindToRegister(a, a == s, true);
			if (a != s)
				MOV(32, gpr.R(a), gpr.R(s));
			if (inst.SH)
				ROL(32, gpr.R(a), Imm8(inst.SH));
			needs_test = true;
		}
		else if(gpr.R(s).IsImm())
		{
			gpr.BindToRegister(a, true, true);
			AndWithMask(gpr.RX(a), ~mask);
			OR(32, gpr.R(a), Imm32(_rotl((u32)gpr.R(s).offset, inst.SH) & mask));
		}
		else if (inst.SH)
		{
			bool isLeftShift = mask == 0U - (1U << inst.SH);
			bool isRightShift = mask == (1U << inst.SH) - 1;
			if (gpr.R(a).IsImm())
			{
				u32 maskA = gpr.R(a).offset & ~mask;
				gpr.BindToRegister(a, false, true);
				MOV(32, gpr.R(a), gpr.R(s));
				if (isLeftShift)
				{
					SHL(32, gpr.R(a), Imm8(inst.SH));
				}
				else if (isRightShift)
				{
					SHR(32, gpr.R(a), Imm8(32 - inst.SH));
				}
				else
				{
					ROL(32, gpr.R(a), Imm8(inst.SH));
					AND(32, gpr.R(a), Imm32(mask));
				}
				OR(32, gpr.R(a), Imm32(maskA));
			}
			else
			{
				// TODO: common cases of this might be faster with pinsrb or abuse of AH
				gpr.BindToRegister(a, true, true);
				MOV(32, R(RSCRATCH), gpr.R(s));
				if (isLeftShift)
				{
					SHL(32, R(RSCRATCH), Imm8(inst.SH));
					AndWithMask(gpr.RX(a), ~mask);
					OR(32, gpr.R(a), R(RSCRATCH));
				}
				else if (isRightShift)
				{
					SHR(32, R(RSCRATCH), Imm8(32 - inst.SH));
					AndWithMask(gpr.RX(a), ~mask);
					OR(32, gpr.R(a), R(RSCRATCH));
				}
				else
				{
					ROL(32, R(RSCRATCH), Imm8(inst.SH));
					XOR(32, R(RSCRATCH), gpr.R(a));
					AndWithMask(RSCRATCH, mask);
					XOR(32, gpr.R(a), R(RSCRATCH));
				}
			}
		}
		else
		{
			gpr.BindToRegister(a, true, true);
			XOR(32, gpr.R(a), gpr.R(s));
			AndWithMask(gpr.RX(a), ~mask);
			XOR(32, gpr.R(a), gpr.R(s));
		}
		if (inst.Rc)
			ComputeRC(gpr.R(a), needs_test);
		gpr.UnlockAll();
	}
}

void Jit64::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, s = inst.RS;

	u32 mask = Helper_Mask(inst.MB, inst.ME);
	if (gpr.R(b).IsImm() && gpr.R(s).IsImm())
	{
		gpr.SetImmediate32(a, _rotl((u32)gpr.R(s).offset, (u32)gpr.R(b).offset & 0x1F) & mask);
	}
	else
	{
		// no register choice
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		MOV(32, R(ECX), gpr.R(b));
		gpr.BindToRegister(a, (a == s), true);
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		ROL(32, gpr.R(a), R(ECX));
		// we need flags if we're merging the branch
		if (inst.Rc && CheckMergedBranch(0))
			AND(32, gpr.R(a), Imm32(mask));
		else
			AndWithMask(gpr.RX(a), mask);
	}
	if (inst.Rc)
		ComputeRC(gpr.R(a), false);
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int d = inst.RD;

	if (gpr.R(a).IsImm())
	{
		gpr.SetImmediate32(d, ~((u32)gpr.R(a).offset) + 1);
		if (inst.OE)
			GenerateConstantOverflow(gpr.R(d).offset == 0x80000000);
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, a == d, true);
		if (a != d)
			MOV(32, gpr.R(d), gpr.R(a));
		NEG(32, gpr.R(d));
		if (inst.OE)
			GenerateOverflow();
	}
	if (inst.Rc)
		ComputeRC(gpr.R(d), false);
	gpr.UnlockAll();
}

void Jit64::srwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;

	if (gpr.R(b).IsImm() && gpr.R(s).IsImm())
	{
		u32 amount = (u32)gpr.R(b).offset;
		gpr.SetImmediate32(a, (amount & 0x20) ? 0 : ((u32)gpr.R(s).offset >> (amount & 0x1f)));
	}
	else
	{
		// no register choice
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		MOV(32, R(ECX), gpr.R(b));
		gpr.BindToRegister(a, a == s, true);
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		SHR(64, gpr.R(a), R(ECX));
	}
	// Shift of 0 doesn't update flags, so we need to test just in case
	if (inst.Rc)
		ComputeRC(gpr.R(a));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::slwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;

	if (gpr.R(b).IsImm() && gpr.R(s).IsImm())
	{
		u32 amount = (u32)gpr.R(b).offset;
		gpr.SetImmediate32(a, (amount & 0x20) ? 0 : (u32)gpr.R(s).offset << amount);
		if (inst.Rc)
			ComputeRC(gpr.R(a));
	}
	else
	{
		// no register choice
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		MOV(32, R(ECX), gpr.R(b));
		gpr.BindToRegister(a, a == s, true);
		if (a != s)
			MOV(32, gpr.R(a), gpr.R(s));
		SHL(64, gpr.R(a), R(ECX));
		if (inst.Rc)
		{
			AND(32, gpr.R(a), gpr.R(a));
			ComputeRC(gpr.R(a), false);
		}
		else
		{
			MOVZX(64, 32, gpr.RX(a), gpr.R(a));
		}
		gpr.UnlockAll();
		gpr.UnlockAllX();
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

	gpr.FlushLockX(ECX);
	gpr.Lock(a, s, b);
	gpr.BindToRegister(a, (a == s || a == b), true);
	MOV(32, R(ECX), gpr.R(b));
	if (a != s)
		MOV(32, gpr.R(a), gpr.R(s));
	SHL(64, gpr.R(a), Imm8(32));
	SAR(64, gpr.R(a), R(ECX));
	if (js.op->wantsCA)
	{
		MOV(32, R(RSCRATCH), gpr.R(a));
		SHR(64, gpr.R(a), Imm8(32));
		TEST(32, gpr.R(a), R(RSCRATCH));
	}
	else
	{
		SHR(64, gpr.R(a), Imm8(32));
	}
	FinalizeCarry(CC_NZ);
	if (inst.Rc)
		ComputeRC(gpr.R(a));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;

	if (amount != 0)
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s, true);
		if (!js.op->wantsCA)
		{
			if (a != s)
				MOV(32, gpr.R(a), gpr.R(s));
			SAR(32, gpr.R(a), Imm8(amount));
		}
		else
		{
			MOV(32, R(RSCRATCH), gpr.R(s));
			if (a != s)
				MOV(32, gpr.R(a), R(RSCRATCH));
			// some optimized common cases that can be done in slightly fewer ops
			if (amount == 1)
			{
				SHR(32, R(RSCRATCH), Imm8(31));         // sign
				AND(32, R(RSCRATCH), gpr.R(a));         // (sign && carry)
				SAR(32, gpr.R(a), Imm8(1));
				MOV(8, PPCSTATE(xer_ca), R(RSCRATCH));  // XER.CA = sign && carry, aka (input&0x80000001) == 0x80000001
			}
			else
			{
				SAR(32, gpr.R(a), Imm8(amount));
				SHL(32, R(RSCRATCH), Imm8(32 - amount));
				TEST(32, R(RSCRATCH), gpr.R(a));
				FinalizeCarry(CC_NZ);
			}
		}
	}
	else
	{
		gpr.Lock(a, s);
		FinalizeCarry(false);
		gpr.BindToRegister(a, a == s, true);

		if (a != s)
			MOV(32, gpr.R(a), gpr.R(s));
	}
	if (inst.Rc)
		ComputeRC(gpr.R(a));
	gpr.UnlockAll();
}

// count leading zeroes
void Jit64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	bool needs_test = false;

	if (gpr.R(s).IsImm())
	{
		u32 mask = 0x80000000;
		u32 i = 0;
		for (; i < 32; i++, mask >>= 1)
		{
			if ((u32)gpr.R(s).offset & mask)
				break;
		}
		gpr.SetImmediate32(a, i);
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s, true);
		if (cpu_info.bLZCNT)
		{
			LZCNT(32, gpr.RX(a), gpr.R(s));
			needs_test = true;
		}
		else
		{
			BSR(32, gpr.RX(a), gpr.R(s));
			FixupBranch gotone = J_CC(CC_NZ);
			MOV(32, gpr.R(a), Imm32(63));
			SetJumpTarget(gotone);
			XOR(32, gpr.R(a), Imm8(0x1f));  // flip order
		}
	}

	if (inst.Rc)
		ComputeRC(gpr.R(a), needs_test, false);
	gpr.UnlockAll();
}

void Jit64::twx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	s32 a = inst.RA;

	if (inst.OPCD == 3) // twi
	{
		gpr.KillImmediate(a, true, false);
		CMP(32, gpr.R(a), Imm32((s32)(s16)inst.SIMM_16));
	}
	else // tw
	{
		gpr.BindToRegister(a, true, false);
		CMP(32, gpr.R(a), gpr.R(inst.RB));
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

	for (const FixupBranch& fixup : fixups)
	{
		SetJumpTarget(fixup);
	}
	LOCK();
	OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_PROGRAM));

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);

	WriteExceptionExit();

	SetJumpTarget(dont_trap);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(js.compilerPC + 4);
	}
}
