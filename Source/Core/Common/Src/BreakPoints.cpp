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

#include "Common.h"

#include "DebugInterface.h"
#include "BreakPoints.h"

void TMemCheck::Action(DebugInterface *debug_interface, u32 iValue, u32 addr, bool write, int size, u32 pc)
{
	if ((write && OnWrite) || (!write && OnRead))
	{
		if (Log)
		{
			DEBUG_LOG(MEMMAP, "CHK %08x %s%i at %08x (%s)",
				iValue, write ? "Write" : "Read", // read or write
				size*8, addr, // address
				debug_interface->GetDescription(addr).c_str() // symbol map description
				);
		}
		if (Break)
			debug_interface->breakNow();
	}
}

bool BreakPoints::IsAddressBreakPoint(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;
	for (iter = m_BreakPoints.begin(); iter != m_BreakPoints.end(); ++iter)
		if ((*iter).iAddress == _iAddress)
			return true;
	return false;
}

bool BreakPoints::IsTempBreakPoint(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;

	for (iter = m_BreakPoints.begin(); iter != m_BreakPoints.end(); ++iter)
		if ((*iter).iAddress == _iAddress && (*iter).bTemporary)
			return true;

	return false;
}

bool BreakPoints::Add(u32 em_address, bool temp)
{
	if (!IsAddressBreakPoint(em_address)) // only add new addresses
	{
		TBreakPoint pt; // breakpoint settings
		pt.bOn = true;
		pt.bTemporary = temp;
		pt.iAddress = em_address;

		m_BreakPoints.push_back(pt);
		return true;
	} else {
		return false;
	}
}

bool BreakPoints::Remove(u32 _iAddress)
{
	std::vector<TBreakPoint>::iterator iter;
	for (iter = m_BreakPoints.begin(); iter != m_BreakPoints.end(); ++iter)
	{
		if ((*iter).iAddress == _iAddress)
		{
			m_BreakPoints.erase(iter);
			return true;
		}
	}
	return false;
}

void BreakPoints::Clear()
{
	m_BreakPoints.clear();
}

void BreakPoints::DeleteByAddress(u32 _Address)
{
    // first check breakpoints
    {
        std::vector<TBreakPoint>::iterator iter;
        for (iter = m_BreakPoints.begin(); iter != m_BreakPoints.end(); ++iter)
        {
            if ((*iter).iAddress == _Address)
            {
                m_BreakPoints.erase(iter);
                return;
            } 
        }
    }
}

void MemChecks::Add(const TMemCheck& _rMemoryCheck)
{
	m_MemChecks.push_back(_rMemoryCheck);
}


TMemCheck *MemChecks::GetMemCheck(u32 address)
{
	std::vector<TMemCheck>::iterator iter;
	for (iter = m_MemChecks.begin(); iter != m_MemChecks.end(); ++iter)
	{
		if ((*iter).bRange)
		{
			if (address >= (*iter).StartAddress && address <= (*iter).EndAddress)
				return &(*iter);
		}
		else
		{
			if ((*iter).StartAddress == address)
				return &(*iter);
		}
	}

	//none found
	return 0;
}

void MemChecks::Clear()
{
	m_MemChecks.clear();
}

void MemChecks::DeleteByAddress(u32 _Address)
{
    std::vector<TMemCheck>::iterator iter;
    for (iter = m_MemChecks.begin(); iter != m_MemChecks.end(); ++iter)
    {
        if ((*iter).StartAddress == _Address)
        {
            m_MemChecks.erase(iter);
			return;
        }
    }
}
