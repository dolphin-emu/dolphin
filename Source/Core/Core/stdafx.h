// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Windows Vista is the lowest version we support
#define _WIN32_WINNT 0x0600

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Don't include windows min/max definitions

#include <algorithm>
