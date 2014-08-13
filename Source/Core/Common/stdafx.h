// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#if _MSC_FULL_VER < 180030723
#error Please update your build environment to VS2013 with Update 3 or later!
#endif

// Windows Vista is the lowest version we support
#define _WIN32_WINNT 0x0600

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Don't include windows min/max definitions

#include <tchar.h>
#include <vector>
#include <Windows.h>
