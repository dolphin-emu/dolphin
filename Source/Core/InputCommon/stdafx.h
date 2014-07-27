// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Windows Vista is the lowest version we support
// BUT We need to use the headers for Win8+ XInput, so Win8 version is used here.
// The XInput code will gracefully fallback to older versions if the new one
// isn't available at runtime.
#define _WIN32_WINNT 0x0602

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Don't include windows min/max definitions

#include <algorithm>
