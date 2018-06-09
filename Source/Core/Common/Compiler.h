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

#ifdef _WIN32
#define DOLPHIN_FORCE_INLINE __forceinline
#else
#define DOLPHIN_FORCE_INLINE inline __attribute__((always_inline))
#endif
