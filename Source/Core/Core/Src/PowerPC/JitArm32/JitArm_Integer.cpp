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
#include "Common.h"
#include "Thunk.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "ArmEmitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitAsm.h"
extern u32 Helper_Mask(u8 mb, u8 me);
// ADDI and RLWINMX broken for now

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

void JitArm::addi(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 d = inst.RD, a = inst.RA;
	if (a)
	{
		if (gpr.IsImm(a))
		{
			gpr.SetImmediate(d, gpr.GetImm(a) + inst.SIMM_16);
			return;
		}
		ARMReg rA = gpr.GetReg(false);
		ARMReg RA = gpr.R(a);
		ARMReg RD = gpr.R(d);
		MOVI2R(rA, (u32)inst.SIMM_16);
		ADD(RD, RA, rA);
	}
	else
		gpr.SetImmediate(d, inst.SIMM_16);
}
void JitArm::addis(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 d = inst.RD, a = inst.RA;
	if (a)
	{
		if (gpr.IsImm(a))
		{
			gpr.SetImmediate(d, gpr.GetImm(a) + (inst.SIMM_16 << 16));
			return;
		}
		ARMReg rA = gpr.GetReg(false);
		ARMReg RA = gpr.R(a);
		ARMReg RD = gpr.R(d);
		MOVI2R(rA, inst.SIMM_16 << 16);
		ADD(RD, RA, rA);
	}
	else
		gpr.SetImmediate(d, inst.SIMM_16 << 16);
}
void JitArm::addx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	
	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		gpr.SetImmediate(d, gpr.GetImm(a) + gpr.GetImm(b));
		if (inst.Rc) ComputeRC(gpr.GetImm(d), 0); 
		return;
	}
	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ADDS(RD, RA, RB);
	if (inst.Rc) ComputeRC();
}

void JitArm::addcx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	
	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ADDS(RD, RA, RB);
	ComputeCarry();
	if (inst.Rc) ComputeRC();
}
void JitArm::addex(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	Default(inst); return;
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

void JitArm::subfx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 a = inst.RA, b = inst.RB, d = inst.RD;
	
	if (inst.OE) PanicAlert("OE: subfx");

	if (gpr.IsImm(a) && gpr.IsImm(b))
	{
		gpr.SetImmediate(d, gpr.GetImm(b) - gpr.GetImm(a));
		if (inst.Rc) ComputeRC(gpr.GetImm(d), 0); 
		return;
	}
	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	SUBS(RD, RB, RA);
	if (inst.Rc) GenerateRC();
}
void JitArm::mulli(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, d = inst.RD;

	if (gpr.IsImm(a))
	{
		gpr.SetImmediate(d, gpr.GetImm(a) * inst.SIMM_16);
		return;
	}
	ARMReg RA = gpr.R(a);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, inst.SIMM_16);
	MUL(RD, RA, rA);
	gpr.Unlock(rA);
}

void JitArm::mullwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	MULS(RD, RA, RB);
	if (inst.OE) PanicAlert("OE: mullwx");
	if (inst.Rc) ComputeRC();
}

void JitArm::mulhwux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 a = inst.RA, b = inst.RB, d = inst.RD;

	ARMReg RA = gpr.R(a);
	ARMReg RB = gpr.R(b);
	ARMReg RD = gpr.R(d);
	ARMReg rA = gpr.GetReg(false);
	UMULLS(rA, RD, RA, RB);
	if (inst.Rc) ComputeRC();
}

void JitArm::ori(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, s = inst.RS;
	
	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) | inst.UIMM);
		return;
	}
	ARMReg RA = gpr.R(a);
	ARMReg RS = gpr.R(s);
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, inst.UIMM);
	ORR(RA, RS, rA);
	gpr.Unlock(rA);
}
void JitArm::oris(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) | (inst.UIMM << 16));
		return;
	}
	ARMReg RA = gpr.R(a);
	ARMReg RS = gpr.R(s);
	ARMReg rA = gpr.GetReg();
	MOVI2R(rA, inst.UIMM << 16);
	ORR(RA, RS, rA);
	gpr.Unlock(rA);
}

void JitArm::orx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	u32 a = inst.RA, b = inst.RB, s = inst.RS;

	if (gpr.IsImm(b) && gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) | gpr.GetImm(b));
		if (inst.Rc) ComputeRC(gpr.GetImm(a), 0); 
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rB = gpr.R(b);
	ARMReg rS = gpr.R(s);
	ORRS(rA, rS, rB);
	if (inst.Rc)
		ComputeRC();
}

void JitArm::xorx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)

	u32 a = inst.RA, b = inst.RB, s = inst.RS;

	if (gpr.IsImm(b) && gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) ^ gpr.GetImm(b));
		if (inst.Rc) ComputeRC(gpr.GetImm(a), 0); 
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rB = gpr.R(b);
	ARMReg rS = gpr.R(s);
	EORS(rA, rS, rB);
	if (inst.Rc)
		ComputeRC();
}

void JitArm::andx(UGeckoInstruction inst)
{
	u32 a = inst.RA, b = inst.RB, s = inst.RS;

	if (gpr.IsImm(s) && gpr.IsImm(b))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) & gpr.GetImm(b));
		if (inst.Rc) ComputeRC(gpr.GetImm(a), 0); 
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rB = gpr.R(b);
	ARMReg rS = gpr.R(s);

	ANDS(rA, rS, rB);

	if (inst.Rc) ComputeRC();
}

void JitArm::andi_rc(UGeckoInstruction inst)
{
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) & inst.UIMM);
		ComputeRC(gpr.GetImm(a), 0); 
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	ARMReg RA = gpr.GetReg();

	MOVI2R(RA, inst.UIMM);
	ANDS(rA, rS, RA);

	ComputeRC();
	gpr.Unlock(RA);
}

void JitArm::andis_rc(UGeckoInstruction inst)
{
	u32 a = inst.RA, s = inst.RS;

	if (gpr.IsImm(s))
	{
		gpr.SetImmediate(a, gpr.GetImm(s) & ((u32)inst.UIMM << 16));
		ComputeRC(gpr.GetImm(a), 0); 
		return;
	}
	ARMReg rA = gpr.R(a);
	ARMReg rS = gpr.R(s);
	ARMReg RA = gpr.GetReg();

	MOVI2R(RA, (u32)inst.UIMM << 16);
	ANDS(rA, rS, RA);

	ComputeRC();
	gpr.Unlock(RA);
}

void JitArm::extshx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)

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
	JITDISABLE(Integer)
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
	JITDISABLE(Integer)

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
	JITDISABLE(Integer)

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
	JITDISABLE(Integer)

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
	JITDISABLE(Integer)

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
	JITDISABLE(Integer)

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
void JitArm::srawix(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Integer)
	int a = inst.RA;
	int s = inst.RS;
	int amount = inst.SH;
	if (amount != 0)
	{
		Default(inst); return;
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
		SetCC(CC_EQ);
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

