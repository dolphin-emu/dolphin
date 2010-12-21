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

// Additional copyrights go to Duddie and Tratax (c) 2004


// HELPER FUNCTIONS

#include "../DSPIntUtil.h"
#include "../DSPEmitter.h"
#include "x64Emitter.h"
#include "ABI.h"
using namespace Gen;

// In: RAX: s64 _Value
// In: RCX: 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register(Gen::X64Reg val)
{
#ifdef _M_X64
	//	// 0x04
	//	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
	CMP(64, R(val), Imm8(0));
	FixupBranch notZero = J_CC(CC_NZ);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_ARITH_ZERO));
	SetJumpTarget(notZero);

	//	// 0x08
	//	if (_Value < 0) g_dsp.r[DSP_REG_SR] |= SR_SIGN;
	CMP(64, R(val), Imm8(0));
	FixupBranch greaterThanEqual = J_CC(CC_GE);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_SIGN));
	SetJumpTarget(greaterThanEqual);

	//	// 0x10
	//	if (_Value != (s32)_Value) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
	MOVSX(64, 32, RDX, R(val));
	CMP(64, R(RDX), R(val));
	FixupBranch noOverS32 = J_CC(CC_E);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_OVER_S32));
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
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_TOP2BITS));
	SetJumpTarget(cC);
#endif
}

// In: RAX: s64 _Value
// In: RCX: 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64(Gen::X64Reg val)
{
#ifdef _M_X64
//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	AND(16, MDisp(R11, DSP_REG_SR * 2), Imm16(~SR_CMP_MASK));
	Update_SR_Register(val);
#endif
}

// In: RAX: s64 _Value
// In: RCX: 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64_Carry(Gen::X64Reg val)
{	
#ifdef _M_X64
	//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	AND(16, MDisp(R11, DSP_REG_SR * 2), Imm16(~SR_CMP_MASK));

	CMP(64, R(RCX), R(val));

	// 0x01
	//	g_dsp.r[DSP_REG_SR] |= SR_CARRY;
	// Carry = (acc>res)
	FixupBranch noCarry = J_CC(CC_BE);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_CARRY));
	SetJumpTarget(noCarry);

	// 0x02 and 0x80
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
	// Overflow = ((acc ^ res) & (ax ^ res)) < 0
	XOR(64, R(RCX), R(val));
	XOR(64, R(RDX), R(val));
	AND(64, R(RCX), R(RDX));
	CMP(64, R(RCX), Imm8(0));
	FixupBranch noOverflow = J_CC(CC_GE);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
	SetJumpTarget(noOverflow);

	Update_SR_Register(val);
#endif
}

// In: RAX: s64 _Value
// In: RCX: 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64_Carry2(Gen::X64Reg val)
{
#ifdef _M_X64
	//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
	AND(16, MDisp(R11, DSP_REG_SR * 2), Imm16(~SR_CMP_MASK));

	CMP(64, R(RCX), R(val));

	// 0x01
	//	g_dsp.r[DSP_REG_SR] |= SR_CARRY;
	// Carry2 = (acc>=res)
	FixupBranch noCarry2 = J_CC(CC_B);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_CARRY));
	SetJumpTarget(noCarry2);

	// 0x02 and 0x80
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
	//	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
	// Overflow = ((acc ^ res) & (ax ^ res)) < 0
	XOR(64, R(RCX), R(val));
	XOR(64, R(RDX), R(val));
	AND(64, R(RCX), R(RDX));
	CMP(64, R(RCX), Imm8(0));
	FixupBranch noOverflow = J_CC(CC_GE);
	OR(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
	SetJumpTarget(noOverflow);

	Update_SR_Register();
#endif
}

//void DSPEmitter::Update_SR_Register16(s16 _Value, bool carry, bool overflow, bool overS32)
//{
//	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;

//	// 0x01
//	if (carry)
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_CARRY;
//	}

//	// 0x02 and 0x80
//	if (overflow)
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
//		g_dsp.r[DSP_REG_SR]  |= SR_OVERFLOW_STICKY; 
//	}

//	// 0x04
//	if (_Value == 0)
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
//	}

//	// 0x08 
//	if (_Value < 0)
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_SIGN;
//	}

//	// 0x10
//	if (overS32) 
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
//	}

//	// 0x20 - Checks if top bits of m are equal
//	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
//	{
//		g_dsp.r[DSP_REG_SR] |= SR_TOP2BITS;
//	}
//}

//void DSPEmitter::Update_SR_LZ(bool value) {

//	if (value == true) 
//		g_dsp.r[DSP_REG_SR] |= SR_LOGIC_ZERO; 
//	else
//		g_dsp.r[DSP_REG_SR] &= ~SR_LOGIC_ZERO;
//}

//inline int GetMultiplyModifier()
//{
//	return (g_dsp.r[DSP_REG_SR] & SR_MUL_MODIFY)?1:2;
//}

//inline bool isCarry() {
//	return (g_dsp.r[DSP_REG_SR] & SR_CARRY) ? true : false;
//}

//inline bool isOverflow() {
//	return (g_dsp.r[DSP_REG_SR] & SR_OVERFLOW) ? true : false;
//}

//inline bool isOverS32() {
//	return (g_dsp.r[DSP_REG_SR] & SR_OVER_S32) ? true : false;
//}

//inline bool isLess() {
//	return (!(g_dsp.r[DSP_REG_SR] & SR_OVERFLOW) != !(g_dsp.r[DSP_REG_SR] & SR_SIGN));
//}

//inline bool isZero() {
//	return (g_dsp.r[DSP_REG_SR] & SR_ARITH_ZERO) ? true : false;
//}

//inline bool isLogicZero() {
//	return (g_dsp.r[DSP_REG_SR] & SR_LOGIC_ZERO) ? true : false;
//}

//inline bool isConditionA() {
//	return (((g_dsp.r[DSP_REG_SR] & SR_OVER_S32) || (g_dsp.r[DSP_REG_SR] & SR_TOP2BITS)) && !(g_dsp.r[DSP_REG_SR] & SR_ARITH_ZERO)) ? true : false;
//}

//see DSPCore.h for flags
//bool CheckCondition(u8 _Condition)
//{
//	switch (_Condition & 0xf)
//	{
//	case 0xf: // Always true.
//		return true;
//	case 0x0: // GE - Greater Equal
//		return !isLess();
//	case 0x1: // L - Less
//		return isLess();
//	case 0x2: // G - Greater
//		return !isLess() && !isZero();
//	case 0x3: // LE - Less Equal
//		return isLess() || isZero();
//	case 0x4: // NZ - Not Zero
//		return !isZero();
//	case 0x5: // Z - Zero 
//		return isZero();
//	case 0x6: // NC - Not carry
//		return !isCarry();
//	case 0x7: // C - Carry 
//		return isCarry();
//	case 0x8: // ? - Not over s32
//		return !isOverS32();
//	case 0x9: // ? - Over s32
//		return isOverS32();
//	case 0xa: // ?
//		return isConditionA();
//	case 0xb: // ?
//		return !isConditionA();
//	case 0xc: // LNZ  - Logic Not Zero
//		return !isLogicZero();
//	case 0xd: // LZ - Logic Zero
//		return isLogicZero();
//	case 0xe: // 0 - Overflow
//		return isOverflow();
//	default:
//		return true;
//	}
//}
