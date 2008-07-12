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

// Lame slow breakpoint system
// TODO: a real one

#include "Common.h"

#include "../HW/CPU.h"

#include "Debugger_SymbolMap.h"
#include "Debugger_BreakPoints.h"

using namespace Debugger;

std::vector<TBreakPoint> CBreakPoints::m_iBreakPoints;
std::vector<TMemCheck> CBreakPoints::MemChecks;
u32 CBreakPoints::m_iBreakOnCount = 0;

TMemCheck::TMemCheck()
{
	numHits = 0;
}

void TMemCheck::Action(u32 iValue, u32 addr, bool write, int size, u32 pc)
{
	if ((write && bOnWrite) || (!write && bOnRead))
	{
		if (bLog)
		{
            const char *copy = Debugger::GetDescription(addr);
			LOG(MEMMAP,"CHK %08x %s%i at %08x (%s)", iValue, write ? "Write" : "Read", size*8, addr, copy);
		}
		if (bBreak)
			CCPU::Break();
	}
}

bool CBreakPoints::IsAddressBreakPoint(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;

	for (iter = m_iBreakPoints.begin(); iter != m_iBreakPoints.end(); ++iter)
		if ((*iter).iAddress == _iAddress)
			return true;

	return false;
}

bool CBreakPoints::IsTempBreakPoint(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;

	for (iter = m_iBreakPoints.begin(); iter != m_iBreakPoints.end(); ++iter)
		if ((*iter).iAddress == _iAddress && (*iter).bTemporary)
			return true;

	return false;
}

TMemCheck *CBreakPoints::GetMemCheck(u32 address)
{
	std::vector<TMemCheck>::iterator iter;
	for (iter = MemChecks.begin(); iter != MemChecks.end(); ++iter)
	{
		if ((*iter).bRange)
		{
			if (address >= (*iter).iStartAddress && address <= (*iter).iEndAddress)
				return &(*iter);
		}
		else
		{
			if ((*iter).iStartAddress==address)
				return &(*iter);
		}
	}

	//none found
	return 0;
}

void CBreakPoints::AddBreakPoint(u32 _iAddress, bool temp)
{
	if (!IsAddressBreakPoint(_iAddress))
	{
		TBreakPoint pt;
		pt.bOn = true;
		pt.bTemporary = temp;
		pt.iAddress = _iAddress;

		m_iBreakPoints.push_back(pt);
	}
}

void CBreakPoints::RemoveBreakPoint(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;

	for (iter = m_iBreakPoints.begin(); iter != m_iBreakPoints.end(); ++iter)
	{
		if ((*iter).iAddress == _iAddress)
		{
			m_iBreakPoints.erase(iter);
			break;
		}
	}
}

void CBreakPoints::ClearAllBreakPoints()
{
	m_iBreakPoints.clear();
}

void CBreakPoints::AddAutoBreakpoints()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
#if 1
	const char *bps[] = {
		"PPCHalt",
	};

	for (int i = 0; i < sizeof(bps) / sizeof(const char *); i++)
	{
		XSymbolIndex idx = FindSymbol(bps[i]);
		if (idx != 0)
		{
			const CSymbol &symbol = GetSymbol(idx);
			AddBreakPoint(symbol.vaddress, false);
		}
	}
#endif
#endif
}
