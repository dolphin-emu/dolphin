// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Core/PowerPC/JitCommon/DivUtils.h"

using namespace JitCommon;

TEST(DivUtils, Signed)
{
  SignedMagic m3 = SignedDivisionConstants(3);
  SignedMagic m5 = SignedDivisionConstants(5);
  SignedMagic m7 = SignedDivisionConstants(7);
  SignedMagic minus3 = SignedDivisionConstants(-3);
  SignedMagic minus5 = SignedDivisionConstants(-5);
  SignedMagic minus7 = SignedDivisionConstants(-7);

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

TEST(DivUtils, Unsigned)
{
  UnsignedMagic m3 = UnsignedDivisionConstants(3);
  UnsignedMagic m5 = UnsignedDivisionConstants(5);
  UnsignedMagic m7 = UnsignedDivisionConstants(7);
  UnsignedMagic m9 = UnsignedDivisionConstants(9);
  UnsignedMagic m19 = UnsignedDivisionConstants(19);

  EXPECT_EQ(0xAAAAAAABU, m3.multiplier);
  EXPECT_EQ(1, m3.shift);
  EXPECT_TRUE(m3.fast);

  EXPECT_EQ(0xCCCCCCCDU, m5.multiplier);
  EXPECT_EQ(2, m5.shift);
  EXPECT_TRUE(m5.fast);

  EXPECT_EQ(0x92492492U, m7.multiplier);
  EXPECT_EQ(2, m7.shift);
  EXPECT_FALSE(m7.fast);

  EXPECT_EQ(0x38E38E39U, m9.multiplier);
  EXPECT_EQ(1, m9.shift);
  EXPECT_TRUE(m9.fast);

  EXPECT_EQ(0xD79435E5U, m19.multiplier);
  EXPECT_EQ(4, m19.shift);
  EXPECT_FALSE(m19.fast);
}
