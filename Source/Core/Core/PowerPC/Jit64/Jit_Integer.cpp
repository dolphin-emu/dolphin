// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <vector>

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

void Jit64::GenerateConstantOverflow(bool overflow)
{
	if (overflow)
	{
		//XER[OV/SO] = 1
		OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_SO_MASK | XER_OV_MASK));
	}
	else
	{
		//XER[OV] = 0
		AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_OV_MASK));
	}
}

void Jit64::GenerateOverflow()
{
	FixupBranch jno = J_CC(CC_NO);
	//XER[OV/SO] = 1
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_SO_MASK | XER_OV_MASK));
	FixupBranch exit = J();
	SetJumpTarget(jno);
	//XER[OV] = 0
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_OV_MASK));
	SetJumpTarget(exit);
}

// Assumes CA,OV are clear
void Jit64::FinalizeCarryOverflow(bool oe, bool inv)
{
	// USES_XER
	if (oe)
	{
		FixupBranch jno = J_CC(CC_NO);
		// Do carry
		FixupBranch carry1 = J_CC(inv ? CC_C : CC_NC);
		JitSetCA();
		SetJumpTarget(carry1);
		//XER[OV/SO] = 1
		OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_SO_MASK | XER_OV_MASK));
		FixupBranch exit = J();
		SetJumpTarget(jno);
		// Do carry
		FixupBranch carry2 = J_CC(inv ? CC_C : CC_NC);
		JitSetCA();
		SetJumpTarget(carry2);
		SetJumpTarget(exit);
	}
	else
	{
		// Do carry
		FixupBranch carry1 = J_CC(inv ? CC_C : CC_NC);
		JitSetCA();
		SetJumpTarget(carry1);
	}
}

void Jit64::GetCarryEAXAndClear()
{
	MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
	BTR(32, R(EAX), Imm8(29));
}

// Assumes that XER is in EAX and that the CA bit is clear.
void Jit64::FinalizeCarryGenerateOverflowEAX(bool oe, bool inv)
{
	// USES_XER
	if (oe)
	{
		FixupBranch jno = J_CC(CC_NO);
		// Do carry
		FixupBranch carry1 = J_CC(inv ? CC_C : CC_NC);
		OR(32, R(EAX), Imm32(XER_CA_MASK));
		SetJumpTarget(carry1);
		//XER[OV/SO] = 1
		OR(32, R(EAX), Imm32(XER_SO_MASK | XER_OV_MASK));
		FixupBranch exit = J();
		SetJumpTarget(jno);
		// Do carry
		FixupBranch carry2 = J_CC(inv ? CC_C : CC_NC);
		OR(32, R(EAX), Imm32(XER_CA_MASK));
		SetJumpTarget(carry2);
		//XER[OV] = 0
		AND(32, R(EAX), Imm32(~XER_OV_MASK));
		SetJumpTarget(exit);
	}
	else
	{
		// Do carry
		FixupBranch carry1 = J_CC(inv ? CC_C : CC_NC);
		OR(32, R(EAX), Imm32(XER_CA_MASK));
		SetJumpTarget(carry1);
	}
	// Dump EAX back into XER
	MOV(32, M(&PowerPC::ppcState.spr[SPR_XER]), R(EAX));
}

// Assumes that the flags were just set through an addition.
void Jit64::GenerateCarry()
{
	// USES_XER
	FixupBranch pNoCarry = J_CC(CC_NC);
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK));
	FixupBranch pContinue = J();
	SetJumpTarget(pNoCarry);
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~(XER_CA_MASK)));
	SetJumpTarget(pContinue);
}

// Assumes that Sign and Zero flags were set by the last operation. Preserves all flags and registers.
void Jit64::GenerateRC()
{
	FixupBranch pZero  = J_CC(CC_Z);
	FixupBranch pNegative = J_CC(CC_S);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x4)); // Result > 0
	FixupBranch continue1 = J();

	SetJumpTarget(pNegative);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x8)); // Result < 0
	FixupBranch continue2 = J();

	SetJumpTarget(pZero);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x2)); // Result == 0

	SetJumpTarget(continue1);
	SetJumpTarget(continue2);
}

void Jit64::ComputeRC(const Gen::OpArg & arg)
{
	if (arg.IsImm())
	{
		s32 value = (s32)arg.offset;
		if (value < 0)
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x8));
		else if (value > 0)
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x4));
		else
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x2));
	}
	else
	{
		if (arg.IsSimpleReg())
			TEST(32, arg, arg);
		else
			CMP(32, arg, Imm8(0));
		FixupBranch pLesser  = J_CC(CC_L);
		FixupBranch pGreater = J_CC(CC_G);
		MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x2)); // _x86Reg == 0
		FixupBranch continue1 = J();

		SetJumpTarget(pGreater);
		MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x4)); // _x86Reg > 0
		FixupBranch continue2 = J();

		SetJumpTarget(pLesser);
		MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x8)); // _x86Reg < 0

		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
	}
}

static u32 Add(u32 a, u32 b) {return a + b;}
static u32 Or (u32 a, u32 b) {return a | b;}
static u32 And(u32 a, u32 b) {return a & b;}
static u32 Xor(u32 a, u32 b) {return a ^ b;}

void Jit64::regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc, bool carry)
{
	gpr.Lock(d, a);
	if (a || binary || carry)  // yeh nasty special case addic
	{
		if (gpr.R(a).IsImm() && !carry)
		{
			gpr.SetImmediate32(d, doop((u32)gpr.R(a).offset, value));
			if (Rc)
			{
				ComputeRC(gpr.R(d));
			}
		}
		else if (a == d)
		{
			gpr.KillImmediate(d, true, true);
			(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			if (Rc)
			{
				// All of the possible passed operators affect Sign/Zero flags
				GenerateRC();
			}
			if (carry)
				GenerateCarry();
		}
		else
		{
			gpr.BindToRegister(d, false);
			MOV(32, gpr.R(d), gpr.R(a));
			(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			if (Rc)
			{
				// All of the possible passed operators affect Sign/Zero flags
				GenerateRC();
			}
			if (carry)
				GenerateCarry();
		}
	}
	else if (doop == Add)
	{
		// a == 0, which for these instructions imply value = 0
		gpr.SetImmediate32(d, value);
		if (Rc)
		{
			ComputeRC(gpr.R(d));
		}
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "WTF regimmop");
	}
	gpr.UnlockAll();
}

void Jit64::reg_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 d = inst.RD, a = inst.RA, s = inst.RS;
	switch (inst.OPCD)
	{
	case 14:  // addi
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
	case 15:
		if (a == 0) // lis
		{
			// Merge with next instruction if loading a 32-bits immediate value (lis + addi, lis + ori)
			if (!js.isLastInstruction && !Core::g_CoreStartupParameter.bEnableDebugging)
			{
				if ((js.next_inst.OPCD == 14) && (js.next_inst.RD == d) && (js.next_inst.RA == d)) // addi
				{
					gpr.SetImmediate32(d, ((u32)inst.SIMM_16 << 16) + (u32)(s32)js.next_inst.SIMM_16);
					js.downcountAmount++;
					js.skipnext = true;
					break;
				}
				else if ((js.next_inst.OPCD == 24) && (js.next_inst.RA == d) && (js.next_inst.RS == d)) // ori
				{
					gpr.SetImmediate32(d, ((u32)inst.SIMM_16 << 16) | (u32)js.next_inst.UIMM);
					js.downcountAmount++;
					js.skipnext = true;
					break;
				}
			}

			// Not merged
			regimmop(d, a, false, (u32)inst.SIMM_16 << 16, Add, &XEmitter::ADD);
		}
		else // addis
		{
			regimmop(d, a, false, (u32)inst.SIMM_16 << 16, Add, &XEmitter::ADD);
		}
		break;
	case 24:
		if (a == 0 && s == 0 && inst.UIMM == 0 && !inst.Rc)  //check for nop
		{NOP(); return;} //make the nop visible in the generated code. not much use but interesting if we see one.
		regimmop(a, s, true, inst.UIMM, Or, &XEmitter::OR);
		break; //ori
	case 25: regimmop(a, s, true, inst.UIMM << 16, Or,  &XEmitter::OR, false); break;//oris
	case 28: regimmop(a, s, true, inst.UIMM,       And, &XEmitter::AND, true); break;
	case 29: regimmop(a, s, true, inst.UIMM << 16, And, &XEmitter::AND, true); break;
	case 26: regimmop(a, s, true, inst.UIMM,       Xor, &XEmitter::XOR, false); break; //xori
	case 27: regimmop(a, s, true, inst.UIMM << 16, Xor, &XEmitter::XOR, false); break; //xoris
	case 12: regimmop(d, a, false, (u32)(s32)inst.SIMM_16, Add, &XEmitter::ADD, false, true); break; //addic
	case 13: regimmop(d, a, true, (u32)(s32)inst.SIMM_16, Add, &XEmitter::ADD, true, true); break; //addic_rc
	default:
		FALLBACK_IF(true);
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

	bool merge_branch = false;
	int test_crf = js.next_inst.BI >> 2;
	// Check if the next instruction is a branch - if it is, merge the two.
	if (((js.next_inst.OPCD == 16 /* bcx */) ||
		((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 528) /* bcctrx */) ||
		((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 16) /* bclrx */)) &&
		(js.next_inst.BO & BO_DONT_DECREMENT_FLAG) &&
		!(js.next_inst.BO & BO_DONT_CHECK_CONDITION)) {
			// Looks like a decent conditional branch that we can merge with.
			// It only test CR, not CTR.
			if (test_crf == crf) {
				merge_branch = true;
			}
	}

	OpArg comparand;
	bool signedCompare;
	if (inst.OPCD == 31) {
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
		u8 compareResult;
		if (signedCompare)
		{
			if ((s32)gpr.R(a).offset == (s32)comparand.offset)
				compareResult = 0x2;
			else if ((s32)gpr.R(a).offset > (s32)comparand.offset)
				compareResult = 0x4;
			else
				compareResult = 0x8;
		}
		else
		{
			if ((u32)gpr.R(a).offset == (u32)comparand.offset)
				compareResult = 0x2;
			else if ((u32)gpr.R(a).offset > (u32)comparand.offset)
				compareResult = 0x4;
			else
				compareResult = 0x8;
		}
		MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(compareResult));
		gpr.UnlockAll();

		if (merge_branch)
		{
			js.downcountAmount++;

			gpr.Flush();
			fpr.Flush();

			int test_bit = 8 >> (js.next_inst.BI & 3);
			u8 conditionResult = (js.next_inst.BO & BO_BRANCH_IF_TRUE) ? test_bit : 0;
			if ((compareResult & test_bit) == conditionResult)
			{
				if (js.next_inst.OPCD == 16) // bcx
				{
					if (js.next_inst.LK)
						MOV(32, M(&LR), Imm32(js.compilerPC + 4));

					u32 destination;
					if (js.next_inst.AA)
						destination = SignExt16(js.next_inst.BD << 2);
					else
						destination = js.next_compilerPC + SignExt16(js.next_inst.BD << 2);
					WriteExit(destination);
				}
				else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 528)) // bcctrx
				{
					if (js.next_inst.LK)
						MOV(32, M(&LR), Imm32(js.compilerPC + 4));
					MOV(32, R(EAX), M(&CTR));
					AND(32, R(EAX), Imm32(0xFFFFFFFC));
					WriteExitDestInEAX();
				}
				else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 16)) // bclrx
				{
					MOV(32, R(EAX), M(&LR));
					if (js.next_inst.LK)
						MOV(32, M(&LR), Imm32(js.compilerPC + 4));
					WriteExitDestInEAX();
				}
				else
				{
					PanicAlert("WTF invalid branch");
				}
			}
			else
			{
				if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
				{
					js.skipnext = true;
					WriteExit(js.next_compilerPC + 4);
				}
			}
		}
	}
	else
	{
		Gen::CCFlags less_than, greater_than;
		if (signedCompare)
		{
			less_than = CC_L;
			greater_than = CC_G;
		}
		else
		{
			less_than = CC_B;
			greater_than = CC_A;
		}

		if (gpr.R(a).IsImm() || (!gpr.R(a).IsSimpleReg() && !comparand.IsImm() && !comparand.IsSimpleReg()))
		{
			// Syntax for CMP is invalid with such arguments. We must load RA in a register.
			gpr.BindToRegister(a, true, false);
		}
		CMP(32, gpr.R(a), comparand);
		gpr.UnlockAll();

		if (!merge_branch)
		{
			// Keep the normal code separate for clarity.

			FixupBranch pLesser  = J_CC(less_than);
			FixupBranch pGreater = J_CC(greater_than);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x2)); // _x86Reg == 0
			FixupBranch continue1 = J();
			SetJumpTarget(pGreater);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x4)); // _x86Reg > 0
			FixupBranch continue2 = J();
			SetJumpTarget(pLesser);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x8)); // _x86Reg < 0
			SetJumpTarget(continue1);
			SetJumpTarget(continue2);
			// TODO: If we ever care about SO, borrow a trick from
			// http://maws.mameworld.info/maws/mamesrc/src/emu/cpu/powerpc/drc_ops.c : bt, adc
		}
		else
		{
			js.downcountAmount++;
			int test_bit = 8 >> (js.next_inst.BI & 3);
			bool condition = (js.next_inst.BO & BO_BRANCH_IF_TRUE) ? false : true;

			// Test swapping (in the future, will be used to inline across branches the right way)
			// if (rand() & 1)
			//     std::swap(destination1, destination2), condition = !condition;

			gpr.Flush();
			fpr.Flush();
			FixupBranch pLesser  = J_CC(less_than);
			FixupBranch pGreater = J_CC(greater_than);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x2));  //  == 0
			FixupBranch continue1 = J();

			SetJumpTarget(pGreater);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x4));  //  > 0
			FixupBranch continue2 = J();

			SetJumpTarget(pLesser);
			MOV(8, M(&PowerPC::ppcState.cr_fast[crf]), Imm8(0x8));  //  < 0
			FixupBranch continue3;
			if (!!(8 & test_bit) == condition) continue3 = J();
			if (!!(4 & test_bit) != condition) SetJumpTarget(continue2);
			if (!!(2 & test_bit) != condition) SetJumpTarget(continue1);
			if (js.next_inst.OPCD == 16) // bcx
			{
				if (js.next_inst.LK)
					MOV(32, M(&LR), Imm32(js.compilerPC + 4));

				u32 destination;
				if (js.next_inst.AA)
					destination = SignExt16(js.next_inst.BD << 2);
				else
					destination = js.next_compilerPC + SignExt16(js.next_inst.BD << 2);
				WriteExit(destination);
			}
			else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 528)) // bcctrx
			{
				if (js.next_inst.LK)
					MOV(32, M(&LR), Imm32(js.compilerPC + 4));
				MOV(32, R(EAX), M(&CTR));
				AND(32, R(EAX), Imm32(0xFFFFFFFC));
				WriteExitDestInEAX();
			}
			else if ((js.next_inst.OPCD == 19) && (js.next_inst.SUBOP10 == 16)) // bclrx
			{
				MOV(32, R(EAX), M(&LR));
				AND(32, R(EAX), Imm32(0xFFFFFFFC));
				if (js.next_inst.LK)
					MOV(32, M(&LR), Imm32(js.compilerPC + 4));
				WriteExitDestInEAX();
			}
			else
			{
				PanicAlert("WTF invalid branch");
			}

			if (!!(8 & test_bit) == condition) SetJumpTarget(continue3);
			if (!!(4 & test_bit) == condition) SetJumpTarget(continue2);
			if (!!(2 & test_bit) == condition) SetJumpTarget(continue1);

			if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
			{
				js.skipnext = true;
				WriteExit(js.next_compilerPC + 4);
			}
		}
	}

	gpr.UnlockAll();
}

void Jit64::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS, b = inst.RB;
	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 31, "Invalid boolX");

	if (gpr.R(s).IsImm() && gpr.R(b).IsImm())
	{
		if (inst.SUBOP10 == 28) /* andx */
			gpr.SetImmediate32(a, (u32)gpr.R(s).offset & (u32)gpr.R(b).offset);
		else if (inst.SUBOP10 == 476) /* nandx */
			gpr.SetImmediate32(a, ~((u32)gpr.R(s).offset & (u32)gpr.R(b).offset));
		else if (inst.SUBOP10 == 60) /* andcx */
			gpr.SetImmediate32(a, (u32)gpr.R(s).offset & (~(u32)gpr.R(b).offset));
		else if (inst.SUBOP10 == 444) /* orx */
			gpr.SetImmediate32(a, (u32)gpr.R(s).offset | (u32)gpr.R(b).offset);
		else if (inst.SUBOP10 == 124) /* norx */
			gpr.SetImmediate32(a, ~((u32)gpr.R(s).offset | (u32)gpr.R(b).offset));
		else if (inst.SUBOP10 == 412) /* orcx */
			gpr.SetImmediate32(a, (u32)gpr.R(s).offset | (~(u32)gpr.R(b).offset));
		else if (inst.SUBOP10 == 316) /* xorx */
			gpr.SetImmediate32(a, (u32)gpr.R(s).offset ^ (u32)gpr.R(b).offset);
		else if (inst.SUBOP10 == 284) /* eqvx */
			gpr.SetImmediate32(a, ~((u32)gpr.R(s).offset ^ (u32)gpr.R(b).offset));
		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
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
				gpr.UnlockAll();
			}
		}
		else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
		{
			if (a != s)
			{
				gpr.Lock(a,s);
				gpr.BindToRegister(a, false, true);
				MOV(32, gpr.R(a), gpr.R(s));
			}
			else
			{
				gpr.KillImmediate(a, true, true);
			}
			NOT(32, gpr.R(a));
			gpr.UnlockAll();
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
		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
	}
	else if ((a == s) || (a == b))
	{
		gpr.Lock(a,((a == s) ? b : s));
		OpArg operand = ((a == s) ? gpr.R(b) : gpr.R(s));
		gpr.BindToRegister(a, true, true);

		if (inst.SUBOP10 == 28) /* andx */
		{
			AND(32, gpr.R(a), operand);
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 476) /* nandx */
		{
			AND(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (inst.SUBOP10 == 60) /* andcx */
		{
			if (a == b)
			{
				NOT(32, gpr.R(a));
				AND(32, gpr.R(a), operand);
			}
			else
			{
				MOV(32, R(EAX), operand);
				NOT(32, R(EAX));
				AND(32, gpr.R(a), R(EAX));
			}
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 444) /* orx */
		{
			OR(32, gpr.R(a), operand);
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 124) /* norx */
		{
			OR(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (inst.SUBOP10 == 412) /* orcx */
		{
			if (a == b)
			{
				NOT(32, gpr.R(a));
				OR(32, gpr.R(a), operand);
			}
			else
			{
				MOV(32, R(EAX), operand);
				NOT(32, R(EAX));
				OR(32, gpr.R(a), R(EAX));
			}
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 316) /* xorx */
		{
			XOR(32, gpr.R(a), operand);
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 284) /* eqvx */
		{
			NOT(32, gpr.R(a));
			XOR(32, gpr.R(a), operand);
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else
		{
			PanicAlert("WTF");
		}
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a,s,b);
		gpr.BindToRegister(a, false, true);

		if (inst.SUBOP10 == 28) /* andx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), gpr.R(b));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 476) /* nandx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (inst.SUBOP10 == 60) /* andcx */
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			AND(32, gpr.R(a), gpr.R(s));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 444) /* orx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 124) /* norx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (inst.SUBOP10 == 412) /* orcx */
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			OR(32, gpr.R(a), gpr.R(s));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 316) /* xorx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			XOR(32, gpr.R(a), gpr.R(b));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SUBOP10 == 284) /* eqvx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			NOT(32, gpr.R(a));
			XOR(32, gpr.R(a), gpr.R(b));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else
		{
			PanicAlert("WTF!");
		}
		gpr.UnlockAll();
	}
}

void Jit64::extsbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;

	if (gpr.R(s).IsImm())
	{
		gpr.SetImmediate32(a, (u32)(s32)(s8)gpr.R(s).offset);
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s, true);
		// Always force moving to EAX because it isn't possible
		// to refer to the lowest byte of some registers, at least in
		// 32-bit mode.
		MOV(32, R(EAX), gpr.R(s));
		MOVSX(32, 8, gpr.RX(a), R(AL)); // watch out for ah and friends
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::extshx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, s = inst.RS;

	if (gpr.R(s).IsImm())
	{
		gpr.SetImmediate32(a, (u32)(s32)(s16)gpr.R(s).offset);
	}
	else
	{
		gpr.Lock(a, s);
		gpr.KillImmediate(s, true, false);
		gpr.BindToRegister(a, a == s, true);
		// This looks a little dangerous, but it's safe because
		// every 32-bit register has a 16-bit half at the same index
		// as the 32-bit register.
		MOVSX(32, 16, gpr.RX(a), gpr.R(s));
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
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
			JitClearCA();
			// Flags act exactly like subtracting from 0
			NEG(32, gpr.R(d));
			// Output carry is inverted
			FixupBranch carry1 = J_CC(CC_C);
			JitSetCA();
			SetJumpTarget(carry1);
		}
		else if (imm == -1)
		{
			// CA is always set in this case
			JitSetCA();
			NOT(32, gpr.R(d));
		}
		else
		{
			JitClearCA();
			NOT(32, gpr.R(d));
			ADD(32, gpr.R(d), Imm32(imm+1));
			// Output carry is normal
			FixupBranch carry1 = J_CC(CC_NC);
			JitSetCA();
			SetJumpTarget(carry1);
		}
	}
	else
	{
		JitClearCA();
		MOV(32, gpr.R(d), Imm32(imm));
		SUB(32, gpr.R(d), gpr.R(a));
		// Output carry is inverted
		FixupBranch carry1 = J_CC(CC_C);
		JitSetCA();
		SetJumpTarget(carry1);
	}
	gpr.UnlockAll();
	// This instruction has no RC flag
}

void Jit64::subfcx(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, (d == a || d == b), true);

	JitClearCAOV(inst.OE);
	if (d == b)
	{
		SUB(32, gpr.R(d), gpr.R(a));
	}
	else if (d == a)
	{
		MOV(32, R(EAX), gpr.R(a));
		MOV(32, gpr.R(d), gpr.R(b));
		SUB(32, gpr.R(d), R(EAX));
	}
	else
	{
		MOV(32, gpr.R(d), gpr.R(b));
		SUB(32, gpr.R(d), gpr.R(a));
	}
	if (inst.Rc) {
		GenerateRC();
	}
	FinalizeCarryOverflow(inst.OE, true);

	gpr.UnlockAll();
}

void Jit64::subfex(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, (d == a || d == b), true);

	GetCarryEAXAndClear();

	bool invertedCarry = false;
	if (d == b)
	{
		// Convert carry to borrow
		CMC();
		SBB(32, gpr.R(d), gpr.R(a));
		invertedCarry = true;
	}
	else if (d == a)
	{
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), gpr.R(b));
	}
	else
	{
		MOV(32, gpr.R(d), gpr.R(a));
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), gpr.R(b));
	}
	if (inst.Rc) {
		GenerateRC();
	}
	FinalizeCarryGenerateOverflowEAX(inst.OE, invertedCarry);

	gpr.UnlockAll();
}

void Jit64::subfmex(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;
	gpr.Lock(a, d);
	gpr.BindToRegister(d, d == a);

	GetCarryEAXAndClear();
	if (d != a)
	{
		MOV(32, gpr.R(d), gpr.R(a));
	}
	NOT(32, gpr.R(d));
	ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
	if (inst.Rc)
	{
		GenerateRC();
	}
	FinalizeCarryGenerateOverflowEAX(inst.OE);
	gpr.UnlockAll();
}

void Jit64::subfzex(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;

	gpr.Lock(a, d);
	gpr.BindToRegister(d, d == a);

	GetCarryEAXAndClear();
	if (d != a)
	{
		MOV(32, gpr.R(d), gpr.R(a));
	}
	NOT(32, gpr.R(d));
	ADC(32, gpr.R(d), Imm8(0));
	if (inst.Rc)
	{
		GenerateRC();
	}
	FinalizeCarryGenerateOverflowEAX(inst.OE);

	gpr.UnlockAll();
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
		if (inst.Rc)
		{
			ComputeRC(gpr.R(d));
		}
		if (inst.OE)
		{
			GenerateConstantOverflow((s64)(i - j) != (s64)i - (s64)j);
		}
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
			MOV(32, R(EAX), gpr.R(a));
			MOV(32, gpr.R(d), gpr.R(b));
			SUB(32, gpr.R(d), R(EAX));
		}
		else
		{
			MOV(32, gpr.R(d), gpr.R(b));
			SUB(32, gpr.R(d), gpr.R(a));
		}
		if (inst.Rc)
		{
			GenerateRC();
		}
		if (inst.OE)
		{
			GenerateOverflow();
		}
		gpr.UnlockAll();
	}
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
		if (imm == 0)
		{
			XOR(32, gpr.R(d), gpr.R(d));
		}
		else if (imm == (u32)-1)
		{
			if (d != a)
				MOV(32, gpr.R(d), gpr.R(a));
			NEG(32, gpr.R(d));
		}
		else if ((imm & (imm - 1)) == 0)
		{
			u32 shift = 0;
			if (imm & 0xFFFF0000) shift |= 16;
			if (imm & 0xFF00FF00) shift |= 8;
			if (imm & 0xF0F0F0F0) shift |= 4;
			if (imm & 0xCCCCCCCC) shift |= 2;
			if (imm & 0xAAAAAAAA) shift |= 1;
			if (d != a)
				MOV(32, gpr.R(d), gpr.R(a));
			if (shift)
				SHL(32, gpr.R(d), Imm8(shift));
		}
		else
		{
			IMUL(32, gpr.RX(d), gpr.R(a), Imm32(imm));
		}

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
		{
			GenerateConstantOverflow((s64)(i*j) != (s64)i * (s64)j);
		}
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		if (gpr.R(a).IsImm() || gpr.R(b).IsImm())
		{
			u32 imm = gpr.R(a).IsImm() ? (u32)gpr.R(a).offset : (u32)gpr.R(b).offset;
			int src = gpr.R(a).IsImm() ? b : a;
			if (imm == 0)
			{
				XOR(32, gpr.R(d), gpr.R(d));
			}
			else if (imm == (u32)-1)
			{
				if (d != src)
					MOV(32, gpr.R(d), gpr.R(src));
				NEG(32, gpr.R(d));
			}
			else if ((imm & (imm - 1)) == 0 && !inst.OE)
			{
				u32 shift = 0;
				if (imm & 0xFFFF0000) shift |= 16;
				if (imm & 0xFF00FF00) shift |= 8;
				if (imm & 0xF0F0F0F0) shift |= 4;
				if (imm & 0xCCCCCCCC) shift |= 2;
				if (imm & 0xAAAAAAAA) shift |= 1;
				if (d != src)
					MOV(32, gpr.R(d), gpr.R(src));
				if (shift)
					SHL(32, gpr.R(d), Imm8(shift));
			}
			else
			{
				IMUL(32, gpr.RX(d), gpr.R(src), Imm32(imm));
			}
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
		{
			GenerateOverflow();
		}
		gpr.UnlockAll();
	}
	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
}

void Jit64::mulhwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		gpr.SetImmediate32(d, (u32)(((u64)gpr.R(a).offset * (u64)gpr.R(b).offset) >> 32));
	}
	else
	{
		gpr.FlushLockX(EDX);
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		if (gpr.RX(d) == EDX)
			PanicAlert("mulhwux : WTF");
		MOV(32, R(EAX), gpr.R(a));
		gpr.KillImmediate(b, true, false);
		MUL(32, gpr.R(b));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		MOV(32, gpr.R(d), R(EDX));
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
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
			{
				GenerateConstantOverflow(true);
			}
		}
		else
		{
			gpr.SetImmediate32(d, (u32)gpr.R(a).offset / (u32)gpr.R(b).offset);
			if (inst.OE)
			{
				GenerateConstantOverflow(false);
			}
		}
	}
	else if (gpr.R(b).IsImm())
	{
		u32 divisor = (u32)gpr.R(b).offset;
		if (divisor == 0)
		{
			gpr.SetImmediate32(d, 0);
			if (inst.OE)
			{
				GenerateConstantOverflow(true);
			}
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
#if _M_X86_64
					gpr.Lock(a, b, d);
					gpr.BindToRegister(d, d == a, true);
					MOV(32, R(EAX), Imm32(magic));
					if (d != a)
						MOV(32, gpr.R(d), gpr.R(a));
					IMUL(64, gpr.RX(d), R(RAX));
					ADD(64, gpr.R(d), R(RAX));
					SHR(64, gpr.R(d), Imm8(shift+32));
#else
					gpr.FlushLockX(EDX);
					gpr.Lock(a, b, d);
					gpr.BindToRegister(d, d == a, true);
					MOV(32, R(EAX), Imm32(magic));
					MUL(32, gpr.R(a));
					XOR(32, gpr.R(d), gpr.R(d));
					ADD(32, R(EAX), Imm32(magic));
					ADC(32, gpr.R(d), R(EDX));
					if (shift)
						SHR(32, gpr.R(d), Imm8(shift));
					gpr.UnlockAllX();
#endif
				}
				else
				{
					// If success, use faster round-up method
#if _M_X86_64
					gpr.Lock(a, b, d);
					gpr.BindToRegister(a, true, false);
					gpr.BindToRegister(d, false, true);
					if (d == a)
					{
						MOV(32, R(EAX), Imm32(magic+1));
						IMUL(64, gpr.RX(d), R(RAX));
					}
					else
					{
						MOV(32, gpr.R(d), Imm32(magic+1));
						IMUL(64, gpr.RX(d), gpr.R(a));
					}
					SHR(64, gpr.R(d), Imm8(shift+32));
#else
					gpr.FlushLockX(EDX);
					gpr.Lock(a, b, d);
					gpr.BindToRegister(d, d == a, true);
					MOV(32, R(EAX), Imm32(magic+1));
					MUL(32, gpr.R(a));
					MOV(32, gpr.R(d), R(EDX));
					if (shift)
						SHR(32, gpr.R(d), Imm8(shift));
					gpr.UnlockAllX();
#endif
				}
			}
			if (inst.OE)
			{
				GenerateConstantOverflow(false);
			}
			gpr.UnlockAll();
		}
	}
	else
	{
		gpr.FlushLockX(EDX);
		gpr.Lock(a, b, d);
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
		//MOV(32, R(EAX), gpr.R(d));
		FixupBranch end = J();
		SetJumpTarget(not_div_by_zero);
		DIV(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		if (inst.OE)
		{
			GenerateConstantOverflow(false);
		}
		SetJumpTarget(end);
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
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
			{
				GenerateConstantOverflow(true);
			}
		}
		else
		{
			gpr.SetImmediate32(d, i / j);
			if (inst.OE)
			{
				GenerateConstantOverflow(false);
			}
		}
	}
	else
	{
		gpr.FlushLockX(EDX);
		gpr.Lock(a, b, d);
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
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
}

void Jit64::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		s32 i = (s32)gpr.R(a).offset, j = (s32)gpr.R(b).offset;
		gpr.SetImmediate32(d, i + j);
		if (inst.Rc)
		{
			ComputeRC(gpr.R(d));
		}
		if (inst.OE)
		{
			GenerateConstantOverflow((s64)(i + j) != (s64)i + (s64)j);
		}
	}
	else if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && !inst.Rc && !inst.OE)
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		LEA(32, gpr.RX(d), MComplex(gpr.RX(a), gpr.RX(b), 1, 0));
		gpr.UnlockAll();
	}
	else if ((d == a) || (d == b))
	{
		int operand = ((d == a) ? b : a);
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);
		ADD(32, gpr.R(d), gpr.R(operand));
		if (inst.Rc)
		{
			GenerateRC();
		}
		if (inst.OE)
		{
			GenerateOverflow();
		}
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), gpr.R(a));
		ADD(32, gpr.R(d), gpr.R(b));
		if (inst.Rc)
		{
			GenerateRC();
		}
		if (inst.OE)
		{
			GenerateOverflow();
		}
		gpr.UnlockAll();
	}
}

void Jit64::addex(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if ((d == a) || (d == b))
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);

		GetCarryEAXAndClear();
		ADC(32, gpr.R(d), gpr.R((d == a) ? b : a));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);

		GetCarryEAXAndClear();
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), gpr.R(b));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
}

void Jit64::addcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if ((d == a) || (d == b))
	{
		int operand = ((d == a) ? b : a);
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);
		JitClearCAOV(inst.OE);
		ADD(32, gpr.R(d), gpr.R(operand));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryOverflow(inst.OE);
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		JitClearCAOV(inst.OE);
		MOV(32, gpr.R(d), gpr.R(a));
		ADD(32, gpr.R(d), gpr.R(b));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryOverflow(inst.OE);
		gpr.UnlockAll();
	}
}

void Jit64::addmex(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;

	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);

		GetCarryEAXAndClear();
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);

		GetCarryEAXAndClear();
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
}

void Jit64::addzex(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA, d = inst.RD;

	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);

		GetCarryEAXAndClear();
		ADC(32, gpr.R(d), Imm8(0));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);

		GetCarryEAXAndClear();
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), Imm8(0));
		if (inst.Rc)
		{
			GenerateRC();
		}
		FinalizeCarryGenerateOverflowEAX(inst.OE);
		gpr.UnlockAll();
	}
}

void Jit64::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	if (gpr.R(s).IsImm())
	{
		unsigned result = (int)gpr.R(s).offset;
		if (inst.SH != 0)
			result = _rotl(result, inst.SH);
		result &= Helper_Mask(inst.MB, inst.ME);
		gpr.SetImmediate32(a, result);
		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s);
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}

		if (inst.SH && inst.MB == 0 && inst.ME==31-inst.SH)
		{
			SHL(32, gpr.R(a), Imm8(inst.SH));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else if (inst.SH && inst.ME == 31 && inst.MB == 32 - inst.SH)
		{
			SHR(32, gpr.R(a), Imm8(inst.MB));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else
		{
			if (inst.SH != 0)
			{
				ROL(32, gpr.R(a), Imm8(inst.SH));
			}
			if (!(inst.MB==0 && inst.ME==31))
			{
				AND(32, gpr.R(a), Imm32(Helper_Mask(inst.MB, inst.ME)));
				if (inst.Rc)
				{
					GenerateRC();
				}
			}
			else if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
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
		{
			ComputeRC(gpr.R(a));
		}
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, true, true);
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		if (mask == 0 || (a == s && inst.SH == 0))
		{
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (mask == 0xFFFFFFFF)
		{
			if (a != s)
			{
				MOV(32, gpr.R(a), gpr.R(s));
			}
			if (inst.SH)
			{
				ROL(32, gpr.R(a), Imm8(inst.SH));
			}
			if (inst.Rc)
			{
				ComputeRC(gpr.R(a));
			}
		}
		else if (inst.SH)
		{
			if (mask == 0U - (1U << inst.SH))
			{
				MOV(32, R(EAX), gpr.R(s));
				SHL(32, R(EAX), Imm8(inst.SH));
				AND(32, gpr.R(a), Imm32(~mask));
				OR(32, gpr.R(a), R(EAX));
			}
			else if (mask == (1U << inst.SH) - 1)
			{
				MOV(32, R(EAX), gpr.R(s));
				SHR(32, R(EAX), Imm8(32-inst.SH));
				AND(32, gpr.R(a), Imm32(~mask));
				OR(32, gpr.R(a), R(EAX));
			}
			else
			{
				MOV(32, R(EAX), gpr.R(s));
				ROL(32, R(EAX), Imm8(inst.SH));
				XOR(32, R(EAX), gpr.R(a));
				AND(32, R(EAX), Imm32(mask));
				XOR(32, gpr.R(a), R(EAX));
			}
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
		else
		{
			XOR(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), Imm32(~mask));
			XOR(32, gpr.R(a), gpr.R(s));
			if (inst.Rc)
			{
				GenerateRC();
			}
		}
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
		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
	}
	else
	{
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, (a == b || a == s), true);
		MOV(32, R(ECX), gpr.R(b));
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		ROL(32, gpr.R(a), R(ECX));
		AND(32, gpr.R(a), Imm32(mask));
		if (inst.Rc)
		{
			GenerateRC();
		}
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}
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
		if (inst.Rc)
		{
			ComputeRC(gpr.R(d));
		}
		if (inst.OE)
		{
			GenerateConstantOverflow(gpr.R(d).offset == 0x80000000);
		}
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, a == d, true);
		if (a != d)
			MOV(32, gpr.R(d), gpr.R(a));
		NEG(32, gpr.R(d));
		if (inst.Rc)
		{
			GenerateRC();
		}
		if (inst.OE)
		{
			GenerateOverflow();
		}
		gpr.UnlockAll();
	}
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
#if _M_X86_64
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, (a == b || a == s), true);
		MOV(32, R(ECX), gpr.R(b));
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		SHR(64, gpr.R(a), R(ECX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
#else
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, (a == b || a == s), true);
		MOV(32, R(ECX), gpr.R(b));
		TEST(32, R(ECX), Imm32(32));
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		FixupBranch branch = J_CC(CC_Z);
		XOR(32, gpr.R(a), gpr.R(a));
		SetJumpTarget(branch);
		SHR(32, gpr.R(a), R(ECX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
#endif
	}
	// Shift of 0 doesn't update flags, so compare manually just in case
	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
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
		{
			ComputeRC(gpr.R(a));
		}
	}
	else
	{
#if _M_X86_64
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, (a == b || a == s), true);
		MOV(32, R(ECX), gpr.R(b));
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		SHL(64, gpr.R(a), R(ECX));
		if (inst.Rc)
		{
			AND(32, gpr.R(a), gpr.R(a));
			GenerateRC();
		}
		else
		{
			MOVZX(64, 32, gpr.R(a).GetSimpleReg(), gpr.R(a));
		}
		gpr.UnlockAll();
		gpr.UnlockAllX();
#else
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, (a == b || a == s), true);
		MOV(32, R(ECX), gpr.R(b));
		TEST(32, R(ECX), Imm32(32));
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}
		FixupBranch branch = J_CC(CC_Z);
		XOR(32, gpr.R(a), gpr.R(a));
		SetJumpTarget(branch);
		SHL(32, gpr.R(a), R(ECX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		// Shift of 0 doesn't update flags, so compare manually just in case
		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
#endif
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
#if _M_X86_64
	gpr.Lock(a, s, b);
	gpr.FlushLockX(ECX);
	gpr.BindToRegister(a, (a == s || a == b), true);
	JitClearCA();
	MOV(32, R(ECX), gpr.R(b));
	if (a != s)
		MOV(32, gpr.R(a), gpr.R(s));
	SHL(64, gpr.R(a), Imm8(32));
	SAR(64, gpr.R(a), R(ECX));
	MOV(32, R(EAX), gpr.R(a));
	SHR(64, gpr.R(a), Imm8(32));
	TEST(32, gpr.R(a), R(EAX));
	FixupBranch nocarry = J_CC(CC_Z);
	JitSetCA();
	SetJumpTarget(nocarry);
	gpr.UnlockAll();
	gpr.UnlockAllX();
#else
	gpr.Lock(a, s, b);
	gpr.FlushLockX(ECX);
	gpr.BindToRegister(a, (a == s || a == b), true);
	JitClearCA();
	MOV(32, R(ECX), gpr.R(b));
	if (a != s)
		MOV(32, gpr.R(a), gpr.R(s));
	TEST(32, R(ECX), Imm32(32));
	FixupBranch topBitSet = J_CC(CC_NZ);
	XOR(32, R(EAX), R(EAX));
	SHRD(32, R(EAX), gpr.R(a), R(ECX));
	SAR(32, gpr.R(a), R(ECX));
	TEST(32, R(EAX), gpr.R(a));
	FixupBranch nocarry1 = J_CC(CC_Z);
	JitSetCA();
	FixupBranch end = J();
	SetJumpTarget(topBitSet);
	SAR(32, gpr.R(a), Imm8(31));
	FixupBranch nocarry2 = J_CC(CC_Z);
	JitSetCA();
	SetJumpTarget(end);
	SetJumpTarget(nocarry1);
	SetJumpTarget(nocarry2);
	gpr.UnlockAll();
	gpr.UnlockAllX();
#endif
	if (inst.Rc) {
		ComputeRC(gpr.R(a));
	}
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
		JitClearCA();
		MOV(32, R(EAX), gpr.R(s));
		if (a != s)
		{
			MOV(32, gpr.R(a), R(EAX));
		}
		SAR(32, gpr.R(a), Imm8(amount));
		if (inst.Rc)
		{
			GenerateRC();
		}
		SHL(32, R(EAX), Imm8(32-amount));
		TEST(32, R(EAX), gpr.R(a));
		FixupBranch nocarry = J_CC(CC_Z);
		JitSetCA();
		SetJumpTarget(nocarry);
		gpr.UnlockAll();
	}
	else
	{
		// FIXME
		FALLBACK_IF(true);

		gpr.Lock(a, s);
		JitClearCA();
		gpr.BindToRegister(a, a == s, true);

		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}

		if (inst.Rc)
		{
			ComputeRC(gpr.R(a));
		}
		gpr.UnlockAll();
	}
}

// count leading zeroes
void Jit64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.R(s).IsImm())
	{
		u32 mask = 0x80000000;
		u32 i = 0;
		for (; i < 32; i++, mask >>= 1)
			if ((u32)gpr.R(s).offset & mask)
				break;
		gpr.SetImmediate32(a, i);
	}
	else
	{
		gpr.Lock(a, s);
		gpr.KillImmediate(s, true, false);
		gpr.BindToRegister(a, (a == s), true);
		BSR(32, gpr.R(a).GetSimpleReg(), gpr.R(s));
		FixupBranch gotone = J_CC(CC_NZ);
		MOV(32, gpr.R(a), Imm32(63));
		SetJumpTarget(gotone);
		XOR(32, gpr.R(a), Imm8(0x1f));  // flip order
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
		// TODO: Check PPC manual too
	}
}

void Jit64::twx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	s32 a = inst.RA;

	if (inst.OPCD == 3) // twi
		CMP(32, gpr.R(a), gpr.R(inst.RB));
	else // tw
		CMP(32, gpr.R(a), Imm32((s32)(s16)inst.SIMM_16));

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
	OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_PROGRAM));

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);

	WriteExceptionExit();

	SetJumpTarget(dont_trap);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
		WriteExit(js.compilerPC + 4);
}
