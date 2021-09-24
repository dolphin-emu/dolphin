// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Core/PowerPC/JitCommon/DivUtils.h"

using namespace JitCommon;

TEST(DivUtils, Signed)
{
  Magic m3 = SignedDivisionConstants(3);
  Magic m5 = SignedDivisionConstants(5);
  Magic m7 = SignedDivisionConstants(7);
  Magic minus3 = SignedDivisionConstants(-3);
  Magic minus5 = SignedDivisionConstants(-5);
  Magic minus7 = SignedDivisionConstants(-7);

  EXPECT_EQ(0x55555556, m3.multiplier);
  EXPECT_EQ(0, m3.shift);
  EXPECT_EQ(0x66666667, m5.multiplier);
  EXPECT_EQ(1, m5.shift);
  EXPECT_EQ(-0x6DB6DB6D, m7.multiplier);
  EXPECT_EQ(2, m7.shift);

  EXPECT_EQ(-0x55555556, minus3.multiplier);
  EXPECT_EQ(0, minus3.shift);
  EXPECT_EQ(-0x66666667, minus5.multiplier);
  EXPECT_EQ(1, minus5.shift);
  EXPECT_EQ(0x6DB6DB6D, minus7.multiplier);
  EXPECT_EQ(2, minus7.shift);
}
