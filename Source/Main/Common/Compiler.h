// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// TODO: Replace this with [[maybe_unused]] directly when GCC 7 and clang 3.9
//       are hard requirements.
#if defined(__GNUC__) || __clang__
// Disable "unused function" warnings for the ones manually marked as such.
#define DOLPHIN_UNUSED __attribute__((unused))
#else
#define DOLPHIN_UNUSED [[maybe_unused]]
#endif

#ifdef _WIN32
#define DOLPHIN_FORCE_INLINE __forceinline
#else
#define DOLPHIN_FORCE_INLINE inline __attribute__((always_inline))
#endif
