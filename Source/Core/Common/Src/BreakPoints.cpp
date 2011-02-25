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

#include "Common.h"
#include "DebugInterface.h"
#include "BreakPoints.h"
#include <sstream>

bool BreakPoints::IsAddressBreakPoint(u32 _iAddress)
{
	for (TBreakPoints::iterator i = m_BreakPoints.begin(); i != m_BreakPoints.end(); ++i)
		if (i->iAddress == _iAddress)
			return true;
	return false;
}

bool BreakPoints::IsTempBreakPoint(u32 _iAddress)
{
	for (TBreakPoints::iterator i = m_BreakPoints.begin(); i != m_BreakPoints.end(); ++i)
		if (i->iAddress == _iAddress && i->bTemporary)
			return true;
	return false;
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
	TBreakPointsStr bps;
	for (TBreakPoints::const_iterator i = m_BreakPoints.begin();
		i != m_BreakPoints.end(); ++i)
	{
		if (!i->bTemporary)
		{
			std::stringstream bp;
			bp << std::hex << i->iAddress << " " << (i->bOn ? "n" : "");
			bps.push_back(bp.str());
		}
	}

	return bps;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bps)
{
	for (TBreakPointsStr::const_iterator i = bps.begin(); i != bps.end(); ++i)
	{
		TBreakPoint bp;
		std::stringstream bpstr;
		bpstr << std::hex << *i;
		bpstr >> bp.iAddress;
		bp.bOn = i->find("n") != i->npos;
		bp.bTemporary = false;
		Add(bp);
	}
}

void BreakPoints::Add(const TBreakPoint& bp)
{
	if (!IsAddressBreakPoint(bp.iAddress))
		m_BreakPoints.push_back(bp);
}

void BreakPoints::Add(u32 em_address, bool temp)
{
	if (!IsAddressBreakPoint(em_address)) // only add new addresses
	{
		TBreakPoint pt; // breakpoint settings
		pt.bOn = true;
		pt.bTemporary = temp;
		pt.iAddress = em_address;

		m_BreakPoints.push_back(pt);
	}
}

void BreakPoints::Remove(u32 _iAddress)
{
	for (TBreakPoints::iterator i = m_BreakPoints.begin(); i != m_BreakPoints.end(); ++i)
	{
		if (i->iAddress == _iAddress)
		{
			m_BreakPoints.erase(i);
			return;
		}
	}
}


MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
	TMemChecksStr mcs;
	for (TMemChecks::const_iterator i = m_MemChecks.begin();
		i != m_MemChecks.end(); ++i)
	{
		std::stringstream mc;
		mc << std::hex << i->StartAddress;
		mc << " " << (i->bRange ? i->EndAddress : i->StartAddress) << " " <<
			(i->bRange ? "n" : "") << (i->OnRead ? "r" : "") <<
			(i->OnWrite ? "w" : "") << (i->Log ? "l" : "") << (i->Break ? "p" : "");
		mcs.push_back(mc.str());
	}

	return mcs;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mcs)
{
	for (TMemChecksStr::const_iterator i = mcs.begin(); i != mcs.end(); ++i)
	{
		TMemCheck mc;
		std::stringstream mcstr;
		mcstr << std::hex << *i;
		mcstr >> mc.StartAddress;
		mc.bRange	= i->find("n") != i->npos;
		mc.OnRead	= i->find("r") != i->npos;
		mc.OnWrite	= i->find("w") != i->npos;
		mc.Log		= i->find("l") != i->npos;
		mc.Break	= i->find("p") != i->npos;
		if (mc.bRange)
			mcstr >> mc.EndAddress;
		else
			mc.EndAddress = mc.StartAddress;
		Add(mc);
	}
}

void MemChecks::Add(const TMemCheck& _rMemoryCheck)
{
	if (GetMemCheck(_rMemoryCheck.StartAddress) == 0)
		m_MemChecks.push_back(_rMemoryCheck);
}

void MemChecks::Remove(u32 _Address)
{
	for (TMemChecks::iterator i = m_MemChecks.begin(); i != m_MemChecks.end(); ++i)
	{
		if (i->StartAddress == _Address)
		{
			m_MemChecks.erase(i);
			return;
		}
	}
}

TMemCheck *MemChecks::GetMemCheck(u32 address)
{
	for (TMemChecks::iterator i = m_MemChecks.begin(); i != m_MemChecks.end(); ++i)
	{
		if (i->bRange)
		{
			if (address >= i->StartAddress && address <= i->EndAddress)
				return &(*i);
		}
		else if (i->StartAddress == address)
			return &(*i);
	}

	// none found
	return 0;
}

void TMemCheck::Action(DebugInterface *debug_interface, u32 iValue, u32 addr,
						bool write, int size, u32 pc)
{
	if ((write && OnWrite) || (!write && OnRead))
	{
		if (Log)
		{
			INFO_LOG(MEMMAP, "CHK %08x (%s) %s%i %0*x at %08x (%s)",
				pc, debug_interface->getDescription(pc).c_str(),
				write ? "Write" : "Read", size*8, size*2, iValue, addr,
				debug_interface->getDescription(addr).c_str()
				);
		}
		if (Break)
			debug_interface->breakNow();
	}
}
