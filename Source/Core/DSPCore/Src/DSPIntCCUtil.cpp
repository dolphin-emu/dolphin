// Copyright (C) 2003-2009 Dolphin Project.

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

#include "DSPIntCCUtil.h"
#include "DSPCore.h"
#include "DSPInterpreter.h"

namespace DSPInterpreter {

void Update_SR_Register64(s64 _Value)
{
	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;

	if (_Value < 0)
	{
		g_dsp.r[DSP_REG_SR] |= SR_SIGN;
	}

	if (_Value == 0)
	{
		g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
	}

	// weird
	if ((_Value >> 62) == 0)
	{	
		g_dsp.r[DSP_REG_SR] |= 0x20;
	}
}

void Update_SR_Register16(s16 _Value)
{
	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;

	if (_Value < 0)
	{
		g_dsp.r[DSP_REG_SR] |= SR_SIGN;
	}

	if (_Value == 0)
	{
		g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
	}

	// weird
	if ((_Value >> 14) == 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x20;
	}
}

void Update_SR_LZ(s64 value) {

	if (value == 0) 
	{
		g_dsp.r[DSP_REG_SR] |= SR_LOGIC_ZERO; 
	}
	else
	{
		g_dsp.r[DSP_REG_SR] &= ~SR_LOGIC_ZERO;
	}

}

int GetMultiplyModifier()
{
	if (g_dsp.r[DSP_REG_SR] & SR_MUL_MODIFY)
		return 1;
	else	
		return 2;
}

inline bool isCarry() {
	return (g_dsp.r[DSP_REG_SR] & SR_CARRY) ? true : false;
}
inline bool isSign() {
	return ((g_dsp.r[DSP_REG_SR] & SR_2) != (g_dsp.r[DSP_REG_SR] & SR_SIGN));
}
inline bool isZero() {
	return (g_dsp.r[DSP_REG_SR] & SR_ARITH_ZERO) ? true : false;
}

//see gdsp_registers.h for flags
bool CheckCondition(u8 _Condition)
{
	switch (_Condition & 0xf)
	{
	case 0x0: //NS - NOT SIGN
		return !isSign();
	case 0x1: // S - SIGN
		return isSign();
	case 0x2: // G - GREATER
		return !isSign() && !isZero();
	case 0x3: // LE - LESS EQUAL
		return isSign() || isZero();
	case 0x4: // NZ - NOT ZERO
		return !isZero();
	case 0x5: // Z - ZERO 
		return isZero();
	case 0x6: // L - LESS
		// Should be that once we set 0x01
		return !isCarry();
			//		if (isSign())
	case 0x7: // GE - GREATER EQUAL
		// Should be that once we set 0x01
		return isCarry();
			//		if (! isSign() || isZero())
	case 0xc: // LNZ  - LOGIC NOT ZERO
		return !(g_dsp.r[DSP_REG_SR] & SR_LOGIC_ZERO);
	case 0xd: // LZ - LOGIC ZERO
		return (g_dsp.r[DSP_REG_SR] & SR_LOGIC_ZERO) != 0;

	case 0xf: // Empty - always true.
		return true;
	default:
		ERROR_LOG(DSPLLE, "Unknown condition check: 0x%04x\n", _Condition & 0xf);
		return false;
	}
}

}  // namespace
