// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/MathUtil.h"
#include "Common/Rational.h"

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

TEST(MathUtil, Compare3Way)
{
  EXPECT_TRUE(std::is_lt(MathUtil::Compare3Way(-1, 1u)));
  EXPECT_TRUE(std::is_gteq(MathUtil::Compare3Way(5u, -17ll)));
  EXPECT_TRUE(std::is_eq(MathUtil::Compare3Way(42ull, 42)));
  EXPECT_TRUE(std::is_neq(MathUtil::Compare3Way(s32(-1), u32(-1))));
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

  // Note that values in the range [2147483584, 2147483776] have an equivalent float representation.
  EXPECT_EQ(std::numeric_limits<s32>::max(), MathUtil::SaturatingCast<s32>(2147483648.f));
  EXPECT_EQ(std::numeric_limits<s32>::min(), MathUtil::SaturatingCast<s32>(-2147483649.f));

  // Cast from a signed integer type to a smaller signed integer type
  EXPECT_EQ(-128, (MathUtil::SaturatingCast<s8, int>(-129)));
}

TEST(MathUtil, RectangleEquality)
{
  MathUtil::Rectangle rect_a(1, 1, 4, 7);
  MathUtil::Rectangle rect_b(1, 1, 4, 7);
  EXPECT_EQ(rect_a, rect_b);

  // Left not equal
  MathUtil::Rectangle rect_c(0, 1, 4, 7);
  EXPECT_NE(rect_a, rect_c);

  // Top not equal
  MathUtil::Rectangle rect_d(1, 3, 4, 7);
  EXPECT_NE(rect_a, rect_d);

  // Right not equal
  MathUtil::Rectangle rect_e(1, 1, 2, 7);
  EXPECT_NE(rect_a, rect_e);

  // Bottom not equal
  MathUtil::Rectangle rect_f(1, 1, 4, 9);
  EXPECT_NE(rect_a, rect_f);

  // No coordinates equal
  MathUtil::Rectangle rect_g(0, 3, 2, 9);
  EXPECT_NE(rect_a, rect_g);
}

TEST(MathUtil, RectangleGetWidthSigned)
{
  // left < right
  MathUtil::Rectangle rect_a(2, 1, 3, 2);
  EXPECT_EQ(rect_a.GetWidth(), 1);

  // left > right
  MathUtil::Rectangle rect_b(3, 1, 1, 2);
  EXPECT_EQ(rect_b.GetWidth(), 2);

  // left == right
  MathUtil::Rectangle rect_c(3, 1, 3, 2);
  EXPECT_EQ(rect_c.GetWidth(), 0);

  // Most significant bit differs, left < right
  MathUtil::Rectangle rect_d(-9, 1, 1, 2);
  EXPECT_EQ(rect_d.GetWidth(), 10);

  // Most significant bit differs, left > right
  MathUtil::Rectangle rect_e(1, 1, -6, 2);
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
  MathUtil::Rectangle rect_a(1, 1, 2, 3);
  EXPECT_EQ(rect_a.GetHeight(), 2);

  // top > bottom
  MathUtil::Rectangle rect_b(1, 4, 2, 0);
  EXPECT_EQ(rect_b.GetHeight(), 4);

  // top == bottom
  MathUtil::Rectangle rect_c(1, 3, 2, 3);
  EXPECT_EQ(rect_c.GetHeight(), 0);

  // Most significant bit differs, top < bottom
  MathUtil::Rectangle rect_d(1, -2, 2, 1);
  EXPECT_EQ(rect_d.GetHeight(), 3);

  // Most significant bit differs, top > bottom
  MathUtil::Rectangle rect_e(1, 0, 2, -1);
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

TEST(MathUtil, Rational)
{
  using MathUtil::Rational;

  // Integer
  const auto r5 = 65 * Rational{8, 13} / 8;
  EXPECT_TRUE(r5.IsInteger());
  EXPECT_EQ(r5, 5);
  EXPECT_EQ(int(r5), 5);
  EXPECT_EQ(r5, Rational{10} * 0.5f);
  EXPECT_NE(r5, Rational(6));

  // Non-Integer
  const auto r5_2 = Rational(5, 2);
  EXPECT_FALSE(r5_2.IsInteger());
  EXPECT_EQ(int(r5_2), 2);
  EXPECT_EQ(r5_2, 12.5f / r5);

  // True/False
  EXPECT_TRUE(r5_2);
  EXPECT_FALSE(r5_2 * 0);
  EXPECT_FALSE(!r5_2);

  // Negative values
  EXPECT_EQ(Rational(-4, -3), Rational(4, 3));
  EXPECT_EQ(Rational(-1, 10), Rational(1, -10));
  EXPECT_TRUE(Rational(-5, 1).IsInteger());
  EXPECT_TRUE(Rational(5, -1).IsInteger());
  EXPECT_NE(r5, -r5);

  // Conversion to/from float
  const Rational r3p5(3.5);
  EXPECT_EQ(r3p5.numerator, 7);
  EXPECT_EQ(r3p5.denominator, 2);
  const Rational neg3p5(-3.5);
  EXPECT_EQ(neg3p5.numerator, -7);
  EXPECT_EQ(neg3p5.denominator, 2);
  EXPECT_EQ(float(r5_2), 2.5);
  EXPECT_EQ(float(-r5_2), -2.5f);
  EXPECT_EQ(r5_2, Rational(2.5f));
  EXPECT_EQ(-r5_2, Rational(-2.5));

  EXPECT_NE(r5_2, Rational(2.500001f));

  // Fraction reduction
  const Rational r15_6{15, 6};
  EXPECT_EQ(r15_6, r5_2);
  const auto f15_6_reduced = r15_6.Reduced();
  EXPECT_EQ(f15_6_reduced.numerator, 5);
  EXPECT_EQ(f15_6_reduced.denominator, 2);

  // Approximations
  EXPECT_EQ(Rational(0.3).Approximated(1000'000), Rational(3, 10));
  EXPECT_EQ(Rational(3, 10).Approximated(9), Rational(2, 7));
  EXPECT_EQ(Rational(-33, 100).Approximated(20), Rational(-1, 3));
  EXPECT_EQ(Rational(0.33).Approximated(10), Rational(1, 3));
  EXPECT_EQ(Rational(1, -100).Approximated(10), Rational(0, 1));
  EXPECT_EQ(Rational(6, -100).Approximated(10), Rational(-1, 10));
  EXPECT_EQ(Rational(101).Approximated(20), Rational(20, 1));
  EXPECT_EQ(Rational<s16>(std::numeric_limits<s16>::max()).Approximated(18), Rational(18, 1));

  constexpr auto s64_max = std::numeric_limits<s64>::max();
  EXPECT_EQ(Rational<s64>(-s64_max).Approximated(13), Rational(-13, 1));
  EXPECT_EQ(Rational<s64>(1, s64_max).Approximated(s64_max), Rational<s64>(1, s64_max));

  // Addition/Subtraction
  EXPECT_EQ(-r5_2 + -r15_6, Rational(-5));
  EXPECT_EQ(2.5f - -r15_6, Rational(5));
  EXPECT_EQ(+r3p5 - 2, 1.5f);
  EXPECT_EQ(r3p5 - 7, -3.5);
  EXPECT_EQ(r3p5 - u8(2), 1.5);
  EXPECT_EQ(r3p5 - 3ull, 0.5);
  EXPECT_EQ(r3p5 - Rational<u64>(1), 2.5);
  EXPECT_EQ(Rational<u8>(6) - 5.5f, 0.5);
  EXPECT_EQ(Rational<u8>(6) + Rational(-5, -5), 7);
  EXPECT_EQ(Rational<u8>(6) - Rational<s64>(3, -3), 7);

  // Inc/Dec
  Rational f7_3_inc{7, 3};
  EXPECT_EQ(f7_3_inc++, 2 + Rational(1, 3));
  EXPECT_EQ(++f7_3_inc, 4 + Rational(1, 3));
  EXPECT_EQ(f7_3_inc--, Rational(1, 3) + 4);
  EXPECT_EQ(--f7_3_inc, Rational(1, 3) + 2);

  // Multiplication/Division
  EXPECT_EQ(r5_2 * 3, Rational(7.5));
  EXPECT_EQ(r5_2 * r5_2, 6.25);
  EXPECT_EQ(7 / r15_6, 2 + Rational(8, 10));
  EXPECT_EQ(r3p5 / r15_6, 12 - Rational(106, 10));
  EXPECT_EQ(r3p5 / 2, 1.75);
  EXPECT_EQ(int(r3p5 / 2), 1);
  EXPECT_EQ(Rational(-1, -3) * 2ull, Rational(2, 3));
  auto ru77 = Rational(77u);
  ru77 /= Rational(-7, -1);
  EXPECT_EQ(ru77, 11);

  // Modulo
  EXPECT_EQ(r3p5 % 2, 1.5f);
  EXPECT_EQ(r3p5 % -2, Rational(3, 2));
  EXPECT_EQ(-r3p5 % 2, Rational(3, -2));
  EXPECT_EQ(-r3p5 % -2, -1.5f);

  // Mediant
  EXPECT_EQ(r5_2 & Rational(2, 1), Rational(7, 3));
  EXPECT_EQ(Rational(11, 5) & Rational(3, -1), Rational(14, 4));

  // Comparison
  EXPECT_TRUE(Rational(-5, 101) < 0);
  EXPECT_TRUE(Rational(5, -101) < 0u);
  EXPECT_TRUE(Rational(-5, -101) > 0);
  EXPECT_TRUE(Rational(7, 5) > Rational(7, 6));
  EXPECT_TRUE(Rational(1, 3) < Rational(-2, -3));
  EXPECT_TRUE(Rational(0.5) != Rational(2, 2));
  EXPECT_TRUE(Rational(10, 3) == 3 + Rational(2, 6));
  EXPECT_TRUE(3 >= Rational(6, 2));
  EXPECT_TRUE(Rational(6, 2) < 4.0);

  // Conversions use SaturatingCast
  EXPECT_EQ(Rational<u32>(Rational(-5)), 0);
  EXPECT_EQ(Rational<u8>(Rational(1, 1000)), Rational(1, 255));
  EXPECT_EQ(Rational<u32>(-9.f), 0);
  EXPECT_EQ(Rational<s16>(std::pow(2.1, 18.0)), std::numeric_limits<s16>::max());
  EXPECT_EQ(Rational<s16>(-std::pow(2.1, 18.0)), std::numeric_limits<s16>::min());
  // Smallest positive non-zero float produces the smallest positive non-zero Rational.
  EXPECT_EQ(Rational<s16>(std::numeric_limits<double>::denorm_min()),
            Rational<s16>(1, std::numeric_limits<s16>::max()));

  // Mixing types uses common_type
  const auto big_result = s64(-1000) * Rational<u16>{3000, 3};
  static_assert(std::is_same_v<decltype(big_result), const Rational<s64>>);
  EXPECT_TRUE(big_result == -1000'000);
  EXPECT_EQ(Rational<u32>(Rational(-6, -2)), 3);
  EXPECT_EQ(Rational(5u) * Rational(-1), 0);
  EXPECT_EQ(Rational(5u) * Rational(-1ll), -5);

  // Works at compile time
  static_assert(Rational(8, 2) + Rational(-6, 123) + 4 == Rational(326, 41));
}

// TODO: Add unit test coverage for `Rectangle::ClampUL`. (And consider removing
// `Rectangle::ClampLL`, which does not have any callers.)
