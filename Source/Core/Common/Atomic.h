// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once 

#ifdef _WIN32

#include "Atomic_Win32.h"

#else

// GCC-compatible compiler assumed!
#include "Atomic_GCC.h"

#endif
