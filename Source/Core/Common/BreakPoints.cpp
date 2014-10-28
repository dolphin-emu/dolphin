// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>

#include "Common/BreakPoints.h"
#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

bool BreakPoints::IsAddressBreakPoint(u32 _iAddress)
{
	for (const TBreakPoint& bp : m_BreakPoints)
		if (bp.iAddress == _iAddress)
			return true;

	return false;
}

bool BreakPoints::IsTempBreakPoint(u32 _iAddress)
{
	for (const TBreakPoint& bp : m_BreakPoints)
		if (bp.iAddress == _iAddress && bp.bTemporary)
			return true;

	return false;
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
	TBreakPointsStr bps;
	for (const TBreakPoint& bp : m_BreakPoints)
	{
		if (!bp.bTemporary)
		{
			std::stringstream ss;
			ss << std::hex << bp.iAddress << " " << (bp.bOn ? "n" : "");
			bps.push_back(ss.str());
		}
	}

	return bps;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bpstrs)
{
	for (const std::string& bpstr : bpstrs)
	{
		TBreakPoint bp;
		std::stringstream ss;
		ss << std::hex << bpstr;
		ss >> bp.iAddress;
		bp.bOn = bpstr.find("n") != bpstr.npos;
		bp.bTemporary = false;
		Add(bp);
	}
}

void BreakPoints::Add(const TBreakPoint& bp)
{
	if (!IsAddressBreakPoint(bp.iAddress))
	{
		m_BreakPoints.push_back(bp);
		if (jit)
			jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4, true);
	}
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

		if (jit)
			jit->GetBlockCache()->InvalidateICache(em_address, 4, true);
	}
}

void BreakPoints::Remove(u32 em_address)
{
	for (auto i = m_BreakPoints.begin(); i != m_BreakPoints.end(); ++i)
	{
		if (i->iAddress == em_address)
		{
			m_BreakPoints.erase(i);
			if (jit)
				jit->GetBlockCache()->InvalidateICache(em_address, 4, true);
			return;
		}
	}
}

void BreakPoints::Clear()
{
	if (jit)
	{
		for (const TBreakPoint& bp : m_BreakPoints)
		{
			jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4, true);
		}
	}

	m_BreakPoints.clear();
}

void BreakPoints::ClearAllTemporary()
{
	for (const TBreakPoint& bp : m_BreakPoints)
	{
		if (bp.bTemporary)
		{
			if (jit)
				jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4, true);
			Remove(bp.iAddress);
		}
	}
}

MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
	TMemChecksStr mcs;
	for (const TMemCheck& bp : m_MemChecks)
	{
		std::stringstream mc;
		mc << std::hex << bp.StartAddress;
		mc << " " << (bp.bRange ? bp.EndAddress : bp.StartAddress) << " " <<
			(bp.bRange ? "n" : "") << (bp.OnRead ? "r" : "") <<
			(bp.OnWrite ? "w" : "") << (bp.Log ? "l" : "") << (bp.Break ? "p" : "");
		mcs.push_back(mc.str());
	}

	return mcs;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mcstrs)
{
	for (const std::string& mcstr : mcstrs)
	{
		TMemCheck mc;
		std::stringstream ss;
		ss << std::hex << mcstr;
		ss >> mc.StartAddress;
		mc.bRange  = mcstr.find("n") != mcstr.npos;
		mc.OnRead  = mcstr.find("r") != mcstr.npos;
		mc.OnWrite = mcstr.find("w") != mcstr.npos;
		mc.Log     = mcstr.find("l") != mcstr.npos;
		mc.Break   = mcstr.find("p") != mcstr.npos;
		if (mc.bRange)
			ss >> mc.EndAddress;
		else
			mc.EndAddress = mc.StartAddress;
		Add(mc);
	}
}

void MemChecks::Add(const TMemCheck& _rMemoryCheck)
{
	if (GetMemCheck(_rMemoryCheck.StartAddress) == nullptr)
		m_MemChecks.push_back(_rMemoryCheck);
}

void MemChecks::Remove(u32 _Address)
{
	for (auto i = m_MemChecks.begin(); i != m_MemChecks.end(); ++i)
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
	for (TMemCheck& bp : m_MemChecks)
	{
		if (bp.bRange)
		{
			if (address >= bp.StartAddress && address <= bp.EndAddress)
				return &(bp);
		}
		else if (bp.StartAddress == address)
		{
			return &(bp);
		}
	}

	// none found
	return nullptr;
}

void TMemCheck::Action(DebugInterface *debug_interface, u32 iValue, u32 addr, bool write, int size, u32 pc)
{
	if ((write && OnWrite) || (!write && OnRead))
	{
		if (Log)
		{
			INFO_LOG(MEMMAP, "CHK %08x (%s) %s%i %0*x at %08x (%s)",
				pc, debug_interface->GetDescription(pc).c_str(),
				write ? "Write" : "Read", size*8, size*2, iValue, addr,
				debug_interface->GetDescription(addr).c_str()
				);
		}

		if (Break)
			debug_interface->BreakNow();
	}
}


const bool Watches::IsAddressWatch(u32 _iAddress)
{
	for (const TWatch& bp : m_Watches)
		if (bp.iAddress == _iAddress)
			return true;

	return false;
}

Watches::TWatchesStr Watches::GetStrings() const
{
	TWatchesStr bps;
	for (const TWatch& bp : m_Watches)
	{
		std::stringstream ss;
		ss << std::hex << bp.iAddress << " " << bp.name;
		bps.push_back(ss.str());
	}

	return bps;
}

void Watches::AddFromStrings(const TWatchesStr& bpstrs)
{
	for (const std::string& bpstr : bpstrs)
	{
		TWatch bp;
		std::stringstream ss;
		ss << std::hex << bpstr;
		ss >> bp.iAddress;
		ss >> std::ws;
		getline(ss, bp.name);
		Add(bp);
	}
}

void Watches::Add(const TWatch& bp)
{
	if (!IsAddressWatch(bp.iAddress))
	{
		m_Watches.push_back(bp);
	}
}

void Watches::Add(u32 em_address)
{
	if (!IsAddressWatch(em_address)) // only add new addresses
	{
		TWatch pt; // breakpoint settings
		pt.bOn = true;
		pt.iAddress = em_address;

		m_Watches.push_back(pt);
	}
}

void Watches::Update(int count, u32 em_address)
{
	m_Watches.at(count).iAddress = em_address;
}

void Watches::UpdateName(int count, const std::string name)
{
	m_Watches.at(count).name = name;
}

void Watches::Remove(u32 em_address)
{
	for (auto i = m_Watches.begin(); i != m_Watches.end(); ++i)
	{
		if (i->iAddress == em_address)
		{
			m_Watches.erase(i);
			return;
		}
	}
}

void Watches::Clear()
{
	m_Watches.clear();
}
