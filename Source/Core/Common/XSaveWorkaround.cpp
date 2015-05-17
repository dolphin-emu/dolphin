// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if defined(_WIN32)

#include <math.h>
#include <Windows.h>

typedef decltype(&GetEnabledXStateFeatures) GetEnabledXStateFeatures_t;

int __cdecl EnableXSaveWorkaround()
{
	// Some Windows environments may have hardware support for AVX/FMA,
	// but the OS does not support it. The CRT math library does not support
	// this scenario, so we have to manually tell it not to use FMA3
	// instructions.

	// The API name is somewhat misleading - we're testing for OS support
	// here.
	if (!IsProcessorFeaturePresent(PF_XSAVE_ENABLED))
	{
		_set_FMA3_enable(0);
		return 0;
	}

	// Even if XSAVE feature is enabled, we have to see if
	// GetEnabledXStateFeatures function is present, and see what it says about
	// AVX state.
	auto kernel32Handle = GetModuleHandle(TEXT("kernel32.dll"));
	if (kernel32Handle == nullptr)
	{
		std::abort();
	}

	auto pGetEnabledXStateFeatures = (GetEnabledXStateFeatures_t)GetProcAddress(
		kernel32Handle, "GetEnabledXStateFeatures");
	if (pGetEnabledXStateFeatures == nullptr ||
		(pGetEnabledXStateFeatures() & XSTATE_MASK_AVX) == 0)
	{
		_set_FMA3_enable(0);
	}

	return 0;
}

// Create a segment which is recognized by the linker to be part of the CRT
// initialization. XI* = C startup, XC* = C++ startup. "A" placement is reserved
// for system use. Thus, the earliest we can get is XIB (C startup is before
// C++).
#pragma section(".CRT$XIB", read)

// Place a symbol in the special segment, make it have C linkage so that
// referencing it doesn't require ugly decorated names.
// Use /include:XSaveWorkaround linker flag to enable this.
extern "C" {
	__declspec(allocate(".CRT$XIB"))
	decltype(&EnableXSaveWorkaround) XSaveWorkaround = EnableXSaveWorkaround;
};

#endif
