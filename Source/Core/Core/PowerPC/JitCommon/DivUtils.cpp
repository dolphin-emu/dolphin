// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/DivUtils.h"

#include <cstdlib>

namespace JitCommon
{
Magic SignedDivisionConstants(s32 d)
{
  const u32 two31 = 2147483648;

  const u32 ad = std::abs(d);
  const u32 t = two31 - (d < 0);
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

  Magic mag;
  mag.multiplier = q2 + 1;
  if (d < 0)
    mag.multiplier = -mag.multiplier;
  mag.shift = p - 32;

  return mag;
}

}  // namespace JitCommon
