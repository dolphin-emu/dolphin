// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "Common/CommonTypes.h"

struct BackPatchInfo
{
  enum
  {
    FLAG_STORE = (1 << 0),
    FLAG_LOAD = (1 << 1),
    FLAG_SIZE_8 = (1 << 2),
    FLAG_SIZE_16 = (1 << 3),
    FLAG_SIZE_32 = (1 << 4),
    FLAG_SIZE_64 = (1 << 5),
    FLAG_FLOAT = (1 << 6),
    FLAG_PAIR = (1 << 7),
    FLAG_REVERSE = (1 << 8),
    FLAG_EXTEND = (1 << 9),
    FLAG_ZERO_256 = (1 << 10),
  };

  static u32 GetFlagSize(u32 flags)
  {
    u32 size = 0;

    if (flags & FLAG_SIZE_8)
      size = 8;
    if (flags & FLAG_SIZE_16)
      size = 16;
    if (flags & FLAG_SIZE_32)
      size = 32;
    if (flags & FLAG_SIZE_64)
      size = 64;
    if (flags & FLAG_ZERO_256)
      size = 256;

    if (flags & FLAG_PAIR)
      size *= 2;

    return size;
  }
};
