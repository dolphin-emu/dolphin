// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "../../Core.h" // include "Common.h", "CoreParameter.h", SCoreStartupParameter
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitAsm.h"

// Assumes that the flags were just set through an addition.
void Jit64::GenerateCarry() {
	// USES_XER
	FixupBranch pNoCarry = J_CC(CC_NC);
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK));
	FixupBranch pContinue = J();
	SetJumpTarget(pNoCarry);
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~(XER_CA_MASK)));
	SetJumpTarget(pContinue);
}

void Jit64::ComputeRC(const Gen::OpArg & arg) {
	if( arg.IsImm() )
	{
		s32 value = (s32)arg.offset;
		if( value < 0 )
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x8));
		else if( value > 0 )
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x4));
		else
			MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x2));
	}
	else
	{
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

u32 Add(u32 a, u32 b) {return a + b;}
u32 Or (u32 a, u32 b) {return a | b;}
u32 And(u32 a, u32 b) {return a & b;}
u32 Xor(u32 a, u32 b) {return a ^ b;}

void Jit64::regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc, bool carry)
{
	gpr.Lock(d, a);
	if (a || binary || carry)  // yeh nasty special case addic
	{
		if (gpr.R(a).IsImm() && !carry)
		{
			gpr.SetImmediate32(d, doop((u32)gpr.R(a).offset, value));
		}
		else if (a == d)
		{
			gpr.KillImmediate(d, true, true);
			(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			if (carry)
				GenerateCarry();
		}
		else
		{
			gpr.BindToRegister(d, false);
			MOV(32, gpr.R(d), gpr.R(a));
			(this->*op)(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
			if (carry)
				GenerateCarry();
		}
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
	{
		ComputeRC(gpr.R(d));
	}
	gpr.UnlockAll();
}

void Jit64::reg_imm(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 d = inst.RD, a = inst.RA, s = inst.RS;
	switch (inst.OPCD)
	{
	case 14:  // addi
		// occasionally used as MOV - emulate, with immediate propagation
		if (gpr.R(a).IsImm() && d != a && a != 0) {
			gpr.SetImmediate32(d, (u32)gpr.R(a).offset + (u32)(s32)(s16)inst.SIMM_16);
		} else if (inst.SIMM_16 == 0 && d != a && a != 0) {
			gpr.Lock(a, d);
			gpr.BindToRegister(d, false, true);
			MOV(32, gpr.R(d), gpr.R(a));
			gpr.UnlockAll();
		} else {
			regimmop(d, a, false, (u32)(s32)inst.SIMM_16,  Add, &XEmitter::ADD); //addi
		}
		break;
	case 15:
		if (a == 0) {	// lis
			// Merge with next instruction if loading a 32-bits immediate value (lis + addi, lis + ori)
			if (!js.isLastInstruction) {
				if ((js.next_inst.OPCD == 14) && (js.next_inst.RD == d) && (js.next_inst.RA == d)) {      // addi
					gpr.SetImmediate32(d, ((u32)inst.SIMM_16 << 16) + (u32)(s32)js.next_inst.SIMM_16);
					js.downcountAmount++;
					js.skipnext = true;
					break;
				}
				else if ((js.next_inst.OPCD == 24) && (js.next_inst.RA == d) && (js.next_inst.RS == d))	{ // ori
					gpr.SetImmediate32(d, ((u32)inst.SIMM_16 << 16) | (u32)js.next_inst.UIMM);
					js.downcountAmount++;
					js.skipnext = true;
					break;
				}
			}

			// Not merged
			regimmop(d, a, false, (u32)inst.SIMM_16 << 16, Add, &XEmitter::ADD);
		}
		else {	// addis
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
		Default(inst);
		break;
	}
}

void Jit64::cmpXX(UGeckoInstruction inst)
{	
	// USES_CR
	INSTRUCTION_START
	JITDISABLE(Integer)
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
	else {
		gpr.Lock(a);
		if (inst.OPCD == 10) {
			//cmpli
			comparand = Imm32((u32)inst.UIMM);
			signedCompare = false;
		} else if (inst.OPCD == 11) {
			//cmpi
			comparand = Imm32((u32)(s32)(s16)inst.UIMM);
			signedCompare = true;
		} else {
			signedCompare = false;	// silence compiler warning
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

			gpr.Flush(FLUSH_ALL);
			fpr.Flush(FLUSH_ALL);

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
					WriteExit(destination, 0);
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
				WriteExit(js.next_compilerPC + 4, 0);
			}

			js.cancel = true;
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
		} else {
			js.downcountAmount++;
			int test_bit = 8 >> (js.next_inst.BI & 3);
			bool condition = (js.next_inst.BO & BO_BRANCH_IF_TRUE) ? false : true;
			
			// Test swapping (in the future, will be used to inline across branches the right way)
			// if (rand() & 1)
			//     std::swap(destination1, destination2), condition = !condition;

			gpr.Flush(FLUSH_ALL);
			fpr.Flush(FLUSH_ALL);
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
				WriteExit(destination, 0);
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

			WriteExit(js.next_compilerPC + 4, 1);

			js.cancel = true;
		}
	}

	gpr.UnlockAll();
}

void Jit64::boolX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
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
	}
	else if ((a == s) || (a == b))
	{
		gpr.Lock(a,((a == s) ? b : s));
		OpArg operand = ((a == s) ? gpr.R(b) : gpr.R(s));
		gpr.BindToRegister(a, true, true);
		
		if (inst.SUBOP10 == 28) /* andx */
		{
			AND(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 476) /* nandx */
		{
			AND(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
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
		}
		else if (inst.SUBOP10 == 444) /* orx */
		{
			OR(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 124) /* norx */
		{
			OR(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
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
		}
		else if (inst.SUBOP10 == 316) /* xorx */
		{
			XOR(32, gpr.R(a), operand);
		}
		else if (inst.SUBOP10 == 284) /* eqvx */
		{
			XOR(32, gpr.R(a), operand);
			NOT(32, gpr.R(a));
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
		}
		else if (inst.SUBOP10 == 476) /* nandx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			AND(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
		}
		else if (inst.SUBOP10 == 60) /* andcx */
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			AND(32, gpr.R(a), gpr.R(s));
		}
		else if (inst.SUBOP10 == 444) /* orx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
		}
		else if (inst.SUBOP10 == 124) /* norx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			OR(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
		}
		else if (inst.SUBOP10 == 412) /* orcx */
		{
			MOV(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
			OR(32, gpr.R(a), gpr.R(s));
		}
		else if (inst.SUBOP10 == 316) /* xorx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			XOR(32, gpr.R(a), gpr.R(b));
		}
		else if (inst.SUBOP10 == 284) /* eqvx */
		{
			MOV(32, gpr.R(a), gpr.R(s));
			XOR(32, gpr.R(a), gpr.R(b));
			NOT(32, gpr.R(a));
		}
		else
		{
			PanicAlert("WTF!");
		}
		gpr.UnlockAll();
	}
	
	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::extsbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)
	int a = inst.RA, d = inst.RD;
	gpr.Lock(a, d);
	gpr.BindToRegister(d, a == d, true);
	int imm = inst.SIMM_16;
	MOV(32, R(EAX), gpr.R(a));
	NOT(32, R(EAX));
	ADD(32, R(EAX), Imm32(imm + 1));
	MOV(32, gpr.R(d), R(EAX));
	GenerateCarry();
	gpr.UnlockAll();
	// This instruction has no RC flag
}

void Jit64::subfcx(UGeckoInstruction inst) 
{
	INSTRUCTION_START;
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, (d == a || d == b), true);

	// For some reason, I could not get the jit versions of sub*
	// working with x86 sub...so we use the ~a + b + 1 method
	JitClearCA();
	MOV(32, R(EAX), gpr.R(a));
	NOT(32, R(EAX));
	ADD(32, R(EAX), gpr.R(b));
	FixupBranch carry1 = J_CC(CC_NC);
	JitSetCA();
	SetJumpTarget(carry1);
	ADD(32, R(EAX), Imm32(1));
	FixupBranch carry2 = J_CC(CC_NC);
	JitSetCA();
	SetJumpTarget(carry2);
	MOV(32, gpr.R(d), R(EAX));

	gpr.UnlockAll();
	if (inst.OE) PanicAlert("OE: subfcx");
	if (inst.Rc) {
		ComputeRC(R(EAX));
	}
}

void Jit64::subfex(UGeckoInstruction inst) 
{
	INSTRUCTION_START;
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.FlushLockX(ECX);
	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, (d == a || d == b), true);

	// Get CA
	MOV(32, R(ECX), M(&PowerPC::ppcState.spr[SPR_XER]));
	SHR(32, R(ECX), Imm8(29));
	AND(32, R(ECX), Imm32(1));
	// Don't need it anymore
	JitClearCA();

	// ~a + b
	MOV(32, R(EAX), gpr.R(a));
	NOT(32, R(EAX));
	ADD(32, R(EAX), gpr.R(b));
	FixupBranch carry1 = J_CC(CC_NC);
	JitSetCA();
	SetJumpTarget(carry1);

	// + CA
	ADD(32, R(EAX), R(ECX));
	FixupBranch carry2 = J_CC(CC_NC);
	JitSetCA();
	SetJumpTarget(carry2);

	MOV(32, gpr.R(d), R(EAX));

	gpr.UnlockAll();
	gpr.UnlockAllX();
	if (inst.OE) PanicAlert("OE: subfex");
	if (inst.Rc) {
		ComputeRC(R(EAX));
	}
}

void Jit64::subfmex(UGeckoInstruction inst)
{
    // USES_XER
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, d = inst.RD;
	
	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, gpr.R(d), gpr.R(a));
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
    if (inst.Rc)
    {
            ComputeRC(gpr.R(d));
    }
}

void Jit64::subfzex(UGeckoInstruction inst)
{
    // USES_XER
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, d = inst.RD;
	
	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), Imm8(0));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, gpr.R(d), gpr.R(a));
		NOT(32, gpr.R(d));
		ADC(32, gpr.R(d), Imm8(0));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
    if (inst.Rc)
    {
            ComputeRC(gpr.R(d));
    }
}

void Jit64::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		gpr.SetImmediate32(d, (u32)gpr.R(b).offset - (u32)gpr.R(a).offset);
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		MOV(32, R(EAX), gpr.R(b));
		SUB(32, R(EAX), gpr.R(a));
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
	}

	if (inst.OE) PanicAlert("OE: subfx");
	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
}

void Jit64::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA, d = inst.RD;

	if (gpr.R(a).IsImm())
	{
		gpr.SetImmediate32(d, (s32)gpr.R(a).offset * (s32)inst.SIMM_16);
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, (d == a), true);
		gpr.KillImmediate(a, true, false);
		IMUL(32, gpr.RX(d), gpr.R(a), Imm32((u32)(s32)inst.SIMM_16));
		gpr.UnlockAll();
	}
}

void Jit64::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		gpr.SetImmediate32(d, (s32)gpr.R(a).offset * (s32)gpr.R(b).offset);
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, (d == a || d == b), true);
		if (d == a) {
			IMUL(32, gpr.RX(d), gpr.R(b));
		} else if (d == b) {
			IMUL(32, gpr.RX(d), gpr.R(a));
		} else {
			MOV(32, gpr.R(d), gpr.R(b));
			IMUL(32, gpr.RX(d), gpr.R(a));
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
	JITDISABLE(Integer)
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
		ComputeRC(gpr.R(d));
}

void Jit64::divwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, d = inst.RD;

	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		if( gpr.R(b).offset == 0 )
			gpr.SetImmediate32(d, 0);
		else
			gpr.SetImmediate32(d, (u32)gpr.R(a).offset / (u32)gpr.R(b).offset);
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
		// doesn't handle if OE is set, but int doesn't either...
		FixupBranch not_div_by_zero = J_CC(CC_NZ);
		MOV(32, gpr.R(d), R(EDX));
		MOV(32, R(EAX), gpr.R(d));
		FixupBranch end = J();
		SetJumpTarget(not_div_by_zero);
		DIV(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		SetJumpTarget(end);
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
    JITDISABLE(Integer)
    int a = inst.RA, b = inst.RB, d = inst.RD;
	if (inst.OE)
		NOTICE_LOG(DYNA_REC, "Add - OE enabled :(");
	
	if (gpr.R(a).IsImm() && gpr.R(b).IsImm())
	{
		gpr.SetImmediate32(d, (u32)gpr.R(a).offset + (u32)gpr.R(b).offset);
	}
	else if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
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
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), gpr.R(a)); 
		ADD(32, gpr.R(d), gpr.R(b));
		gpr.UnlockAll();
	}
	
	if (inst.Rc)
	{
			ComputeRC(gpr.R(d));
	}
}

void Jit64::addex(UGeckoInstruction inst)
{
    // USES_XER
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, b = inst.RB, d = inst.RD;
	
	if ((d == a) || (d == b))
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		ADC(32, gpr.R(d), gpr.R((d == a) ? b : a));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), gpr.R(b));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
    if (inst.Rc)
    {
            ComputeRC(gpr.R(d));
    }
}

void Jit64::addcx(UGeckoInstruction inst)
{
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, b = inst.RB, d = inst.RD;
    _assert_msg_(DYNA_REC, !inst.OE, "Add - OE enabled :(");
	
	if ((d == a) || (d == b))
	{
		int operand = ((d == a) ? b : a);
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, true);
		ADD(32, gpr.R(d), gpr.R(operand));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, b, d);
		gpr.BindToRegister(d, false);
		MOV(32, gpr.R(d), gpr.R(a)); 
		ADD(32, gpr.R(d), gpr.R(b));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
	if (inst.Rc)
	{
			ComputeRC(gpr.R(d));
	}
}

void Jit64::addmex(UGeckoInstruction inst)
{
    // USES_XER
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, d = inst.RD;
	
	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), Imm32(0xFFFFFFFF));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
    if (inst.Rc)
    {
            ComputeRC(gpr.R(d));
    }
}

void Jit64::addzex(UGeckoInstruction inst)
{
    // USES_XER
    INSTRUCTION_START
    JITDISABLE(Integer)
    int a = inst.RA, d = inst.RD;
	
	if (d == a)
	{
		gpr.Lock(d);
		gpr.BindToRegister(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		ADC(32, gpr.R(d), Imm8(0));
		GenerateCarry();
		gpr.UnlockAll();
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, false);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, gpr.R(d), gpr.R(a));
		ADC(32, gpr.R(d), Imm8(0));
		GenerateCarry();
		gpr.UnlockAll();
	}
	
    if (inst.Rc)
    {
            ComputeRC(gpr.R(d));
    }
}

void Jit64::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int s = inst.RS;
	if (gpr.R(s).IsImm())
	{
		unsigned result = (int)gpr.R(s).offset;
		if (inst.SH != 0)
			result = _rotl(result, inst.SH);
		result &= Helper_Mask(inst.MB, inst.ME);
		gpr.SetImmediate32(a, result);
	}
	else
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s);
		if (a != s)
		{
			MOV(32, gpr.R(a), gpr.R(s));
		}

		if (inst.MB == 0 && inst.ME==31-inst.SH)
		{
			SHL(32, gpr.R(a), Imm8(inst.SH));
		}
		else if (inst.ME == 31 && inst.MB == 32 - inst.SH)
		{
			SHR(32, gpr.R(a), Imm8(inst.MB));
		}
		else
		{
			bool written = false;
			if (inst.SH != 0)
			{
				ROL(32, gpr.R(a), Imm8(inst.SH));
				written = true;
			}
			if (!(inst.MB==0 && inst.ME==31)) 
			{
				written = true;
				AND(32, gpr.R(a), Imm32(Helper_Mask(inst.MB, inst.ME)));
			}
			_assert_msg_(DYNA_REC, written, "W T F!!!");
		}
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
}


void Jit64::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int s = inst.RS;

	if (gpr.R(a).IsImm() && gpr.R(s).IsImm())
	{
		u32 mask = Helper_Mask(inst.MB,inst.ME);
		gpr.SetImmediate32(a, ((u32)gpr.R(a).offset & ~mask) | (_rotl((u32)gpr.R(s).offset,inst.SH) & mask));
	}
	else
	{
		gpr.Lock(a, s);
		gpr.KillImmediate(a, true, true);
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		MOV(32, R(EAX), gpr.R(s));
		AND(32, gpr.R(a), Imm32(~mask));
		if (inst.SH)
			ROL(32, R(EAX), Imm8(inst.SH));
		AND(32, R(EAX), Imm32(mask));
		OR(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA, b = inst.RB, s = inst.RS;
	
	u32 mask = Helper_Mask(inst.MB, inst.ME);
	if (gpr.R(b).IsImm() && gpr.R(s).IsImm())
	{
		gpr.SetImmediate32(a, _rotl((u32)gpr.R(s).offset, (u32)gpr.R(b).offset & 0x1F) & mask);
	}
	else
	{
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.KillImmediate(a, (a ==  s || a == b), true);
		MOV(32, R(EAX), gpr.R(s));
		MOV(32, R(ECX), gpr.R(b));
		AND(32, R(ECX), Imm32(0x1f));
		ROL(32, R(EAX), R(ECX));
		AND(32, R(EAX), Imm32(mask));
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int d = inst.RD;

	if (gpr.R(a).IsImm())
	{
		gpr.SetImmediate32(d, ~((u32)gpr.R(a).offset) + 1);
	}
	else
	{
		gpr.Lock(a, d);
		gpr.BindToRegister(d, a == d, true);
		if (a != d)
			MOV(32, gpr.R(d), gpr.R(a));
		NEG(32, gpr.R(d));
		gpr.UnlockAll();
	}

	if (inst.Rc)
	{
		ComputeRC(gpr.R(d));
	}
}

void Jit64::srwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
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
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, a == s || a == b || s == b, true);
		MOV(32, R(ECX), gpr.R(b));
		XOR(32, R(EAX), R(EAX));
		TEST(32, R(ECX), Imm32(32));
		FixupBranch branch = J_CC(CC_NZ);
		MOV(32, R(EAX), gpr.R(s));
		SHR(32, R(EAX), R(ECX));
		SetJumpTarget(branch);
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	if (inst.Rc) 
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::slwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;

	if (gpr.R(b).IsImm() && gpr.R(s).IsImm())
	{
		u32 amount = (u32)gpr.R(b).offset;
		gpr.SetImmediate32(a, (amount & 0x20) ? 0 : (u32)gpr.R(s).offset << amount);
	}
	else
	{
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.BindToRegister(a, a == s || a == b || s == b, true);
		MOV(32, R(ECX), gpr.R(b));
		XOR(32, R(EAX), R(EAX));
		TEST(32, R(ECX), Imm32(32));
		FixupBranch branch = J_CC(CC_NZ);
		MOV(32, R(EAX), gpr.R(s));
		SHL(32, R(EAX), R(ECX));
		SetJumpTarget(branch);
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	if (inst.Rc) 
	{
		ComputeRC(gpr.R(a));
	}
}

void Jit64::srawx(UGeckoInstruction inst)
{
	// USES_XER
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int b = inst.RB;
	int s = inst.RS;
	gpr.Lock(a, s, b);
	gpr.FlushLockX(ECX);
	gpr.BindToRegister(a, a == s || a == b, true);
	MOV(32, R(ECX), gpr.R(b));
	TEST(32, R(ECX), Imm32(32));
	FixupBranch topBitSet = J_CC(CC_NZ);
	if (a != s)
		MOV(32, gpr.R(a), gpr.R(s));
	MOV(32, R(EAX), Imm32(1));
	SHL(32, R(EAX), R(ECX));
	ADD(32, R(EAX), Imm32(0x7FFFFFFF));
	AND(32, R(EAX), gpr.R(a));
	ADD(32, R(EAX), Imm32(-1));
	CMP(32, R(EAX), Imm32(-1));
	SETcc(CC_L, R(EAX));
	SAR(32, gpr.R(a), R(ECX));
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~(1 << 29)));
	SHL(32, R(EAX), Imm8(29));
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), R(EAX));
	FixupBranch end = J();
	SetJumpTarget(topBitSet);
	MOV(32, R(EAX), gpr.R(s));
	SAR(32, R(EAX), Imm8(31));
	MOV(32, gpr.R(a), R(EAX));
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~(1 << 29)));
	AND(32, R(EAX), Imm32(1<<29));
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), R(EAX));
	SetJumpTarget(end);
	gpr.UnlockAll();
	gpr.UnlockAllX();

	if (inst.Rc) {
		ComputeRC(gpr.R(a));
	}
}

void Jit64::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;
	if (amount != 0)
	{
		gpr.Lock(a, s);
		gpr.BindToRegister(a, a == s, true);
		MOV(32, R(EAX), gpr.R(s));
		MOV(32, gpr.R(a), R(EAX));
		SAR(32, gpr.R(a), Imm8(amount));
		CMP(32, R(EAX), Imm8(0));
		FixupBranch nocarry1 = J_CC(CC_GE);
		TEST(32, R(EAX), Imm32((u32)0xFFFFFFFF >> (32 - amount))); // were any 1s shifted out?
		FixupBranch nocarry2 = J_CC(CC_Z);
		JitSetCA();
		FixupBranch carry = J(false);
		SetJumpTarget(nocarry1);
		SetJumpTarget(nocarry2);
		JitClearCA();
		SetJumpTarget(carry);
		gpr.UnlockAll();
	}
	else
	{
		Default(inst); return;
		gpr.Lock(a, s);
		JitClearCA();
		gpr.BindToRegister(a, a == s, true);
		if (a != s)
			MOV(32, gpr.R(a), gpr.R(s));
		gpr.UnlockAll();
	}

	if (inst.Rc) {
		ComputeRC(gpr.R(a));
	}
}

// count leading zeroes
void Jit64::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)

	s32 a = inst.RA;

	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);

	MOV(8, R(AL), Imm8(inst.TO));

	if (inst.OPCD == 3) // twi
		CMP(32, gpr.R(a), gpr.R(inst.RB));
	else // tw
		CMP(32, gpr.R(a), Imm32((s32)(s16)inst.SIMM_16));

	FixupBranch al = J_CC(CC_L);
	FixupBranch ag = J_CC(CC_G);
	FixupBranch ae = J_CC(CC_Z);
	// FIXME: will never be reached. But also no known code uses it...
	FixupBranch ll = J_CC(CC_NO);
	FixupBranch lg = J_CC(CC_O);

	SetJumpTarget(al);
	TEST(8, R(AL), Imm8(16));
	FixupBranch exit1 = J_CC(CC_NZ);
	FixupBranch take1 = J();
	SetJumpTarget(ag);
	TEST(8, R(AL), Imm8(8));
	FixupBranch exit2 = J_CC(CC_NZ);
	FixupBranch take2 = J();
	SetJumpTarget(ae);
	TEST(8, R(AL), Imm8(4));
	FixupBranch exit3 = J_CC(CC_NZ);
	FixupBranch take3 = J();
	SetJumpTarget(ll);
	TEST(8, R(AL), Imm8(2));
	FixupBranch exit4 = J_CC(CC_NZ);
	FixupBranch take4 = J();
	SetJumpTarget(lg);
	TEST(8, R(AL), Imm8(1));
	FixupBranch exit5 = J_CC(CC_NZ);
	FixupBranch take5 = J();

	SetJumpTarget(take1);
	SetJumpTarget(take2);
	SetJumpTarget(take3);
	SetJumpTarget(take4);
	SetJumpTarget(take5);
	LOCK();
	OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_PROGRAM));
	WriteExceptionExit();
	
	SetJumpTarget(exit1);
	SetJumpTarget(exit2);
	SetJumpTarget(exit3);
	SetJumpTarget(exit4);
	SetJumpTarget(exit5);
	WriteExit(js.compilerPC + 4, 1);
}
