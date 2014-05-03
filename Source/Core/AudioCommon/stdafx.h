// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
/*
#ifdef HAVE_DXSDK_JUNE_2010
#define _WIN32_WINNT 0x501
#else
#pragma message("Resulting binary will be compatible with DirectX >= Windows 8. Install the June 2010 DirectX SDK if you want to build for older environments.")
#define _WIN32_WINNT 0x602
#endif
*/
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Don't include windows min/max definitions

#include <algorithm>
#include <functional>
