// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004


// HELPER FUNCTIONS

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

namespace DSPInterpreter {

void Update_SR_Register64(s64 _Value, bool carry, bool overflow)
{
	g_dsp.r.sr &= ~SR_CMP_MASK;

	// 0x01
	if (carry)
	{
		g_dsp.r.sr |= SR_CARRY;
	}

	// 0x02 and 0x80
	if (overflow)
	{
		g_dsp.r.sr |= SR_OVERFLOW;
		g_dsp.r.sr  |= SR_OVERFLOW_STICKY;
	}

	// 0x04
	if (_Value == 0)
	{
		g_dsp.r.sr |= SR_ARITH_ZERO;
	}

	// 0x08
	if (_Value < 0)
	{
		g_dsp.r.sr |= SR_SIGN;
	}

	// 0x10
	if (_Value != (s32)_Value)
	{
		g_dsp.r.sr |= SR_OVER_S32;
	}

	// 0x20 - Checks if top bits of m are equal
	if (((_Value & 0xc0000000) == 0) || ((_Value & 0xc0000000) == 0xc0000000))
	{
		g_dsp.r.sr |= SR_TOP2BITS;
	}
}


void Update_SR_Register16(s16 _Value, bool carry, bool overflow, bool overS32)
{
	g_dsp.r.sr &= ~SR_CMP_MASK;

	// 0x01
	if (carry)
	{
		g_dsp.r.sr |= SR_CARRY;
	}

	// 0x02 and 0x80
	if (overflow)
	{
		g_dsp.r.sr |= SR_OVERFLOW;
		g_dsp.r.sr  |= SR_OVERFLOW_STICKY;
	}

	// 0x04
	if (_Value == 0)
	{
		g_dsp.r.sr |= SR_ARITH_ZERO;
	}

	// 0x08
	if (_Value < 0)
	{
		g_dsp.r.sr |= SR_SIGN;
	}

	// 0x10
	if (overS32)
	{
		g_dsp.r.sr |= SR_OVER_S32;
	}

	// 0x20 - Checks if top bits of m are equal
	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
	{
		g_dsp.r.sr |= SR_TOP2BITS;
	}
}

void Update_SR_LZ(bool value)
{
	if (value == true)
		g_dsp.r.sr |= SR_LOGIC_ZERO;
	else
		g_dsp.r.sr &= ~SR_LOGIC_ZERO;
}

inline int GetMultiplyModifier()
{
	return (g_dsp.r.sr & SR_MUL_MODIFY)?1:2;
}

inline bool isCarry()
{
	return (g_dsp.r.sr & SR_CARRY) ? true : false;
}

inline bool isOverflow()
{
	return (g_dsp.r.sr & SR_OVERFLOW) ? true : false;
}

inline bool isOverS32()
{
	return (g_dsp.r.sr & SR_OVER_S32) ? true : false;
}

inline bool isLess()
{
	return (!(g_dsp.r.sr & SR_OVERFLOW) != !(g_dsp.r.sr & SR_SIGN));
}

inline bool isZero()
{
	return (g_dsp.r.sr & SR_ARITH_ZERO) ? true : false;
}

inline bool isLogicZero()
{
	return (g_dsp.r.sr & SR_LOGIC_ZERO) ? true : false;
}

inline bool isConditionA()
{
	return (((g_dsp.r.sr & SR_OVER_S32) || (g_dsp.r.sr & SR_TOP2BITS)) && !(g_dsp.r.sr & SR_ARITH_ZERO)) ? true : false;
}

//see DSPCore.h for flags
bool CheckCondition(u8 _Condition)
{
	switch (_Condition & 0xf)
	{
	case 0xf: // Always true.
		return true;
	case 0x0: // GE - Greater Equal
		return !isLess();
	case 0x1: // L - Less
		return isLess();
	case 0x2: // G - Greater
		return !isLess() && !isZero();
	case 0x3: // LE - Less Equal
		return isLess() || isZero();
	case 0x4: // NZ - Not Zero
		return !isZero();
	case 0x5: // Z - Zero
		return isZero();
	case 0x6: // NC - Not carry
		return !isCarry();
	case 0x7: // C - Carry
		return isCarry();
	case 0x8: // ? - Not over s32
		return !isOverS32();
	case 0x9: // ? - Over s32
		return isOverS32();
	case 0xa: // ?
		return isConditionA();
	case 0xb: // ?
		return !isConditionA();
	case 0xc: // LNZ  - Logic Not Zero
		return !isLogicZero();
	case 0xd: // LZ - Logic Zero
		return isLogicZero();
	case 0xe: // 0 - Overflow
		return isOverflow();
	default:
		return true;
	}
}

}  // namespace
