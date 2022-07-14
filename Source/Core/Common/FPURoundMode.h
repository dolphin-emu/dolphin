// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace FPURoundMode
{
enum class RoundMode
{
  Nearest = 0b00,
  TowardsZero = 0b01,
  TowardsPositiveInfinity = 0b10,
  TowardsNegativeInfinity = 0b11
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
}  // namespace FPURoundMode
