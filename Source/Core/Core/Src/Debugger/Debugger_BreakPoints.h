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

	const TBreakPoints& GetBreakPoints() { return m_BreakPoints; }

	// is address breakpoint
	bool IsAddressBreakPoint(u32 _iAddress);
	bool IsTempBreakPoint(u32 _iAddress);

	// AddBreakPoint
	bool Add(u32 em_address, bool temp=false);

	// Remove Breakpoint
	bool Remove(u32 _iAddress);
	void Clear();

    void DeleteByAddress(u32 _Address);

private:
	TBreakPoints m_BreakPoints;
	u32	m_iBreakOnCount;
};


// Memory breakpoints
class MemChecks
{
public:
	typedef std::vector<TMemCheck> TMemChecks;
	TMemChecks m_MemChecks;
	const TMemChecks& GetMemChecks()		{ return m_MemChecks; }
	void Add(const TMemCheck& _rMemoryCheck);

	//memory breakpoint
	TMemCheck *GetMemCheck(u32 address);
    void DeleteByAddress(u32 _Address);

	void Clear();
};

#endif

