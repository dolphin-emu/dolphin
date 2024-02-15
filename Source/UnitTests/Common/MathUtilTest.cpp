// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/MathUtil.h"

TEST(MathUtil, IntLog2)
{
  EXPECT_EQ(0, MathUtil::IntLog2(1));
  EXPECT_EQ(1, MathUtil::IntLog2(2));
  EXPECT_EQ(2, MathUtil::IntLog2(4));
  EXPECT_EQ(3, MathUtil::IntLog2(8));
  EXPECT_EQ(63, MathUtil::IntLog2(0x8000000000000000ull));

  // Rounding behavior.
  EXPECT_EQ(3, MathUtil::IntLog2(15));
  EXPECT_EQ(63, MathUtil::IntLog2(0xFFFFFFFFFFFFFFFFull));
}

TEST(MathUtil, NextPowerOf2)
{
  EXPECT_EQ(4U, MathUtil::NextPowerOf2(3));
  EXPECT_EQ(4U, MathUtil::NextPowerOf2(4));
  EXPECT_EQ(8U, MathUtil::NextPowerOf2(6));
  EXPECT_EQ(0x40000000U, MathUtil::NextPowerOf2(0x23456789));
}

TEST(MathUtil, SaturatingCast)
{
  // Cast from an integer type to a smaller type
  EXPECT_EQ(255u, (MathUtil::SaturatingCast<u8, int>(1000)));
  EXPECT_EQ(255u, (MathUtil::SaturatingCast<u8, u16>(1000u)));
  EXPECT_EQ(255u, (MathUtil::SaturatingCast<u8, std::size_t>(1000)));

  // Cast from a signed integer type
  EXPECT_EQ(0u, (MathUtil::SaturatingCast<u8, int>(-1)));
  EXPECT_EQ(0u, (MathUtil::SaturatingCast<u8, int>(-1000)));
  EXPECT_EQ(0u, (MathUtil::SaturatingCast<u32, int>(-1)));
  EXPECT_EQ(-1000, (MathUtil::SaturatingCast<s16, int>(-1000)));
  EXPECT_EQ(-1000, (MathUtil::SaturatingCast<int, int>(-1000)));
  EXPECT_EQ(-1000, (MathUtil::SaturatingCast<s64, int>(-1000)));

  // Cast from an unsigned integer type to a smaller integer type
  EXPECT_EQ(0x7fff, (MathUtil::SaturatingCast<s16, u32>(0xffffffffu)));
  EXPECT_EQ(0x7fffffff, (MathUtil::SaturatingCast<int, u32>(0xffffffffu)));

  // Cast from a floating point type to an integer type
  EXPECT_EQ(255u, MathUtil::SaturatingCast<u8>(1234.0));
  EXPECT_EQ(0u, MathUtil::SaturatingCast<u8>(-1234.0));
  EXPECT_EQ(127, MathUtil::SaturatingCast<s8>(5678.0));
  EXPECT_EQ(-128, MathUtil::SaturatingCast<s8>(-5678.0));
  EXPECT_EQ(65535u, MathUtil::SaturatingCast<u16>(999999.0));

  // Negative zero
  EXPECT_EQ(0u, MathUtil::SaturatingCast<u8>(0.0));
  EXPECT_EQ(0u, MathUtil::SaturatingCast<u8>(-0.0));
  EXPECT_EQ(0, MathUtil::SaturatingCast<s8>(0.0));
  EXPECT_EQ(0, MathUtil::SaturatingCast<s8>(-0.0));

  // Edge cases
  EXPECT_EQ(std::numeric_limits<s32>::max(),
            MathUtil::SaturatingCast<s32>(std::numeric_limits<float>::infinity()));
  EXPECT_EQ(std::numeric_limits<s32>::min(),
            MathUtil::SaturatingCast<s32>(-std::numeric_limits<float>::infinity()));
  // 16777217 = 2^24 + 1 is the first integer that cannot be represented correctly with a f32.
  EXPECT_EQ(16777216, MathUtil::SaturatingCast<s32>(float(16777216)));
  EXPECT_EQ(16777216, MathUtil::SaturatingCast<s32>(float(16777217)));
}
