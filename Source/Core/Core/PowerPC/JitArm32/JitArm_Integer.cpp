// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

extern u32 Helper_Mask(u8 mb, u8 me);

// Assumes that Sign and Zero flags were set by the last operation. Preserves all flags and registers.
// Jit64 ComputerRC is signed
// JIT64 GenerateRC is unsigned
void JitArm::GenerateRC(int cr) {
	ARMReg rB = gpr.GetReg();

	MOV(rB, 0x4); // Result > 0
	SetCC(CC_EQ); MOV(rB, 0x2); // Result == 0
	SetCC(CC_MI); MOV(rB, 0x8); // Result < 0
	SetCC();

	STRB(rB, R9, PPCSTATE_OFF(cr_fast) + cr);
	gpr.Unlock(rB);
}
void JitArm::ComputeRC(int cr) {
	ARMReg rB = gpr.GetReg();

	MOV(rB, 0x2); // Result == 0
	SetCC(CC_LT); MOV(rB, 0x8); // Result < 0
	SetCC(CC_GT); MOV(rB, 0x4); // Result > 0
	SetCC();

	STRB(rB, R9, PPCSTATE_OFF(cr_fast) + cr);
	gpr.Unlock(rB);
}
void JitArm::ComputeRC(s32 value, int cr) {
	ARMReg rB = gpr.GetReg();

	if (value < 0)
		MOV(rB, 0x8);
	else if (value > 0)
		MOV(rB, 0x4);
	else
		MOV(rB, 0x2);

	STRB(rB, R9, PPCSTATE_OFF(cr_fast) + cr);
	gpr.Unlock(rB);
}

void JitArm::ComputeCarry()
{
	ARMReg tmp = gpr.GetReg();
	Operand2 mask = Operand2(2, 2); // XER_CA_MASK
	LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	SetCC(CC_CS);
	ORR(tmp, tmp, mask);
	SetCC(CC_CC);
	BIC(tmp, tmp, mask);
	SetCC();
	STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	gpr.Unlock(tmp);
}
void JitArm::ComputeCarry(bool Carry)
{
	ARMReg tmp = gpr.GetReg();
	Operand2 mask = Operand2(2, 2); // XER_CA_MASK
	LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	if (Carry)
		ORR(tmp, tmp, mask);
	else
		BIC(tmp, tmp, mask);
	STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	gpr.Unlock(tmp);
}

void JitArm::GetCarryAndClear(ARMReg reg)
{
	ARMReg tmp = gpr.GetReg();
	Operand2 mask = Operand2(2, 2); // XER_CA_MASK
	LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	AND(reg, tmp, mask);
	BIC(tmp, tmp, mask);
	STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	gpr.Unlock(tmp);
}

void JitArm::FinalizeCarry(ARMReg reg)
{
	ARMReg tmp = gpr.GetReg();
	Operand2 mask = Operand2(2, 2); // XER_CA_MASK
	SetCC(CC_CS);
	ORR(reg, reg, mask);
	SetCC();
	LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	ORR(tmp, tmp, reg);
	STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
	gpr.Unlock(tmp);
}
// Wrong - prevents WW from loading in to a game and also inverted intro logos
void JitArm::subfic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	// FIXME
	FallBackToInterpreter(inst);
	return;

	int a = inst.RA, d = inst.RD;

	int imm = inst.SIMM_16;
	if (d == a)
	{
		if (imm == 0)
		{
			ARMReg tmp = gpr.GetReg();
			Operand2 mask = Operand2(2, 2); // XER_CA_MASK
			LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			BIC(tmp, tmp, mask);
			// Flags act exactly like subtracting from 0
			RSBS(gpr.R(d), gpr.R(d), 0);
			// Output carry is inverted
			SetCC(CC_CC);
				ORR(tmp, tmp, mask);
			SetCC();
			STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			gpr.Unlock(tmp);
		}
		else if (imm == -1)
		{
			// CA is always set in this case
			ARMReg tmp = gpr.GetReg();
			Operand2 mask = Operand2(2, 2); // XER_CA_MASK
			LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			ORR(tmp, tmp, mask);
			STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			gpr.Unlock(tmp);

			MVN(gpr.R(d), gpr.R(d));
		}
		else
		{
			ARMReg tmp = gpr.GetReg();
			ARMReg rA = gpr.GetReg();
			Operand2 mask = Operand2(2, 2); // XER_CA_MASK
			MOVI2R(rA, imm + 1);
			LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			BIC(tmp, tmp, mask);
			// Flags act exactly like subtracting from 0
			MVN(gpr.R(d), gpr.R(d));
			ADDS(gpr.R(d), gpr.R(d), rA);
			// Output carry is inverted
			SetCC(CC_CS);
				ORR(tmp, tmp, mask);
			SetCC();
			STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
			gpr.Unlock(tmp, rA);
		}
	}
	else
	{
		ARMReg tmp = gpr.GetReg();
		Operand2 mask = Operand2(2, 2); // XER_CA_MASK
		MOVI2R(gpr.R(d), imm);
		LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		BIC(tmp, tmp, mask);
		// Flags act exactly like subtracting from 0
		SUBS(gpr.R(d), gpr.R(d), gpr.R(a));
		// Output carry is inverted
		SetCC(CC_CC);
			ORR(tmp, tmp, mask);
		SetCC();
		STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		gpr.Unlock(tmp);
	}
	// This instruction has no RC flag
}

u32 Add(u32 a, u32 b) {return a + b;}
u32 Sub(u32 a, u32 b) {return a - b;}
u32 Mul(u32 a, u32 b) {return a * b;}
u32 Or (u32 a, u32 b) {return a | b;}
u32 And(u32 a, u32 b) {return a & b;}
u32 Xor(u32 a, u32 b) {return a ^ b;}

void JitArm::arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	u32 a = inst.RA, b = inst.RB, d = inst.RD, s = inst.RS;
	ARMReg RA, RB, RD, RS;
	bool isImm[2] = {false, false}; // Arg1 & Arg2
	u32 Imm[2] = {0, 0};
	bool Rc = false;
	bool carry = false;
	bool isUnsigned = false;
	bool shiftedImm = false;

	switch (inst.OPCD)
	{
		case 7: // mulli
			if (gpr.IsImm(a))
			{
				isImm[0] = true;
				Imm[0] = gpr.GetImm(a);
			}
			isImm[1] = true;
			Imm[1] = inst.SIMM_16;
		break;
		case 13: // addic_rc
			Rc = true;
		case 12: // addic
			if (gpr.IsImm(a))
			{
				isImm[0] = true;
				Imm[0] = gpr.GetImm(a);
			}
			isImm[1] = true;
			Imm[1] = inst.SIMM_16;
			carry = true;
		break;
		case 15: // addis
			shiftedImm = true;
		case 14: // addi
			if (a)
			{
				if (gpr.IsImm(a))
				{
					isImm[0] = true;
					Imm[0] = gpr.GetImm(a);
				}
			}
			else
			{
				isImm[0] = true;
				Imm[0] = 0;
			}
			isImm[1] = true;
			Imm[1] = inst.SIMM_16 << (shiftedImm ? 16 : 0);
		break;
		case 25: // oris
			shiftedImm = true;
		case 24: // ori
			if (gpr.IsImm(s))
			{
				isImm[0] = true;
				Imm[0] = gpr.GetImm(s);
			}
			isImm[1] = true;
			Imm[1] = inst.UIMM << (shiftedImm ? 16 : 0);
		break;
		case 27: // xoris
			shiftedImm = true;
		case 26: // xori
			if (gpr.IsImm(s))
			{
				isImm[0] = true;
				Imm[0] = gpr.GetImm(s);
			}
			isImm[1] = true;
			Imm[1] = inst.UIMM << (shiftedImm ? 16 : 0);
		break;
		case 29: // andis_rc
			shiftedImm = true;
		case 28: // andi_rc
			if (gpr.IsImm(s))
			{
				isImm[0] = true;
				Imm[0] = gpr.GetImm(s);
			}
			isImm[1] = true;
			Imm[1] = inst.UIMM << (shiftedImm ? 16 : 0);
			Rc = true;
		break;

		case 31: // addcx, addx, subfx
			switch (inst.SUBOP10)
			{
				case 24: // slwx
				case 28: // andx
				case 60: // andcx
				case 124: // norx
				case 284: // eqvx
				case 316: // xorx
				case 412: // orcx
				case 444: // orx
				case 476: // nandx
				case 536: // srwx
				case 792: // srawx
					if (gpr.IsImm(s))
					{
						isImm[0] = true;
						Imm[0] = gpr.GetImm(s);
					}
					if (gpr.IsImm(b))
					{
						isImm[1] = true;
						Imm[1] = gpr.GetImm(b);
					}
					Rc = inst.Rc;
				break;

				case 10:  // addcx
				case 522: // addcox
					carry = true;
				case 40: // subfx
					isUnsigned = true;
				case 235: // mullwx
				case 266:
				case 747: // mullwox
				case 778: // both addx
					if (gpr.IsImm(a))
					{
						isImm[0] = true;
						Imm[0] = gpr.GetImm(a);
					}
					if (gpr.IsImm(b))
					{
						isImm[1] = true;
						Imm[1] = gpr.GetImm(b);
					}
					Rc = inst.Rc;
				break;
			}
		break;
		default:
			WARN_LOG(DYNA_REC, "Unknown OPCD %d with arith function", inst.OPCD);
			FallBackToInterpreter(inst);
			return;
		break;
	}
	if (isImm[0] && isImm[1]) // Immediate propagation
	{
		bool hasCarry = false;
		u32 dest = d;
		switch (inst.OPCD)
		{
			case 7:
				gpr.SetImmediate(d, Mul(Imm[0], Imm[1]));
			break;
			case 12:
			case 13:
				gpr.SetImmediate(d, Add(Imm[0], Imm[1]));
				hasCarry = Interpreter::Helper_Carry(Imm[0], Imm[1]);
			break;
			case 14:
			case 15:
				gpr.SetImmediate(d, Add(Imm[0], Imm[1]));
				hasCarry = Interpreter::Helper_Carry(Imm[0], Imm[1]);
			break;
			case 24:
			case 25:
				gpr.SetImmediate(a, Or(Imm[0], Imm[1]));
				dest = a;
			break;
			case 26:
			case 27:
				gpr.SetImmediate(a, Xor(Imm[0], Imm[1]));
				dest = a;
			break;
			case 28:
			case 29:
				gpr.SetImmediate(a, And(Imm[0], Imm[1]));
				dest = a;
			break;
			case 31: // addcx, addx, subfx
				switch (inst.SUBOP10)
				{
					case 24:
						gpr.SetImmediate(a, Imm[0] << Imm[1]);
						dest = a;
					break;
					case 28:
						gpr.SetImmediate(a, And(Imm[0], Imm[1]));
						dest = a;
					break;
					case 40: // subfx
						gpr.SetImmediate(d, Sub(Imm[1], Imm[0]));
					break;
					case 60:
						gpr.SetImmediate(a, And(Imm[1], ~Imm[0]));
						dest = a;
					break;
					case 124:
						gpr.SetImmediate(a, ~Or(Imm[0], Imm[1]));
						dest = a;
					break;
					case 747:
					case 235:
						gpr.SetImmediate(d, Mul(Imm[0], Imm[1]));
					break;
					case 284:
						gpr.SetImmediate(a, ~Xor(Imm[0], Imm[1]));
						dest = a;
					break;
					case 316:
						gpr.SetImmediate(a, Xor(Imm[0], Imm[1]));
						dest = a;
					break;
					case 412:
						gpr.SetImmediate(a, Or(Imm[0], ~Imm[1]));
						dest = a;
					break;
					case 444:
						gpr.SetImmediate(a, Or(Imm[0], Imm[1]));
						dest = a;
					break;
					case 476:
						gpr.SetImmediate(a, ~And(Imm[1], Imm[0]));
						dest = a;
					break;
					case 536:
						gpr.SetImmediate(a, Imm[0] >> Imm[1]);
						dest = a;
					break;
					case 792:
						gpr.SetImmediate(a, ((s32)Imm[0]) >> Imm[1]);
						dest = a;
					break;
					case 10: // addcx
					case 266:
					case 778: // both addx
						gpr.SetImmediate(d, Add(Imm[0], Imm[1]));
						hasCarry = Interpreter::Helper_Carry(Imm[0], Imm[1]);
					break;
				}
			break;
		}
		if (carry) ComputeCarry(hasCarry);
		if (Rc) ComputeRC(gpr.GetImm(dest), 0);
		return;
	}
	// One or the other isn't a IMM
	switch (inst.OPCD)
	{
		case 7:
		{
			ARMReg rA = gpr.GetReg();
			RD = gpr.R(d);
			RA = gpr.R(a);
			MOVI2R(rA, Imm[1]);
			MUL(RD, RA, rA);
			gpr.Unlock(rA);
		}
		break;
		case 12:
		case 13:
		{
			ARMReg rA = gpr.GetReg();
			RD = gpr.R(d);
			RA = gpr.R(a);
			MOVI2R(rA, Imm[1]);
			ADDS(RD, RA, rA);
			gpr.Unlock(rA);
		}
		break;
		case 14:
		case 15: // Arg2 is always Imm
			if (!isImm[0])
			{
				ARMReg rA = gpr.GetReg();
				RD = gpr.R(d);
				RA = gpr.R(a);
				MOVI2R(rA, Imm[1]);
				ADD(RD, RA, rA);
				gpr.Unlock(rA);
			}
			else
				gpr.SetImmediate(d, Imm[1]);
		break;
		case 24:
		case 25:
		{
			ARMReg rA = gpr.GetReg();
			RS = gpr.R(s);
			RA = gpr.R(a);
			MOVI2R(rA, Imm[1]);
			ORR(RA, RS, rA);
			gpr.Unlock(rA);
		}
		break;
		case 26:
		case 27:
		{
			ARMReg rA = gpr.GetReg();
			RS = gpr.R(s);
			RA = gpr.R(a);
			MOVI2R(rA, Imm[1]);
			EOR(RA, RS, rA);
			gpr.Unlock(rA);
		}

		break;
		case 28:
		case 29:
		{
			ARMReg rA = gpr.GetReg();
			RS = gpr.R(s);
			RA = gpr.R(a);
			MOVI2R(rA, Imm[1]);
			ANDS(RA, RS, rA);
			gpr.Unlock(rA);
		}
		break;
		case 31:
			switch (inst.SUBOP10)
			{
				case 24:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					LSLS(RA, RS, RB);
				break;
				case 28:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					ANDS(RA, RS, RB);
				break;
				case 40: // subfx
					RD = gpr.R(d);
					RA = gpr.R(a);
					RB = gpr.R(b);
					SUBS(RD, RB, RA);
				break;
				case 60:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					BICS(RA, RS, RB);
				break;
				case 124:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					ORR(RA, RS, RB);
					MVNS(RA, RA);
				break;
				case 747:
				case 235:
					RD = gpr.R(d);
					RA = gpr.R(a);
					RB = gpr.R(b);
					MULS(RD, RA, RB);
				break;
				case 284:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					EOR(RA, RS, RB);
					MVNS(RA, RA);
				break;
				case 316:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					EORS(RA, RS, RB);
				break;
				case 412:
				{
					ARMReg rA = gpr.GetReg();
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					MVN(rA, RB);
					ORRS(RA, RS, rA);
					gpr.Unlock(rA);
				}
				break;
				case 444:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					ORRS(RA, RS, RB);
				break;
				case 476:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					AND(RA, RS, RB);
					MVNS(RA, RA);
				break;
				case 536:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					LSRS(RA,  RS, RB);
				break;
				case 792:
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					ASRS(RA,  RS, RB);
				break;
				case 10: // addcx
				case 266:
				case 778: // both addx
					RD = gpr.R(d);
					RA = gpr.R(a);
					RB = gpr.R(b);
					ADDS(RD, RA, RB);
				break;
			}
		break;
	}
	if (carry) ComputeCarry();
	if (Rc) isUnsigned ? GenerateRC() : ComputeRC();
}

void JitArm::addex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	// FIXME
	FallBackToInterpreter(inst);
	return;

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg();
	GetCarryAndClear(rA);
	ADDS(RD, RA, RB);
	FinalizeCarry(rA);
	if (inst.Rc) ComputeRC();
	gpr.Unlock(rA);
}

void JitArm::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	u32 a = inst.RA, s = inst.RS;

	ARMReg RA = gpr.R(a);
	ARMReg RS = gpr.R(s);
	CLZ(RA, RS);
	if (inst.Rc)
	{
		CMP(RA, 0);
		ComputeRC();
	}
}

void JitArm::mulhwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg(false);
	UMULLS(rA, RD, RA, RB);
	if (inst.Rc) ComputeRC();
}

void JitArm::extshx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, (u32)(s32)(s16)gpr.GetImm(s));
		if (inst.Rc) ComputeRC(gpr.GetImm(a), 0);
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	SXTH(rA, rS);
	if (inst.Rc){
		CMP(rA, 0);
		ComputeRC();
	}
}
void JitArm::extsbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, (u32)(s32)(s8)gpr.GetImm(s));
		if (inst.Rc) ComputeRC(gpr.GetImm(a), 0);
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	SXTB(rA, rS);
	if (inst.Rc){
		CMP(rA, 0);
		ComputeRC();
	}
}
void JitArm::cmp (UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	int crf = inst.CRFD;
	u32 a = inst.RA, b = inst.RB;

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		ComputeRC((s32)gpr.GetImm(a) - (s32)gpr.GetImm(b), crf);
		return;
	}

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	CMP(RA, RB);

	ComputeRC(crf);
}
void JitArm::cmpi(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	u32 a = inst.RA;
	int crf = inst.CRFD;
	if (gpr.IsImm(a))
		ComputeRC((s32)gpr.GetImm(a) - inst.SIMM_16, crf);
	else
	{
		ARMReg RA = gpr.R(a);
		if (inst.SIMM_16 >= 0 && inst.SIMM_16 < 256)
			CMP(RA, inst.SIMM_16);
		else
		{
			ARMReg rA = gpr.GetReg();
			MOVI2R(rA, inst.SIMM_16);
			CMP(RA, rA);
			gpr.Unlock(rA);
		}
		ComputeRC(crf);
	}
}
void JitArm::cmpl(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg RB = gpr.R(inst.RB);
	ARMReg rA = gpr.GetReg();
	int crf = inst.CRFD;

	CMP(RA, RB);
	// Unsigned GenerateRC()

	MOV(rA, 0x2); // Result == 0
	SetCC(CC_LO); MOV(rA, 0x8); // Result < 0
	SetCC(CC_HI); MOV(rA, 0x4); // Result > 0
	SetCC();

	STRB(rA, R9, PPCSTATE_OFF(cr_fast) + crf);
	gpr.Unlock(rA);
}

void JitArm::cmpli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg rA = gpr.GetReg();
	int crf = inst.CRFD;
	u32 uimm = (u32)inst.UIMM;
	if (uimm < 256)
	{
		CMP(RA, uimm);
	}
	else
	{
		MOVI2R(rA, (u32)inst.UIMM);
		CMP(RA, rA);
	}
	// Unsigned GenerateRC()

	MOV(rA, 0x2); // Result == 0
	SetCC(CC_LO); MOV(rA, 0x8); // Result < 0
	SetCC(CC_HI); MOV(rA, 0x4); // Result > 0
	SetCC();

	STRB(rA, R9, PPCSTATE_OFF(cr_fast) + crf);
	gpr.Unlock(rA);
}

void JitArm::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	ARMReg RA = gpr.R(inst.RA);
	ARMReg RD = gpr.R(inst.RD);

	RSBS(RD, RA, 0);
	if (inst.Rc)
	{
		GenerateRC();
	}
	if (inst.OE)
	{
		BKPT(0x333);
		//GenerateOverflow();
	}
}
void JitArm::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	u32 mask = Helper_Mask(inst.MB,inst.ME);
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	MOVI2R(rA, mask);

	Operand2 Shift(RS, ST_ROR, 32 - inst.SH); // This rotates left, while ARM has only rotate right, so swap it.
	if (inst.Rc)
	{
		BIC (rB, RA, rA); // RA & ~mask
		AND (rA, rA, Shift);
		ORRS(RA, rB, rA);
		GenerateRC();
	}
	else
	{
		BIC (rB, RA, rA); // RA & ~mask
		AND (rA, rA, Shift);
		ORR(RA, rB, rA);
	}
	gpr.Unlock(rA, rB);
}

void JitArm::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	u32 mask = Helper_Mask(inst.MB,inst.ME);
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, mask);

	Operand2 Shift(RS, ST_ROR, 32 - inst.SH); // This rotates left, while ARM has only rotate right, so swap it.
	if (inst.Rc)
	{
		ANDS(RA, rA, Shift);
		GenerateRC();
	}
	else
		AND (RA, rA, Shift);
	gpr.Unlock(rA);

	//m_GPR[inst.RA] = _rotl(m_GPR[inst.RS],inst.SH) & mask;
}
void JitArm::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	u32 mask = Helper_Mask(inst.MB,inst.ME);
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg RB = gpr.R(inst.RB);
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	MOVI2R(rA, mask);

	// PPC rotates left, ARM rotates right. Swap it
	MOV(rB, 32);
	SUB(rB, rB, RB);

	Operand2 Shift(RS, ST_ROR, rB); // Register shifted register
	if (inst.Rc)
	{
		ANDS(RA, rA, Shift);
		GenerateRC();
	}
	else
		AND (RA, rA, Shift);
	gpr.Unlock(rA, rB);
}

void JitArm::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;

	if (amount != 0)
	{
		ARMReg RA = gpr.R(a);
		ARMReg RS = gpr.R(s);
		ARMReg tmp = gpr.GetReg();
		Operand2 mask = Operand2(2, 2); // XER_CA_MASK

		MOV(tmp, RS);
		ASRS(RA, RS, amount);
		if (inst.Rc)
			GenerateRC();
		LSL(tmp, tmp, 32 - amount);
		TST(tmp, RA);

		LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		BIC(tmp, tmp, mask);
		SetCC(CC_NEQ);
		ORR(tmp, tmp, mask);
		SetCC();
		STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		gpr.Unlock(tmp);
	}
	else
	{
		ARMReg RA = gpr.R(a);
		ARMReg RS = gpr.R(s);
		MOV(RA, RS);

		ARMReg tmp = gpr.GetReg();
		Operand2 mask = Operand2(2, 2); // XER_CA_MASK
		LDR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		BIC(tmp, tmp, mask);
		STR(tmp, R9, PPCSTATE_OFF(spr[SPR_XER]));
		gpr.Unlock(tmp);

	}
}

void JitArm::twx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff)

	s32 a = inst.RA;

	gpr.Flush();
	fpr.Flush();

	ARMReg RA = gpr.GetReg();
	ARMReg RB = gpr.GetReg();
	MOV(RA, inst.TO);

	if (inst.OPCD == 3) // twi
		CMP(gpr.R(a), gpr.R(inst.RB));
	else // tw
	{
		MOVI2R(RB, (s32)(s16)inst.SIMM_16);
		CMP(gpr.R(a), RB);
	}

	FixupBranch al = B_CC(CC_LT);
	FixupBranch ag = B_CC(CC_GT);
	FixupBranch ae = B_CC(CC_EQ);
	// FIXME: will never be reached. But also no known code uses it...
	FixupBranch ll = B_CC(CC_VC);
	FixupBranch lg = B_CC(CC_VS);

	SetJumpTarget(al);
	TST(RA, 16);
	FixupBranch exit1 = B_CC(CC_NEQ);
	FixupBranch take1 = B();
	SetJumpTarget(ag);
	TST(RA, 8);
	FixupBranch exit2 = B_CC(CC_NEQ);
	FixupBranch take2 = B();
	SetJumpTarget(ae);
	TST(RA, 4);
	FixupBranch exit3 = B_CC(CC_NEQ);
	FixupBranch take3 = B();
	SetJumpTarget(ll);
	TST(RA, 2);
	FixupBranch exit4 = B_CC(CC_NEQ);
	FixupBranch take4 = B();
	SetJumpTarget(lg);
	TST(RA, 1);
	FixupBranch exit5 = B_CC(CC_NEQ);
	FixupBranch take5 = B();

	SetJumpTarget(take1);
	SetJumpTarget(take2);
	SetJumpTarget(take3);
	SetJumpTarget(take4);
	SetJumpTarget(take5);

	LDR(RA, R9, PPCSTATE_OFF(Exceptions));
	MOVI2R(RB, EXCEPTION_PROGRAM); // XXX: Can be optimized
	ORR(RA, RA, RB);
	STR(RA, R9, PPCSTATE_OFF(Exceptions));
	WriteExceptionExit();

	SetJumpTarget(exit1);
	SetJumpTarget(exit2);
	SetJumpTarget(exit3);
	SetJumpTarget(exit4);
	SetJumpTarget(exit5);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
		WriteExit(js.compilerPC + 4);

	gpr.Unlock(RA, RB);
}
