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
	TMemCheck() {
		numHits = 0;
	}
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


// Code breakpoints.
class BreakPoints
{
public:
	typedef std::vector<TBreakPoint> TBreakPoints;

	static const TBreakPoints& GetBreakPoints() { return m_BreakPoints; }

	// is address breakpoint
	static bool IsAddressBreakPoint(u32 _iAddress);
	static bool IsTempBreakPoint(u32 _iAddress);

	// AddBreakPoint
	static void Add(u32 em_address, bool temp=false);

	// Remove Breakpoint
	static void Remove(u32 _iAddress);
	static void Clear();

	static void UpdateBreakPointView();

	static void AddAutoBreakpoints();
    static void DeleteByAddress(u32 _Address);

private:
	static TBreakPoints m_BreakPoints;
	static u32	m_iBreakOnCount;
};


// Memory breakpoints
class MemChecks
{
public:
	typedef std::vector<TMemCheck> TMemChecks;
	static TMemChecks m_MemChecks;
	static const TMemChecks& GetMemChecks()		{ return m_MemChecks; }
	static void Add(const TMemCheck& _rMemoryCheck);

	//memory breakpoint
	static TMemCheck *GetMemCheck(u32 address);
    static void DeleteByAddress(u32 _Address);

	void Clear();
};

#endif

