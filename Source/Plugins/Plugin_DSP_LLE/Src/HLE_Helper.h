// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef HLE_HELPER_H
#define HLE_HELPER_H

#include "gdsp_interpreter.h"

bool WriteDMEM(uint16 addr, uint16 val);
uint16 ReadDMEM(uint16 addr);
void Update_SR_Register(sint64 _Value);
sint8 GetMultiplyModifier();

template<unsigned N> 
class TAccumulator
{
public:

	TAccumulator()
	{
		_assert_(N < 2);
	}

	void operator=(sint64 val)
	{
		setValue(val);
	}

	void operator<<=(sint64 value)
	{
		sint64 acc = getValue();
		acc <<= value;
		setValue(acc);
	}

	void operator>>=(sint64 value)
	{
		sint64 acc = getValue();
		acc >>= value;
		setValue(acc);
	}

	void operator+=(sint64 value)
	{
		sint64 acc = getValue();
		acc += value;
		setValue(acc);
	}

	operator sint64()
	{
		return getValue();
	}

	operator uint64()
	{
		return getValue();
	}

	inline void setValue(sint64 val)
	{
		g_dsp.r[0x1c + N] = (uint16)val;
		val >>= 16;
		g_dsp.r[0x1e + N] = (uint16)val;
		val >>= 16;
		g_dsp.r[0x10 + N] = (uint16)val;
	}

	inline sint64 getValue()
	{
		sint64 val;
		sint64 low_acc;
		val       = (sint8)g_dsp.r[0x10 + N];
		val     <<= 32;
		low_acc   = g_dsp.r[0x1e + N];
		low_acc <<= 16;
		low_acc  |= g_dsp.r[0x1c + N];
		val |= low_acc;
		return(val);
	}
};

class CProd
{
public:
	CProd()
	{
	}

	void operator=(sint64 val)
	{
		g_dsp.r[0x14] = (uint16)val;
		val >>= 16;
		g_dsp.r[0x15] = (uint16)val;
		val >>= 16;
		g_dsp.r[0x16] = (uint16)val;
		g_dsp.r[0x17] = 0;
	}

	operator sint64()
	{
		sint64 val;
		sint64 low_prod;
		val   = (sint8)g_dsp.r[0x16];
		val <<= 32;
		low_prod  = g_dsp.r[0x15];
		low_prod += g_dsp.r[0x17];
		low_prod <<= 16;
		low_prod |= g_dsp.r[0x14];
		val += low_prod;
		return(val);
	}
};

#endif
