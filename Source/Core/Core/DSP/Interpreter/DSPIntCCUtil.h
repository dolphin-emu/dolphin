// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#pragma once

#include "Common/CommonTypes.h"

// Anything to do with SR and conditions goes here.

namespace DSP::Interpreter
{
constexpr bool isCarry(u64 val, u64 result)
{
  return val > result;
}

constexpr bool isCarry2(u64 val, u64 result)
{
  return val >= result;
}

constexpr bool isOverflow(s64 val1, s64 val2, s64 res)
{
  return ((val1 ^ res) & (val2 ^ res)) < 0;
}

constexpr bool isOverS32(s64 acc)
{
  return acc != static_cast<s32>(acc);
}
}  // namespace DSP::Interpreter
