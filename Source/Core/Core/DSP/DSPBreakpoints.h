// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

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
  bool IsAddressBreakPoint(u32 addr)
  {
    if (!InBounds(addr)) [[unlikely]]
      return false;

    return breakpoints[Index(addr)] != BreakpointStatus::None;
  }
  // AddBreakPoint
  bool Add(u32 addr, bool temp = false)
  {
    if (!InBounds(addr)) [[unlikely]]
      return false;

    bool was_one = breakpoints[Index(addr)] != BreakpointStatus::None;

    if (!was_one) [[likely]]
    {
      breakpoints[Index(addr)] = temp ? BreakpointStatus::Temporary : BreakpointStatus::Permanent;
      return true;
    }
    else
    {
      return false;
    }
  }

  // Remove Breakpoint
  bool Remove(u32 addr)
  {
    if (!InBounds(addr)) [[unlikely]]
      return false;

    const bool was_one = breakpoints[Index(addr)] != BreakpointStatus::None;
    breakpoints[Index(addr)] = BreakpointStatus::None;
    return was_one;
  }

  void Clear() { breakpoints.fill(BreakpointStatus::None); }
  void DeleteByAddress(u32 addr)
  {
    if (InBounds(addr)) [[likely]]
      breakpoints[Index(addr)] = BreakpointStatus::None;
  }

private:
  static constexpr u32 START_ADDR = 0;
  static constexpr u32 END_ADDR = UINT16_MAX;
  static constexpr u32 NUM_ENTRIES = END_ADDR - START_ADDR + 1;
  constexpr bool InBounds(u32 addr) { return START_ADDR <= addr && addr <= END_ADDR; }
  constexpr size_t Index(u32 addr) { return addr - START_ADDR; }

  enum class BreakpointStatus : u8
  {
    None,
    Permanent,
    Temporary,
  };
  std::array<BreakpointStatus, NUM_ENTRIES> breakpoints;
};
}  // namespace DSP
