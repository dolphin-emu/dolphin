// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/DivUtils.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace JitCommon
{
SignedMagic SignedDivisionConstants(s32 divisor)
{
  const u32 two31 = 2147483648;

  const u32 ad = std::abs(divisor);
  const u32 t = two31 - (divisor < 0);
  const u32 anc = t - 1 - t % ad;
  u32 q1 = two31 / anc;
  u32 r1 = two31 - q1 * anc;
  u32 q2 = two31 / ad;
  u32 r2 = two31 - q2 * ad;

  s32 p = 31;
  u32 delta;

  do
  {
    p++;

    q1 *= 2;
    r1 *= 2;
    if (r1 >= anc)
    {
      q1++;
      r1 -= anc;
    }

    q2 *= 2;
    r2 *= 2;
    if (r2 >= ad)
    {
      q2++;
      r2 -= ad;
    }
    delta = ad - r2;
  } while (q1 < delta || (q1 == delta && r1 == 0));

  SignedMagic mag;
  mag.multiplier = q2 + 1;
  if (divisor < 0)
    mag.multiplier = -mag.multiplier;
  mag.shift = p - 32;

  return mag;
}

UnsignedMagic UnsignedDivisionConstants(u32 divisor)
{
  u32 shift = 31 - std::countl_zero(divisor);

  u64 magic_dividend = 0x100000000ULL << shift;
  u32 multiplier = magic_dividend / divisor;
  u32 max_quotient = multiplier >> shift;

  // Test for failure in round-up method
  u32 round_up = (u64(multiplier + 1) * (max_quotient * divisor - 1)) >> (shift + 32);
  bool fast = round_up == max_quotient - 1;

  if (fast)
  {
    multiplier++;

    // Use smallest magic number and shift amount possible
    u32 trailing_zeroes = std::min(shift, u32(std::countr_zero(multiplier)));
    multiplier >>= trailing_zeroes;
    shift -= trailing_zeroes;
  }

  UnsignedMagic mag;
  mag.multiplier = multiplier;
  mag.shift = shift;
  mag.fast = fast;

  return mag;
}

}  // namespace JitCommon
