// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define _WIN32_WINNT 0x501
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500       // Default value is 0x0400
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <algorithm>
