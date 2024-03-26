// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace JitCommon
{
struct SignedMagic
{
  s32 multiplier;
  u8 shift;
};

// Calculate the constants required to optimize a signed 32-bit integer division.
// Taken from The PowerPC Compiler Writer's Guide and LLVM.
// Divisor must not be -1, 0, 1 or INT_MIN.
SignedMagic SignedDivisionConstants(s32 divisor);

struct UnsignedMagic
{
  u32 multiplier;
  u8 shift;
  bool fast;
};

/// Calculate the constants required to optimize an unsigned 32-bit integer
/// division.
/// Divisor must not be 0, 1, or a power of two.
///
/// Original implementation by calc84maniac.
/// Results are the same as the approach laid out in Hacker's Delight, with an
/// improvement for so-called uncooperative divisors (e.g. 7), as discovered by
/// ridiculousfish.
///
/// See also:
/// https://ridiculousfish.com/blog/posts/labor-of-division-episode-iii.html
/// https://rubenvannieuwpoort.nl/posts/division-by-constant-unsigned-integers
UnsignedMagic UnsignedDivisionConstants(u32 divisor);

}  // namespace JitCommon
