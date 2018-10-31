// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This header contains type definitions that are shared between the Dolphin core and
// other parts of the code. Any definitions that are only used by the core should be
// placed in "Common.h" instead.

#pragma once

#include <cstdint>

#ifdef _WIN32
#include <tchar.h>
#else
// For using Windows lock code
#define TCHAR char
#define LONG int
#endif

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
