// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "Common/Logging/LogManager.h"
#include "VideoCommon/DriverDetails.h"

namespace DriverDetails
{
	struct BugInfo
	{
		u32 m_os; // Which OS has the issue
		Vendor m_vendor; // Which vendor has the error
		Driver m_driver; // Which driver has the error
		s32 m_family; // Which family of hardware has the issue
		Bug m_bug; // Which bug it is
		double m_versionstart; // When it started
		double m_versionend; // When it ended
		bool m_hasbug; // Does it have it?
	};

	// Local members
#ifdef _WIN32
	const u32 m_os = OS_ALL | OS_WINDOWS;
#elif ANDROID
	const u32 m_os = OS_ALL | OS_ANDROID;
#elif __APPLE__
	const u32 m_os = OS_ALL | OS_OSX;
#elif __linux__
	const u32 m_os = OS_ALL | OS_LINUX;
#endif

	static Vendor m_vendor = VENDOR_UNKNOWN;
	static Driver m_driver = DRIVER_UNKNOWN;
	static s32    m_family = 0;
	static double m_version = 0.0;

	// This is a list of all known bugs for each vendor
	// We use this to check if the device and driver has a issue
	static BugInfo m_known_bugs[] = {
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_NODYNUBOACCESS,      14.0, 94.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENCENTROID,      14.0, 46.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENINFOLOG,       -1.0, 46.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_ANNIHILATEDUBOS,     41.0, 46.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENSWAP,          -1.0, 46.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENBUFFERSTREAM,  -1.0, -1.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENTEXTURESIZE,   -1.0, 65.0, true},
		{OS_ALL,    VENDOR_QUALCOMM, DRIVER_QUALCOMM_3XX, -1, BUG_BROKENATTRIBUTELESS, -1.0, 94.0, true},
		{OS_ALL,    VENDOR_ARM,      DRIVER_ARM_MIDGARD,  -1, BUG_BROKENBUFFERSTREAM,  -1.0, -1.0, true},
		{OS_ALL,    VENDOR_MESA,     DRIVER_NOUVEAU,      -1, BUG_BROKENUBO,           900,  916, true},
		{OS_ALL,    VENDOR_MESA,     DRIVER_R600,         -1, BUG_BROKENUBO,           900,  913, true},
		{OS_ALL,    VENDOR_MESA,     DRIVER_I965,         -1, BUG_BROKENUBO,           900,  920, true},
		{OS_LINUX,  VENDOR_ATI,      DRIVER_ATI,          -1, BUG_BROKENPINNEDMEMORY,  -1.0, -1.0, true},
		{OS_LINUX,  VENDOR_NVIDIA,   DRIVER_NVIDIA,       -1, BUG_BROKENBUFFERSTORAGE, -1.0, 33138.0, true},
		{OS_OSX,    VENDOR_INTEL,    DRIVER_INTEL,      3000, BUG_PRIMITIVERESTART,    -1.0, -1.0, true},
		{OS_WINDOWS,VENDOR_NVIDIA,   DRIVER_NVIDIA,       -1, BUG_BROKENUNSYNCMAPPING, -1.0, -1.0, true},
		{OS_LINUX,  VENDOR_NVIDIA,   DRIVER_NVIDIA,       -1, BUG_BROKENUNSYNCMAPPING, -1.0, -1.0, true},
		{OS_WINDOWS,VENDOR_INTEL,    DRIVER_INTEL,        -1, BUG_INTELBROKENBUFFERSTORAGE, 101810.3907, 101810.3960, true},
	};

	static std::map<Bug, BugInfo> m_bugs;

	void Init(Vendor vendor, Driver driver, const double version, const s32 family)
	{
		m_vendor = vendor;
		m_driver = driver;
		m_version = version;
		m_family = family;

		if (driver == DRIVER_UNKNOWN)
			switch (vendor)
			{
				case VENDOR_NVIDIA:
				case VENDOR_TEGRA:
					m_driver = DRIVER_NVIDIA;
					break;
				case VENDOR_ATI:
					m_driver = DRIVER_ATI;
					break;
				case VENDOR_INTEL:
					m_driver = DRIVER_INTEL;
					break;
				case VENDOR_IMGTEC:
					m_driver = DRIVER_IMGTEC;
					break;
				case VENDOR_VIVANTE:
					m_driver = DRIVER_VIVANTE;
					break;
				default:
					break;
			}

		for (auto& bug : m_known_bugs)
		{
			if (( bug.m_os & m_os ) &&
			    ( bug.m_vendor == m_vendor || bug.m_vendor == VENDOR_ALL ) &&
			    ( bug.m_driver == m_driver || bug.m_driver == DRIVER_ALL ) &&
			    ( bug.m_family == m_family || bug.m_family == -1) &&
			    ( bug.m_versionstart <= m_version || bug.m_versionstart == -1 ) &&
			    ( bug.m_versionend > m_version || bug.m_versionend == -1 )
			)
				m_bugs.insert(std::make_pair(bug.m_bug, bug));
		}
	}

	bool HasBug(Bug bug)
	{
		auto it = m_bugs.find(bug);
		if (it == m_bugs.end())
			return false;
		return it->second.m_hasbug;
	}
}
