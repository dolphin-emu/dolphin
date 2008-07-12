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
#ifndef _DEBUGGER_BREAKPOINTS_H
#define _DEBUGGER_BREAKPOINTS_H

#include <vector>
#include <string>

#include "Common.h"

struct TBreakPoint
{
	u32    iAddress;
	bool	bOn;
	bool    bTemporary;
};

struct TMemCheck
{
	TMemCheck();
	u32 iStartAddress;
	u32 iEndAddress;

	bool	bRange;

	bool	bOnRead;
	bool	bOnWrite;

	bool	bLog;
	bool	bBreak;

	u32 numHits;

	void Action(u32 _iValue, u32 addr, bool write, int size, u32 pc);
};

class CBreakPoints
{
private:

	enum { MAX_NUMBER_OF_CALLSTACK_ENTRIES = 16384};
	enum { MAX_NUMBER_OF_BREAKPOINTS = 16};
	
	static std::vector<TBreakPoint> m_iBreakPoints;

	static u32	m_iBreakOnCount;

public:
	static std::vector<TMemCheck> MemChecks;

	// is address breakpoint
	static bool IsAddressBreakPoint(u32 _iAddress);

	//memory breakpoint
	static TMemCheck *GetMemCheck(u32 address);
					
	// is break on count
	static void SetBreakCount(u32 count) { m_iBreakOnCount = count; }
	static u32 GetBreakCount() { return m_iBreakOnCount; }

	static bool IsTempBreakPoint(u32 _iAddress);

	// AddBreakPoint
	static void AddBreakPoint(u32 _iAddress, bool temp=false);

	// Remove Breakpoint
	static void RemoveBreakPoint(u32 _iAddress);

	static void ClearAllBreakPoints();

	static void AddAutoBreakpoints();
};

#endif

