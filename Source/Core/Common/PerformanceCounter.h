// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if !defined(_WIN32)

#include <cstdint>

#include "Common/CommonTypes.h"

typedef u64 LARGE_INTEGER;
bool QueryPerformanceCounter(u64* out);
bool QueryPerformanceFrequency(u64* lpFrequency);

#endif
