// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSP_BREAKPOINTS
#define _DSP_BREAKPOINTS

#include "Common.h"

// super fast breakpoints for a limited range.
// To be used interchangeably with the BreakPoints class.
class DSPBreakpoints
{
public:
	DSPBreakpoints()
	{
		Clear();
	}
	
	// is address breakpoint
	bool IsAddressBreakPoint(u32 addr)
	{
		return b[addr] != 0;
	}

	// AddBreakPoint
	bool Add(u32 addr, bool temp=false)
	{
		bool was_one = b[addr] != 0;
		
		if (!was_one)
		{
			b[addr] = temp ? 2 : 1;
			return true;
		}
		else
		{
			return false;
		}
	}

	// Remove Breakpoint
	bool Remove(u32 addr)
	{
		bool was_one = b[addr] != 0;
		b[addr] = 0;
		return was_one;
	}

	void Clear()
	{
		for (int i = 0; i < 65536; i++)
			b[i] = 0;
	}

	void DeleteByAddress(u32 addr)
	{
		b[addr] = 0;
	}

private:
	u8 b[65536];
};

#endif
