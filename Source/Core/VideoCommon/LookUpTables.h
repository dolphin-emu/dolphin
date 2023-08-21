// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

constexpr u8 Convert3To8(u8 v)
{
  // Swizzle bits: 00000123 -> 12312312
  return (v << 5) | (v << 2) | (v >> 1);
}

constexpr u8 Convert4To8(u8 v)
{
  // Swizzle bits: 00001234 -> 12341234
  return (v << 4) | v;
}

constexpr u8 Convert5To8(u8 v)
{
  // Swizzle bits: 00012345 -> 12345123
  return (v << 3) | (v >> 2);
}

constexpr u8 Convert6To8(u8 v)
{
  // Swizzle bits: 00123456 -> 12345612
  return (v << 2) | (v >> 4);
}
