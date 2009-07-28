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

#ifndef _DSP_BREAKPOINTS
#define _DSP_BREAKPOINTS

#include "Common.h"

// super fast breakpoints for a limited range.
// To be used interchangably with the BreakPoints class.
class DSPBreakpoints
{
public:
	DSPBreakpoints() {Clear();}
	// is address breakpoint
	bool IsAddressBreakPoint(u32 addr) {
		return b[addr] != 0;
	}

	// AddBreakPoint
	bool Add(u32 addr, bool temp=false) {
		bool was_one = b[addr] != 0;
		if (!was_one) {
			b[addr] = temp ? 2 : 1;
			return true;
		} else {
			return false;
		}
	}
	// Remove Breakpoint
	bool Remove(u32 addr) {
		bool was_one = b[addr] != 0;
		b[addr] = 0;
		return was_one;
	}
	void Clear() {
		for (int i = 0; i < 65536; i++)
			b[i] = 0;
	}

	void DeleteByAddress(u32 addr) {
		b[addr] = 0;
	}

private:
	u8 b[65536];
};

#endif
