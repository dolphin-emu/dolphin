// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#pragma once

#include "Common/CommonTypes.h"

// Anything to do with SR and conditions goes here.

namespace DSP::Interpreter
{
constexpr bool isCarryAdd(u64 val, u64 result)
{
  return val > result;
}

constexpr bool isCarrySubtract(u64 val, u64 result)
{
  return val >= result;
}

constexpr bool isOverflow(s64 val1, s64 val2, s64 res)
{
  // val1 > 0 and val2 > 0 yet res < 0, or val1 < 0 and val2 < 0 yet res > 0.
  return ((val1 ^ res) & (val2 ^ res)) < 0;
}

constexpr bool isOverS32(s64 acc)
{
  return acc != static_cast<s32>(acc);
}
}  // namespace DSP::Interpreter
