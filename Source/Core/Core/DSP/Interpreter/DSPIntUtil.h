// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace DSP::Interpreter
{
// ---------------------------------------------------------------------------------------
// --- ACC - main accumulators (40-bit)
// ---------------------------------------------------------------------------------------

// s64 -> s40
inline s64 dsp_convert_long_acc(s64 val)
{
  return ((val << 24) >> 24);
}

inline s64 dsp_round_long_acc(s64 val)
{
  if ((val & 0x10000) != 0)
    val = (val + 0x8000) & ~0xffff;
  else
    val = (val + 0x7fff) & ~0xffff;

  return val;
}
}  // namespace DSP::Interpreter
