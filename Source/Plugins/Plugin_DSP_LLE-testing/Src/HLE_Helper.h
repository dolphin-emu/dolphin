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

#ifndef HLE_HELPER_H
#define HLE_HELPER_H

#include "gdsp_interpreter.h"

bool WriteDMEM(u16 addr, u16 val);
u16 ReadDMEM(u16 addr);
void Update_SR_Register(s64 _Value);
s8 GetMultiplyModifier();

template<unsigned N> 
class TAccumulator
{
public:

	TAccumulator()
	{
		_assert_(N < 2);
	}

	void operator=(s64 val)
	{
		setValue(val);
	}

	void operator<<=(s64 value)
	{
		s64 acc = getValue();
		acc <<= value;
		setValue(acc);
	}

	void operator>>=(s64 value)
	{
		s64 acc = getValue();
		acc >>= value;
		setValue(acc);
	}

	void operator+=(s64 value)
	{
		s64 acc = getValue();
		acc += value;
		setValue(acc);
	}

	operator s64()
	{
		return getValue();
	}

	operator u64()
	{
		return getValue();
	}

	inline void setValue(s64 val)
	{
		g_dsp.r[0x1c + N] = (u16)val;
		val >>= 16;
		g_dsp.r[0x1e + N] = (u16)val;
		val >>= 16;
		g_dsp.r[0x10 + N] = (u16)val;
	}

	inline s64 getValue()
	{
		s64 val;
		s64 low_acc;
		val       = (s8)g_dsp.r[0x10 + N];
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

	void operator=(s64 val)
	{
		g_dsp.r[0x14] = (u16)val;
		val >>= 16;
		g_dsp.r[0x15] = (u16)val;
		val >>= 16;
		g_dsp.r[0x16] = (u16)val;
		g_dsp.r[0x17] = 0;
	}

	operator s64()
	{
		s64 val;
		s64 low_prod;
		val   = (s8)g_dsp.r[0x16];
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
