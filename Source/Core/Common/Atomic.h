// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32

#include "Common/Atomic_Win32.h"  // IWYU pragma: export

#else

// GCC-compatible compiler assumed!
#include "Common/Atomic_GCC.h"  // IWYU pragma: export

#endif
