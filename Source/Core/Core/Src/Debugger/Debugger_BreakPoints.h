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
	u32     iAddress;
	bool	bOn;
	bool    bTemporary;
};

struct TMemCheck
{
	TMemCheck();
	u32 StartAddress;
	u32 EndAddress;

	bool	bRange;

	bool	OnRead;
	bool	OnWrite;

	bool	Log;
	bool	Break;

	u32		numHits;

	void Action(u32 _iValue, u32 addr, bool write, int size, u32 pc);
};

class CBreakPoints
{
public:

	typedef std::vector<TBreakPoint> TBreakPoints;
	typedef std::vector<TMemCheck> TMemChecks;

	static const TBreakPoints& GetBreakPoints() { return m_BreakPoints; }
	static const TMemChecks& GetMemChecks()		{ return m_MemChecks; }

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

	static void AddMemoryCheck(const TMemCheck& _rMemoryCheck);

	static void ClearAllBreakPoints();

	static void AddAutoBreakpoints();

    static void DeleteElementByAddress(u32 _Address);

private:

	static TBreakPoints m_BreakPoints;

	static TMemChecks m_MemChecks;

	static u32	m_iBreakOnCount;

};

#endif

