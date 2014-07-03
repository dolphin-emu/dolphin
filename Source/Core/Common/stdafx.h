// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

/*
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500    // Default value is 0x0400
#endif
*/

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Don't include windows min/max definitions

/*
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1
*/
#include <tchar.h>
#include <vector>
#include <windows.h>

