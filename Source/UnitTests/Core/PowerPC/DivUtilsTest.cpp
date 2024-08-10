// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Core/PowerPC/JitCommon/DivUtils.h"

using namespace JitCommon;

TEST(DivUtils, Signed)
{
  const auto [multiplier_m3, shift_m3] = SignedDivisionConstants(3);
  const auto [multiplier_m5, shift_m5] = SignedDivisionConstants(5);
  const auto [multiplier_m7, shift_m7] = SignedDivisionConstants(7);
  const auto [multiplier_minus3, shift_minus3] = SignedDivisionConstants(-3);
  const auto [multiplier_minus5, shift_minus5] = SignedDivisionConstants(-5);
  const auto [multiplier_minus7, shift_minus7] = SignedDivisionConstants(-7);

  EXPECT_EQ(0x55555556, multiplier_m3);
  EXPECT_EQ(0, shift_m3);
  EXPECT_EQ(0x66666667, multiplier_m5);
  EXPECT_EQ(1, shift_m5);
  EXPECT_EQ(-0x6DB6DB6D, multiplier_m7);
  EXPECT_EQ(2, shift_m7);

  EXPECT_EQ(-0x55555556, multiplier_minus3);
  EXPECT_EQ(0, shift_minus3);
  EXPECT_EQ(-0x66666667, multiplier_minus5);
  EXPECT_EQ(1, shift_minus5);
  EXPECT_EQ(0x6DB6DB6D, multiplier_minus7);
  EXPECT_EQ(2, shift_minus7);
}

TEST(DivUtils, Unsigned)
{
  const auto [multiplier_m3, shift_m3, fast_m3] = UnsignedDivisionConstants(3);
  const auto [multiplier_m5, shift_m5, fast_m5] = UnsignedDivisionConstants(5);
  const auto [multiplier_m7, shift_m7, fast_m7] = UnsignedDivisionConstants(7);
  const auto [multiplier_m9, shift_m9, fast_m9] = UnsignedDivisionConstants(9);
  const auto [multiplier_m19, shift_m19, fast_m19] = UnsignedDivisionConstants(19);

  EXPECT_EQ(0xAAAAAAABU, multiplier_m3);
  EXPECT_EQ(1, shift_m3);
  EXPECT_TRUE(fast_m3);

  EXPECT_EQ(0xCCCCCCCDU, multiplier_m5);
  EXPECT_EQ(2, shift_m5);
  EXPECT_TRUE(fast_m5);

  EXPECT_EQ(0x92492492U, multiplier_m7);
  EXPECT_EQ(2, shift_m7);
  EXPECT_FALSE(fast_m7);

  EXPECT_EQ(0x38E38E39U, multiplier_m9);
  EXPECT_EQ(1, shift_m9);
  EXPECT_TRUE(fast_m9);

  EXPECT_EQ(0xD79435E5U, multiplier_m19);
  EXPECT_EQ(4, shift_m19);
  EXPECT_FALSE(fast_m19);
}
