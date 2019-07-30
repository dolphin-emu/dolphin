// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/MathUtil.h"

TEST(MathUtil, IntLog2)
{
  EXPECT_EQ(0, IntLog2(1));
  EXPECT_EQ(1, IntLog2(2));
  EXPECT_EQ(2, IntLog2(4));
  EXPECT_EQ(3, IntLog2(8));
  EXPECT_EQ(63, IntLog2(0x8000000000000000ull));

  // Rounding behavior.
  EXPECT_EQ(3, IntLog2(15));
  EXPECT_EQ(63, IntLog2(0xFFFFFFFFFFFFFFFFull));
}

TEST(MathUtil, NextPowerOf2)
{
  EXPECT_EQ(4U, MathUtil::NextPowerOf2(3));
  EXPECT_EQ(4U, MathUtil::NextPowerOf2(4));
  EXPECT_EQ(8U, MathUtil::NextPowerOf2(6));
  EXPECT_EQ(0x40000000U, MathUtil::NextPowerOf2(0x23456789));
}
