// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#if defined(_WIN32)
#include <profileapi.h>
#else

typedef u64 LARGE_INTEGER;
bool QueryPerformanceCounter(u64* out);
bool QueryPerformanceFrequency(u64* lpFrequency);

#endif

u64 QueryCachedPerformanceFrequency();
