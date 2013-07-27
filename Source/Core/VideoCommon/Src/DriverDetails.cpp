// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "LogManager.h"
#include "DriverDetails.h"

namespace DriverDetails
{
	struct BugInfo
	{
		Bug m_bug; // Which bug it is
		u32 m_devfamily; // Which device(family) has the error
		double m_versionstart; // When it started
		double m_versionend; // When it ended
		bool m_hasbug; // Does it have it?
	};

	// Local members
	Vendor	m_vendor = VENDOR_UNKNOWN;
	u32	m_devfamily = 0;
	double	m_version = 0.0;

	// This is a list of all known bugs for each vendor
	// We use this to check if the device and driver has a issue
	BugInfo m_qualcommbugs[] = {
		{BUG_NODYNUBOACCESS, 300, 14.0, -1.0},
		{BUG_BROKENCENTROID, 300, 14.0, -1.0},
		{BUG_BROKENINFOLOG, 300, -1.0, -1.0},
	};

	std::map<std::pair<Vendor, Bug>, BugInfo> m_bugs;
	
	// Private function
	void InitBugMap()
	{
		switch(m_vendor)
		{
			case VENDOR_QUALCOMM:
				for (unsigned int a = 0; a < (sizeof(m_qualcommbugs) / sizeof(BugInfo)); ++a)
					m_bugs[std::make_pair(m_vendor, m_qualcommbugs[a].m_bug)] = m_qualcommbugs[a];
			break;
			case VENDOR_ARM:
			default:
			break;
		}
	}

	void Init(Vendor vendor, const u32 devfamily, const double version)
	{
		m_vendor = vendor;
		m_devfamily = devfamily;
		m_version = version;
		InitBugMap();

		for (auto it = m_bugs.begin(); it != m_bugs.end(); ++it)
			if (it->second.m_devfamily == m_devfamily)
				if (it->second.m_versionend == -1.0 || (it->second.m_versionstart <= m_version && it->second.m_versionend > m_version))
					it->second.m_hasbug = true;
	}

	bool HasBug(Bug bug)
	{
		auto it = m_bugs.find(std::make_pair(m_vendor, bug));
		if (it == m_bugs.end())
			return false;	
		return it->second.m_hasbug;
	}
}
