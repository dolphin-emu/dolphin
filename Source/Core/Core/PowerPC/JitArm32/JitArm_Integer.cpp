// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

using namespace ArmGen;

void JitArm::ComputeRC(ARMReg value, int cr)
{
	ARMReg rB = gpr.GetReg();

	Operand2 ASRReg(value, ST_ASR, 31);

	STR(value, R9, PPCSTATE_OFF(cr_val[cr]));
	MOV(rB, ASRReg);
	STR(rB, R9, PPCSTATE_OFF(cr_val[cr]) + sizeof(u32));

	gpr.Unlock(rB);
}

void JitArm::ComputeRC(s32 value, int cr)
{
	ARMReg rB = gpr.GetReg();

	Operand2 ASRReg(rB, ST_ASR, 31);

	MOVI2R(rB, value);
	STR(rB, R9, PPCSTATE_OFF(cr_val[cr]));
	MOV(rB, ASRReg);
	STR(rB, R9, PPCSTATE_OFF(cr_val[cr]) + sizeof(u32));

	gpr.Unlock(rB);
}

void JitArm::ComputeCarry()
{
	ARMReg tmp = gpr.GetReg();
	SetCC(CC_CS);
	ORR(tmp, tmp, 1);
	SetCC(CC_CC);
	BIC(tmp, tmp, 1);
	SetCC();
	STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	gpr.Unlock(tmp);
}

void JitArm::ComputeCarry(bool Carry)
{
	ARMReg tmp = gpr.GetReg();
	LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	if (Carry)
		ORR(tmp, tmp, 1);
	else
		BIC(tmp, tmp, 1);
	STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	gpr.Unlock(tmp);
}

void JitArm::GetCarryAndClear(ARMReg reg)
{
	ARMReg tmp = gpr.GetReg();
	LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	AND(reg, tmp, 1);
	BIC(tmp, tmp, 1);
	STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	gpr.Unlock(tmp);
}

void JitArm::FinalizeCarry(ARMReg reg)
{
	ARMReg tmp = gpr.GetReg();
	SetCC(CC_CS);
	ORR(reg, reg, 1);
	SetCC();
	LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	ORR(tmp, tmp, reg);
	STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
	gpr.Unlock(tmp);
}

void JitArm::subfic(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	int a = inst.RA, d = inst.RD;

	int imm = inst.SIMM_16;
	if (d == a)
	{
		if (imm == 0)
		{
			ARMReg tmp = gpr.GetReg();
			LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			BIC(tmp, tmp, 1);
			// Flags act exactly like subtracting from 0
			RSBS(gpr.R(d), gpr.R(d), 0);
			SetCC(CC_CS);
				ORR(tmp, tmp, 1);
			SetCC();
			STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			gpr.Unlock(tmp);
		}
		else if (imm == -1)
		{
			// CA is always set in this case
			ARMReg tmp = gpr.GetReg();
			LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			ORR(tmp, tmp, 1);
			STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			gpr.Unlock(tmp);

			MVN(gpr.R(d), gpr.R(d));
		}
		else
		{
			ARMReg tmp = gpr.GetReg();
			ARMReg rA = gpr.GetReg();
			MOVI2R(rA, imm + 1);
			LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			BIC(tmp, tmp, 1);
			// Flags act exactly like subtracting from 0
			MVN(gpr.R(d), gpr.R(d));
			ADDS(gpr.R(d), gpr.R(d), rA);
			// Output carry is inverted
			SetCC(CC_CS);
				ORR(tmp, tmp, 1);
			SetCC();
			STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
			gpr.Unlock(tmp, rA);
		}
	}
	else
	{
		ARMReg tmp = gpr.GetReg();
		MOVI2R(gpr.R(d), imm);
		LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		BIC(tmp, tmp, 1);
		// Flags act exactly like subtracting from 0
		SUBS(gpr.R(d), gpr.R(d), gpr.R(a));
		// Output carry is inverted
		SetCC(CC_CS);
			ORR(tmp, tmp, 1);
		SetCC();
		STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		gpr.Unlock(tmp);
	}
	// This instruction has no RC flag
}

static u32 Add(u32 a, u32 b)
{
	return a + b;
}

static u32 Sub(u32 a, u32 b)
{
	return a - b;
}

static u32 Mul(u32 a, u32 b)
{
	return a * b;
}

static u32 Or (u32 a, u32 b)
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

void JitArm::arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	u32 a = inst.RA, b = inst.RB, d = inst.RD, s = inst.RS;
	ARMReg RA, RB, RD, RS;
	bool isImm[2] = {false, false}; // Arg1 & Arg2
	u32 Imm[2] = {0, 0};
	bool Rc = false;
	bool carry = false;
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
			FALLBACK_IF(true);
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

		if (carry)
			ComputeCarry(hasCarry);

		if (Rc)
			ComputeRC(gpr.GetImm(dest), 0);

		return;
	}

	u32 dest = d;
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
			{
				gpr.SetImmediate(d, Imm[1]);
			}
		break;
		case 24:
		case 25:
		{
			dest = a;
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
			dest = a;
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
			dest = a;
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
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					LSLS(RA, RS, RB);
				break;
				case 28:
					dest = a;
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
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					BICS(RA, RS, RB);
				break;
				case 124:
					dest = a;
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
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					EOR(RA, RS, RB);
					MVNS(RA, RA);
				break;
				case 316:
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					EORS(RA, RS, RB);
				break;
				case 412:
				{
					dest = a;
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
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					ORRS(RA, RS, RB);
				break;
				case 476:
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					AND(RA, RS, RB);
					MVNS(RA, RA);
				break;
				case 536:
					dest = a;
					RA = gpr.R(a);
					RS = gpr.R(s);
					RB = gpr.R(b);
					LSRS(RA,  RS, RB);
				break;
				case 792:
					dest = a;
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

	if (carry)
		ComputeCarry();

	if (Rc)
		ComputeRC(gpr.R(dest));
}

void JitArm::addex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	// FIXME
	FALLBACK_IF(true);

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg();
	GetCarryAndClear(rA);
	ADDS(RD, RA, RB);
	FinalizeCarry(rA);

	if (inst.Rc)
		ComputeRC(RD);

	gpr.Unlock(rA);
}

void JitArm::cntlzwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA, s = inst.RS;

	ARMReg RA = gpr.R(a);
	ARMReg RS = gpr.R(s);
	CLZ(RA, RS);
	if (inst.Rc)
		ComputeRC(RA);
}

void JitArm::mulhwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg(false);
	UMULL(rA, RD, RA, RB);

	if (inst.Rc)
		ComputeRC(RD);
}

void JitArm::extshx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, (u32)(s32)(s16)gpr.GetImm(s));

		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);

		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	SXTH(rA, rS);
	if (inst.Rc)
		ComputeRC(rA);
}
void JitArm::extsbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, (u32)(s32)(s8)gpr.GetImm(s));

		if (inst.Rc)
			ComputeRC(gpr.GetImm(a), 0);

		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	SXTB(rA, rS);
	if (inst.Rc)
		ComputeRC(rA);
}
void JitArm::cmp (UGeckoInstruction inst)
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

	FALLBACK_IF(true);
}
void JitArm::cmpi(UGeckoInstruction inst)
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

	FALLBACK_IF(true);
}

void JitArm::negx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	ARMReg RA = gpr.R(inst.RA);
	ARMReg RD = gpr.R(inst.RD);

	RSB(RD, RA, 0);
	if (inst.Rc)
		ComputeRC(RD);

	if (inst.OE)
	{
		BKPT(0x333);
		//GenerateOverflow();
	}
}
void JitArm::rlwimix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	u32 mask = Helper_Mask(inst.MB,inst.ME);
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg rA = gpr.GetReg();
	ARMReg rB = gpr.GetReg();
	MOVI2R(rA, mask);

	Operand2 Shift(RS, ST_ROR, 32 - inst.SH); // This rotates left, while ARM has only rotate right, so swap it.
	BIC (rB, RA, rA); // RA & ~mask
	AND (rA, rA, Shift);
	ORR(RA, rB, rA);

	if (inst.Rc)
		ComputeRC(RA);
	gpr.Unlock(rA, rB);
}

void JitArm::rlwinmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	u32 mask = Helper_Mask(inst.MB,inst.ME);
	ARMReg RA = gpr.R(inst.RA);
	ARMReg RS = gpr.R(inst.RS);
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, mask);

	Operand2 Shift(RS, ST_ROR, 32 - inst.SH); // This rotates left, while ARM has only rotate right, so swap it.
	AND(RA, rA, Shift);

	if (inst.Rc)
		ComputeRC(RA);
	gpr.Unlock(rA);

	//m_GPR[inst.RA] = _rotl(m_GPR[inst.RS],inst.SH) & mask;
}
void JitArm::rlwnmx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

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
	AND(RA, rA, Shift);

	if (inst.Rc)
		ComputeRC(RA);
	gpr.Unlock(rA, rB);
}

void JitArm::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;

	if (amount != 0)
	{
		ARMReg RA = gpr.R(a);
		ARMReg RS = gpr.R(s);
		ARMReg tmp = gpr.GetReg();

		MOV(tmp, RS);
		ASR(RA, RS, amount);
		if (inst.Rc)
			ComputeRC(RA);
		LSL(tmp, tmp, 32 - amount);
		TST(tmp, RA);

		LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		BIC(tmp, tmp, 1);
		SetCC(CC_NEQ);
			ORR(tmp, tmp, 1);
		SetCC();
		STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		gpr.Unlock(tmp);
	}
	else
	{
		ARMReg RA = gpr.R(a);
		ARMReg RS = gpr.R(s);
		MOV(RA, RS);

		ARMReg tmp = gpr.GetReg();
		LDRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		BIC(tmp, tmp, 1);
		STRB(tmp, R9, PPCSTATE_OFF(xer_ca));
		gpr.Unlock(tmp);

	}
}

void JitArm::twx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITIntegerOff);

	s32 a = inst.RA;

	ARMReg RA = gpr.GetReg();
	ARMReg RB = gpr.GetReg();
	MOV(RA, inst.TO);

	if (inst.OPCD == 3) // twi
	{
		MOVI2R(RB, (s32)(s16)inst.SIMM_16);
		CMP(gpr.R(a), RB);
	}
	else // tw
	{
		CMP(gpr.R(a), gpr.R(inst.RB));
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

	gpr.Flush(FLUSH_MAINTAIN_STATE);
	fpr.Flush(FLUSH_MAINTAIN_STATE);

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
	{
		gpr.Flush();
		fpr.Flush();

		WriteExit(js.compilerPC + 4);
	}

	gpr.Unlock(RA, RB);
}
