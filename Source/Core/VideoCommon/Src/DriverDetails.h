// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#pragma once
#include "CommonTypes.h"

namespace DriverDetails
{
	// Enum of known vendors
	// Tegra and Nvidia are separated out due to such substantial differences
	enum Vendor
	{
		VENDOR_NVIDIA = 0,
		VENDOR_ATI,
		VENDOR_INTEL,
		VENDOR_ARM,
		VENDOR_QUALCOMM,
		VENDOR_IMGTEC,
		VENDOR_TEGRA,
		VENDOR_VIVANTE,
		VENDOR_UNKNOWN
	};

	// Enum of known bugs
	// These can be vendor specific, but we put them all in here
	// For putting a new bug in here, make sure to put a detailed comment above the enum
	// This'll ensure we know exactly what the issue is.
	enum Bug
	{
		// Bug: No Dynamic UBO array object access
		// Affected Devices: Qualcomm/Adreno
		// Started Version: 14
		// Ended Version: -1
		// Accessing UBO array members dynamically causes the Adreno shader compiler to crash
		// Errors out with "Internal Error"
		BUG_NODYNUBOACCESS = 0,
		// Bug: Centroid is broken in shaders
		// Affected devices: Qualcomm/Adreno
		// Started Version: 14
		// Ended Version: -1
		// Centroid in/out, used in the shaders, is used for multisample buffers to get the texel correctly
		// When MSAA is disabled, it acts like a regular in/out
		// Tends to cause the driver to render full white or black
		BUG_BROKENCENTROID,
	};
	
	// Initializes our internal vendor, device family, and driver version	
	void Init(Vendor vendor, const u32 devfamily, const double version);
	
	// Once Vendor and driver version is set, this will return if it has the applicable bug passed to it.
	const bool HasBug(Bug bug);
}
