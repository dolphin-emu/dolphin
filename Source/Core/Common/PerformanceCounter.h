// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#if !defined(_WIN32)

#include <cstdint>

#include "Common/CommonTypes.h"

typedef u64 LARGE_INTEGER;
bool QueryPerformanceCounter(u64* out);
bool QueryPerformanceFrequency(u64* lpFrequency);

#endif
