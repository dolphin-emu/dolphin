// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Common::FPU
{
enum RoundMode : u32
{
  ROUND_NEAR = 0,
  ROUND_CHOP = 1,
  ROUND_UP = 2,
  ROUND_DOWN = 3
};

void SetSIMDMode(RoundMode rounding_mode, bool non_ieee_mode);

/*
 * There are two different flavors of float to int conversion:
 * _mm_cvtps_epi32() and _mm_cvttps_epi32().
 *
 * The first rounds according to the MXCSR rounding bits.
 * The second one always uses round towards zero.
 */
void SaveSIMDState();
void LoadSIMDState();
void LoadDefaultSIMDState();
}  // namespace Common::FPU
