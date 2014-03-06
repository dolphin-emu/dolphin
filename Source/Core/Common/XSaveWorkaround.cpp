// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#if defined(_WIN32) && defined(_ARCH_64)

#include <math.h>
#include <Windows.h>

// This puts the rest of this translation unit into a segment which is
// initialized by the CRT *before* any of the other code (including C++
// static initializers).
#pragma warning(disable : 4075)
#pragma init_seg(".CRT$XCB")

struct EnableXSaveWorkaround
{
	EnableXSaveWorkaround()
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
		}
	}
};

static EnableXSaveWorkaround enableXSaveWorkaround;

// N.B. Any code after this will still be in the .CRT$XCB segment. Please just
// do not append any code here unless it is intended to be executed before
// static initializers.

#endif
