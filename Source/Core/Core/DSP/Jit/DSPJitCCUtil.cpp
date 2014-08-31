// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/DSPEmitter.h"
#include "Core/DSP/DSPIntUtil.h" // Helper functions

using namespace Gen;

// In: RAX: s64 _Value
// Clobbers RDX
void DSPEmitter::Update_SR_Register(Gen::X64Reg val)
{
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	//	// 0x04
	//	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
	CMP(64, R(val), Imm8(0));
	FixupBranch notZero = J_CC(CC_NZ);
	OR(16, sr_reg, Imm16(SR_ARITH_ZERO));
	SetJumpTarget(notZero);

	//	// 0x08
	//	if (_Value < 0) g_dsp.r[DSP_REG_SR] |= SR_SIGN;
	CMP(64, R(val), Imm8(0));
	FixupBranch greaterThanEqual = J_CC(CC_GE);
	OR(16, sr_reg, Imm16(SR_SIGN));
	SetJumpTarget(greaterThanEqual);

	//	// 0x10
	//	if (_Value != (s32)_Value) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
	MOVSX(64, 32, RDX, R(val));
	CMP(64, R(RDX), R(val));
	FixupBranch noOverS32 = J_CC(CC_E);
	OR(16, sr_reg, Imm16(SR_OVER_S32));
	SetJumpTarget(noOverS32);

	//	// 0x20 - Checks if top bits of m are equal
	//	if (((_Value & 0xc0000000) == 0) || ((_Value & 0xc0000000) == 0xc0000000))
	AND(32, R(val), Imm32(0xc0000000));
	CMP(32, R(val), Imm32(0));
	FixupBranch zeroC = J_CC(CC_E);
	CMP(32, R(val), Imm32(0xc0000000));
	FixupBranch cC = J_CC(CC_NE);
	SetJumpTarget(zeroC);
	//		g_dsp.r[DSP_REG_SR] |= SR_TOP2BITS;
	OR(16, sr_reg, Imm16(SR_TOP2BITS));
	SetJumpTarget(cC);
	gpr.putReg(DSP_REG_SR);
}

// In: RAX: s64 _Value
// Clobbers RDX
void DSPEmitter::Update_SR_Register64(Gen::X64Reg val)
{
//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	AND(16, sr_reg, Imm16(~SR_CMP_MASK));
	gpr.putReg(DSP_REG_SR);
	Update_SR_Register(val);
}

// In: (val): s64 _Value
// In: (carry_ovfl): 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64_Carry(X64Reg val, X64Reg carry_ovfl)
{
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	AND(16, sr_reg, Imm16(~SR_CMP_MASK));

	CMP(64, R(carry_ovfl), R(val));

	// 0x01
	//	g_dsp.r[DSP_REG_SR] |= SR_CARRY;
	// Carry = (acc>res)
	FixupBranch noCarry = J_CC(CC_BE);
	OR(16, sr_reg, Imm16(SR_CARRY));
	SetJumpTarget(noCarry);

	// 0x02 and 0x80
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
	// Overflow = ((acc ^ res) & (ax ^ res)) < 0
	XOR(64, R(carry_ovfl), R(val));
	XOR(64, R(RDX), R(val));
	AND(64, R(carry_ovfl), R(RDX));
	CMP(64, R(carry_ovfl), Imm8(0));
	FixupBranch noOverflow = J_CC(CC_GE);
	OR(16, sr_reg, Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
	SetJumpTarget(noOverflow);

	gpr.putReg(DSP_REG_SR);
	Update_SR_Register(val);
}

// In: (val): s64 _Value
// In: (carry_ovfl): 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64_Carry2(X64Reg val, X64Reg carry_ovfl)
{
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	AND(16, sr_reg, Imm16(~SR_CMP_MASK));

	CMP(64, R(carry_ovfl), R(val));

	// 0x01
	//	g_dsp.r[DSP_REG_SR] |= SR_CARRY;
	// Carry2 = (acc>=res)
	FixupBranch noCarry2 = J_CC(CC_B);
	OR(16, sr_reg, Imm16(SR_CARRY));
	SetJumpTarget(noCarry2);

	// 0x02 and 0x80
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
	// Overflow = ((acc ^ res) & (ax ^ res)) < 0
	XOR(64, R(carry_ovfl), R(val));
	XOR(64, R(RDX), R(val));
	AND(64, R(carry_ovfl), R(RDX));
	CMP(64, R(carry_ovfl), Imm8(0));
	FixupBranch noOverflow = J_CC(CC_GE);
	OR(16, sr_reg, Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
	SetJumpTarget(noOverflow);
	gpr.putReg(DSP_REG_SR);

	Update_SR_Register();
}

// In: RAX: s64 _Value
// Clobbers RDX
void DSPEmitter::Update_SR_Register16(X64Reg val)
{
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	AND(16, sr_reg, Imm16(~SR_CMP_MASK));

	//	// 0x04
	//	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
	CMP(64, R(val), Imm8(0));
	FixupBranch notZero = J_CC(CC_NZ);
	OR(16, sr_reg, Imm16(SR_ARITH_ZERO));
	SetJumpTarget(notZero);

	//	// 0x08
	//	if (_Value < 0) g_dsp.r[DSP_REG_SR] |= SR_SIGN;
	CMP(64, R(val), Imm8(0));
	FixupBranch greaterThanEqual = J_CC(CC_GE);
	OR(16, sr_reg, Imm16(SR_SIGN));
	SetJumpTarget(greaterThanEqual);

	//	// 0x20 - Checks if top bits of m are equal
	//	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
	//AND(32, R(val), Imm32(0xc0000000));
	SHR(16, R(val), Imm8(14));
	CMP(16, R(val), Imm16(0));
	FixupBranch nZero = J_CC(CC_NE);
	OR(16, sr_reg, Imm16(SR_TOP2BITS));
	FixupBranch cC = J();
	SetJumpTarget(nZero);
	CMP(16, R(val), Imm16(3));
	FixupBranch notThree = J_CC(CC_NE);
	//		g_dsp.r[DSP_REG_SR] |= SR_TOP2BITS;
	OR(16, sr_reg, Imm16(SR_TOP2BITS));
	SetJumpTarget(notThree);
	SetJumpTarget(cC);
	gpr.putReg(DSP_REG_SR);
}

// In: RAX: s64 _Value
// Clobbers RDX
void DSPEmitter::Update_SR_Register16_OverS32(Gen::X64Reg val)
{
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	AND(16, sr_reg, Imm16(~SR_CMP_MASK));

	//	// 0x10
	//	if (_Value != (s32)_Value) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
	MOVSX(64, 32, RCX, R(val));
	CMP(64, R(RCX), R(val));
	FixupBranch noOverS32 = J_CC(CC_E);
	OR(16, sr_reg, Imm16(SR_OVER_S32));
	SetJumpTarget(noOverS32);

	gpr.putReg(DSP_REG_SR);
	//	// 0x20 - Checks if top bits of m are equal
	//	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
	//AND(32, R(val), Imm32(0xc0000000));
	Update_SR_Register16(val);
}
