// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

enum CCFlags
{
  CC_EQ = 0,      // Equal
  CC_NEQ,         // Not equal
  CC_CS,          // Carry Set
  CC_CC,          // Carry Clear
  CC_MI,          // Minus (Negative)
  CC_PL,          // Plus
  CC_VS,          // Overflow
  CC_VC,          // No Overflow
  CC_HI,          // Unsigned higher
  CC_LS,          // Unsigned lower or same
  CC_GE,          // Signed greater than or equal
  CC_LT,          // Signed less than
  CC_GT,          // Signed greater than
  CC_LE,          // Signed less than or equal
  CC_AL,          // Always (unconditional) 14
  CC_HS = CC_CS,  // Alias of CC_CS  Unsigned higher or same
  CC_LO = CC_CC,  // Alias of CC_CC  Unsigned lower
};
constexpr u32 NO_COND = 0xE0000000;
