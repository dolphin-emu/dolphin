// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/BPMemory.h"

#include <cstring>

// BP state
// STATE_TO_SAVE
BPMemory bpmem;

float FogParam0::GetA() const
{
  // scale mantissa from 11 to 23 bits
  const u32 integral = (static_cast<u32>(sign) << 31) | (static_cast<u32>(exponent) << 23) |
                       (static_cast<u32>(mantissa) << 12);

  float real;
  std::memcpy(&real, &integral, sizeof(u32));
  return real;
}

float FogParam3::GetC() const
{
  // scale mantissa from 11 to 23 bits
  const u32 integral = (c_sign.Value() << 31) | (c_exp.Value() << 23) | (c_mant.Value() << 12);

  float real;
  std::memcpy(&real, &integral, sizeof(u32));
  return real;
}
