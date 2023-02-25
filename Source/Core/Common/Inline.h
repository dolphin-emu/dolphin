// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MSC_VER
#define DOLPHIN_FORCE_INLINE __forceinline
#else
#define DOLPHIN_FORCE_INLINE inline __attribute__((always_inline))
#endif
