// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include "Common/CommonTypes.h"

namespace DSP
{
// super fast breakpoints for a limited range.
// To be used interchangeably with the BreakPoints class.
class DSPBreakpoints
{
public:
  DSPBreakpoints() { Clear(); }
  // is address breakpoint
  bool IsAddressBreakPoint(const u32 addr) const { return b[addr] != 0; }
  // AddBreakPoint
  bool Add(const u32 addr, const bool temp = false)
  {
    const bool was_one = b[addr] != 0;

    if (!was_one)
    {
      b[addr] = temp ? 2 : 1;
      return true;
    }
    else
    {
      return false;
    }
  }

  // Remove Breakpoint
  bool Remove(const u32 addr)
  {
    const bool was_one = b[addr] != 0;
    b[addr] = 0;
    return was_one;
  }

  void Clear() { memset(b, 0, sizeof(b)); }
  void DeleteByAddress(const u32 addr) { b[addr] = 0; }

private:
  u8 b[65536];
};
}  // namespace DSP
