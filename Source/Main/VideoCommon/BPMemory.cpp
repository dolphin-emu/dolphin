// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/BPMemory.h"

#include "Common/BitUtils.h"

// BP state
// STATE_TO_SAVE
BPMemory bpmem;

bool BlendMode::UseLogicOp() const
{
  // Logicop bit has lowest priority.
  if (subtract || blendenable || !logicopenable)
    return false;

  // Fast path for Kirby's Return to Dreamland, they use it with dstAlpha.
  if (logicmode == BlendMode::NOOP)
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

float FogParams::GetA() const
{
  if (IsNaNCase())
    return 0.0f;

  // scale mantissa from 11 to 23 bits
  const u32 integral = (static_cast<u32>(a.sign) << 31) | (static_cast<u32>(a.exp) << 23) |
                       (static_cast<u32>(a.mant) << 12);

  return Common::BitCast<float>(integral);
}

float FogParams::GetC() const
{
  if (IsNaNCase())
  {
    constexpr float inf = std::numeric_limits<float>::infinity();
    return !a.sign && !c_proj_fsel.c_sign ? -inf : inf;
  }

  // scale mantissa from 11 to 23 bits
  const u32 integral = (c_proj_fsel.c_sign.Value() << 31) | (c_proj_fsel.c_exp.Value() << 23) |
                       (c_proj_fsel.c_mant.Value() << 12);

  return Common::BitCast<float>(integral);
}
