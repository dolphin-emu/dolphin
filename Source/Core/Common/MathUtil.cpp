// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MathUtil.h"

#include <numeric>

// Calculate sum of a float list
float MathFloatVectorSum(const std::vector<float>& Vec)
{
  return std::accumulate(Vec.begin(), Vec.end(), 0.0f);
}
