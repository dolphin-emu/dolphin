// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace JitCommon
{
struct Magic
{
  s32 multiplier;
  u8 shift;
};

// Calculate the constants required to optimize a signed 32-bit integer division.
// Taken from The PowerPC Compiler Writer's Guide and LLVM.
// Divisor must not be -1, 0, 1 or INT_MIN.
Magic SignedDivisionConstants(s32 divisor);

}  // namespace JitCommon
