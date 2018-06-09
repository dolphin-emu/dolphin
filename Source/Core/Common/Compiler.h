// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__GNUC__) || __clang__
// Disable "unused function" warnings for the ones manually marked as such.
#define UNUSED __attribute__((unused))
#else
// Not sure MSVC even checks this...
#define UNUSED
#endif

#ifndef _WIN32
#define __forceinline inline __attribute__((always_inline))
#endif
