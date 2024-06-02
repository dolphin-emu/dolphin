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

TEST(MathUtil, RectangleEquality)
{
  MathUtil::Rectangle<int> rect_a(1, 1, 4, 7);
  MathUtil::Rectangle<int> rect_b(1, 1, 4, 7);
  EXPECT_EQ(rect_a, rect_b);

  // Left not equal
  MathUtil::Rectangle<int> rect_c(0, 1, 4, 7);
  EXPECT_NE(rect_a, rect_c);

  // Top not equal
  MathUtil::Rectangle<int> rect_d(1, 3, 4, 7);
  EXPECT_NE(rect_a, rect_d);

  // Right not equal
  MathUtil::Rectangle<int> rect_e(1, 1, 2, 7);
  EXPECT_NE(rect_a, rect_e);

  // Bottom not equal
  MathUtil::Rectangle<int> rect_f(1, 1, 4, 9);
  EXPECT_NE(rect_a, rect_f);

  // No coordinates equal
  MathUtil::Rectangle<int> rect_g(0, 3, 2, 9);
  EXPECT_NE(rect_a, rect_g);
}

TEST(MathUtil, RectangleGetWidthSigned)
{
  // left < right
  MathUtil::Rectangle<int> rect_a(2, 1, 3, 2);
  EXPECT_EQ(rect_a.GetWidth(), 1);

  // left > right
  MathUtil::Rectangle<int> rect_b(3, 1, 1, 2);
  EXPECT_EQ(rect_b.GetWidth(), 2);

  // left == right
  MathUtil::Rectangle<int> rect_c(3, 1, 3, 2);
  EXPECT_EQ(rect_c.GetWidth(), 0);

  // Most significant bit differs, left < right
  MathUtil::Rectangle<int> rect_d(-9, 1, 1, 2);
  EXPECT_EQ(rect_d.GetWidth(), 10);

  // Most significant bit differs, left > right
  MathUtil::Rectangle<int> rect_e(1, 1, -6, 2);
  EXPECT_EQ(rect_e.GetWidth(), 7);
}

TEST(MathUtil, RectangleGetWidthUnsigned)
{
  // left < right
  MathUtil::Rectangle<u32> rect_a(1, 1, 6, 2);
  EXPECT_EQ(rect_a.GetWidth(), u32{5});

  // left > right
  MathUtil::Rectangle<u32> rect_b(5, 1, 1, 2);
  EXPECT_EQ(rect_b.GetWidth(), u32{4});

  // left == right
  MathUtil::Rectangle<u32> rect_c(1, 2, 1, 2);
  EXPECT_EQ(rect_c.GetWidth(), u32{0});

  // Most significant bit differs, left < right
  MathUtil::Rectangle<u32> rect_d(2, 1, 0xFFFFFFF5, 2);
  EXPECT_EQ(rect_d.GetWidth(), u32{0xFFFFFFF3});

  // Most significant bit differs, left > right
  MathUtil::Rectangle<u32> rect_e(0xFFFFFFF7, 1, 1, 2);
  EXPECT_EQ(rect_e.GetWidth(), u32{0xFFFFFFF6});
}

TEST(MathUtil, RectangleGetHeightSigned)
{
  // top < bottom
  MathUtil::Rectangle<int> rect_a(1, 1, 2, 3);
  EXPECT_EQ(rect_a.GetHeight(), 2);

  // top > bottom
  MathUtil::Rectangle<int> rect_b(1, 4, 2, 0);
  EXPECT_EQ(rect_b.GetHeight(), 4);

  // top == bottom
  MathUtil::Rectangle<int> rect_c(1, 3, 2, 3);
  EXPECT_EQ(rect_c.GetHeight(), 0);

  // Most significant bit differs, top < bottom
  MathUtil::Rectangle<int> rect_d(1, -2, 2, 1);
  EXPECT_EQ(rect_d.GetHeight(), 3);

  // Most significant bit differs, top > bottom
  MathUtil::Rectangle<int> rect_e(1, 0, 2, -1);
  EXPECT_EQ(rect_e.GetHeight(), 1);
}

TEST(MathUtil, RectangleGetHeightUnsigned)
{
  // top < bottom
  MathUtil::Rectangle<u32> rect_a(1, 1, 2, 2);
  EXPECT_EQ(rect_a.GetHeight(), u32{1});

  // top > bottom
  MathUtil::Rectangle<u32> rect_b(1, 4, 2, 2);
  EXPECT_EQ(rect_b.GetHeight(), u32{2});

  // top == bottom
  MathUtil::Rectangle<u32> rect_c(1, 1, 2, 1);
  EXPECT_EQ(rect_c.GetHeight(), u32{0});

  // Most significant bit differs, top < bottom
  MathUtil::Rectangle<u32> rect_d(1, 2, 2, 0xFFFFFFFB);
  EXPECT_EQ(rect_d.GetHeight(), u32{0xFFFFFFF9});

  // Most significant bit differs, top > bottom
  MathUtil::Rectangle<u32> rect_e(1, 0xFFFFFFF9, 2, 1);
  EXPECT_EQ(rect_e.GetHeight(), u32{0xFFFFFFF8});
}

// TODO: Add unit test coverage for `Rectangle::ClampUL`. (And consider removing
// `Rectangle::ClampLL`, which does not have any callers.)
