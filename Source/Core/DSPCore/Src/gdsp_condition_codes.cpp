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

#include "gdsp_condition_codes.h"
#include "gdsp_interpreter.h"
#include "DSPCore.h"
#include "DSPInterpreter.h"

namespace DSPInterpreter {

void Update_SR_Register64(s64 _Value)
{
	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;

	if (_Value < 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x8;
	}

	if (_Value == 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x4;
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
		g_dsp.r[DSP_REG_SR] |= 0x8;
	}

	if (_Value == 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x4;
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
	return (g_dsp.r[DSP_REG_SR] & 0x01) ? true : false;
}
inline bool isSign() {
	return ((g_dsp.r[DSP_REG_SR] & 0x02) != (g_dsp.r[DSP_REG_SR] & 0x08));
}

inline bool isZero() {
	return (g_dsp.r[DSP_REG_SR] & 0x04) ? true : false;
}



//see gdsp_registers.h for flags
bool CheckCondition(u8 _Condition)
{
	bool taken = false;
	switch (_Condition & 0xf)
	{
	case 0x0: //NS - NOT SIGN
		if (! isSign())
			taken = true;
		break;
		
	case 0x1: // S - SIGN
		if (isSign())
			taken = true;
		break;
 
	case 0x2: // G - GREATER
		if (! isSign() && !isZero())
			taken = true;
		break;

	case 0x3: // LE - LESS EQUAL
		if (isSign() || isZero())
			taken = true;
		break;

	case 0x4: // NZ - NOT ZERO

		if (!isZero())
			taken = true;
		break;

	case 0x5: // Z - ZERO 

		if (isZero())
			taken = true;
		break;

	case 0x6: // L - LESS
		// Should be that once we set 0x01
		if (!isCarry())
			//		if (isSign())
			taken = true;
		break;

	case 0x7: // GE - GREATER EQUAL
		// Should be that once we set 0x01
		if (isCarry())
			//		if (! isSign() || isZero())
			taken = true;
		break;

	case 0xc: // LNZ  - LOGIC NOT ZERO

		if (!(g_dsp.r[DSP_REG_SR] & SR_LOGIC_ZERO))
			taken = true;
		break;

	case 0xd: // LZ - LOGIC ZERO

		if (g_dsp.r[DSP_REG_SR] & SR_LOGIC_ZERO)
			taken = true;
		break;

	case 0xf: // Empty
		taken = true;
		break;

	default:
		ERROR_LOG(DSPLLE, "Unknown condition check: 0x%04x\n", _Condition & 0xf);
		break;
	}

	return taken;
}

}  // namespace
