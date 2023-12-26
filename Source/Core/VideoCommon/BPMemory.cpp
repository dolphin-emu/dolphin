// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BPMemory.h"

#include "Common/BitUtils.h"

// BP state
// STATE_TO_SAVE
BPMemory bpmem;

bool BlendMode::UseLogicOp() const
{
  // Blending overrides the logicop bit.
  if (blendenable || !logicopenable)
    return false;

  // Fast path for Kirby's Return to Dreamland, they use it with dstAlpha.
  if (logicmode == LogicOp::NoOp)
    return false;

  return true;
}

bool FogParams::IsNaNCase() const
{
  // Check for the case where both a and c are infinity or NaN.
  // On hardware, this results in the following colors:
  //
  // -------------------------------------------------------
  // |   A   |   C   |  Result  |   A   |   C   |  Result  |
  // -------------------------------------------------------
  // |  inf  |  inf  |  Fogged  |  inf  |  nan  |  Fogged  |
  // |  inf  | -inf  | Unfogged |  inf  | -nan  | Unfogged |
  // | -inf  |  inf  | Unfogged | -inf  |  nan  | Unfogged |
  // | -inf  | -inf  | Unfogged | -inf  | -nan  | Unfogged |
  // -------------------------------------------------------
  // |  nan  |  inf  |  Fogged  |  nan  |  nan  |  Fogged  |
  // |  nan  | -inf  | Unfogged |  nan  | -nan  | Unfogged |
  // | -nan  |  inf  | Unfogged | -nan  |  nan  | Unfogged |
  // | -nan  | -inf  | Unfogged | -nan  | -nan  | Unfogged |
  // -------------------------------------------------------
  //
  // We replicate this by returning A=0, and C=inf for the inf/inf case, otherwise -inf.
  // This ensures we do not pass a NaN to the GPU, and -inf/inf clamp to 0/1 respectively.
  return a.exp == 255 && c_proj_fsel.c_exp == 255;
}

float FogParam0::FloatValue() const
{
  // scale mantissa from 11 to 23 bits
  const u32 integral = (sign << 31) | (exp << 23) | (mant << 12);
  return Common::BitCast<float>(integral);
}

float FogParam3::FloatValue() const
{
  // scale mantissa from 11 to 23 bits
  const u32 integral = (c_sign << 31) | (c_exp << 23) | (c_mant << 12);
  return Common::BitCast<float>(integral);
}

float FogParams::GetA() const
{
  if (IsNaNCase())
    return 0.0f;

  return a.FloatValue();
}

float FogParams::GetC() const
{
  if (IsNaNCase())
  {
    constexpr float inf = std::numeric_limits<float>::infinity();
    return !a.sign && !c_proj_fsel.c_sign ? -inf : inf;
  }

  return c_proj_fsel.FloatValue();
}
