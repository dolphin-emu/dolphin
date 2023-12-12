// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <limits>
#include <random>

#include <gtest/gtest.h>

#include "Common/BitUtils.h"
#include "Common/FloatUtils.h"

#include "../Core/PowerPC/TestValues.h"

TEST(FloatUtils, IsQNAN)
{
  EXPECT_TRUE(Common::IsQNAN(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(Common::IsQNAN(Common::SNANConstant<double>()));
}

TEST(FloatUtils, IsSNAN)
{
  EXPECT_FALSE(Common::IsSNAN(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_TRUE(Common::IsSNAN(Common::SNANConstant<double>()));
}

TEST(FloatUtils, FlushToZero)
{
  // To test the software implementation we need to make sure FTZ and DAZ are disabled.
  // Using volatile here to ensure the compiler doesn't constant-fold it,
  // we want the multiplication to occur at test runtime.
  volatile float s = std::numeric_limits<float>::denorm_min();
  volatile double d = std::numeric_limits<double>::denorm_min();
  EXPECT_LT(0.f, s * 2);
  EXPECT_LT(0.0, d * 2);

  EXPECT_EQ(+0.0, Common::FlushToZero(+std::numeric_limits<double>::denorm_min()));
  EXPECT_EQ(-0.0, Common::FlushToZero(-std::numeric_limits<double>::denorm_min()));
  EXPECT_EQ(+0.0, Common::FlushToZero(+std::numeric_limits<double>::min() / 2));
  EXPECT_EQ(-0.0, Common::FlushToZero(-std::numeric_limits<double>::min() / 2));
  EXPECT_EQ(std::numeric_limits<double>::min(),
            Common::FlushToZero(std::numeric_limits<double>::min()));
  EXPECT_EQ(std::numeric_limits<double>::max(),
            Common::FlushToZero(std::numeric_limits<double>::max()));
  EXPECT_EQ(+std::numeric_limits<double>::infinity(),
            Common::FlushToZero(+std::numeric_limits<double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<double>::infinity(),
            Common::FlushToZero(-std::numeric_limits<double>::infinity()));

  // Test all subnormals as well as an equally large set of random normal floats.
  std::default_random_engine engine(0);
  std::uniform_int_distribution<u32> dist(0x00800000u, 0x7fffffffu);
  for (u32 i = 0; i <= 0x007fffffu; ++i)
  {
    u32 i_tmp = i;
    EXPECT_EQ(+0.f, Common::FlushToZero(Common::BitCast<float>(i_tmp)));

    i_tmp |= 0x80000000u;
    EXPECT_EQ(-0.f, Common::FlushToZero(Common::BitCast<float>(i_tmp)));

    i_tmp = dist(engine);
    EXPECT_EQ(i_tmp, Common::BitCast<u32>(Common::FlushToZero(Common::BitCast<float>(i_tmp))));

    i_tmp |= 0x80000000u;
    EXPECT_EQ(i_tmp, Common::BitCast<u32>(Common::FlushToZero(Common::BitCast<float>(i_tmp))));
  }
}

TEST(FloatUtils, ApproximateReciprocalSquareRoot)
{
  constexpr std::array<u64, 57> expected_values{
      0x7FF0'0000'0000'0000, 0x617F'FE80'0000'0000, 0x60BF'FE80'0000'0000, 0x5FE0'0008'2C00'0000,
      0x5FDF'FE80'0000'0000, 0x5FDF'FE80'0000'0000, 0x3FEF'FE80'0000'0000, 0x1FF0'0008'2C00'0000,
      0x0000'0000'0000'0000, 0x7FF8'0000'0000'0001, 0x7FFF'FFFF'FFFF'FFFF, 0x7FF8'0000'0000'0000,
      0x7FFF'FFFF'FFFF'FFFF, 0xFFF0'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0xFFF8'0000'0000'0001, 0xFFFF'FFFF'FFFF'FFFF,
      0xFFF8'0000'0000'0000, 0xFFFF'FFFF'FFFF'FFFF, 0x43E6'9FA0'0000'0000, 0x43DF'FE80'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x43E6'9360'6000'0000, 0x43DF'ED30'7000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x44A6'9FA0'0000'0000, 0x4496'9FA0'0000'0000,
      0x448F'FE80'0000'0000, 0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000,
      0x44A6'9360'6000'0000, 0x4496'9360'6000'0000, 0x448F'ED30'7000'0000, 0x7FF8'0000'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x3C06'9FA0'0000'0000, 0x3BFF'FE80'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x43EF'FE80'0000'0000, 0x43F6'9FA0'0000'0000,
      0x7FF8'0000'0000'0000, 0x7FF8'0000'0000'0000, 0x3FEA'2040'0000'0000, 0x3FA0'3108'0000'0000,
      0x7FF8'0000'0000'0000};

  for (size_t i = 0; i < double_test_values.size(); ++i)
  {
    u64 ivalue = double_test_values[i];
    double dvalue = Common::BitCast<double>(ivalue);

    u64 expected = expected_values[i];

    u64 actual = Common::BitCast<u64>(Common::ApproximateReciprocalSquareRoot(dvalue));

    EXPECT_EQ(expected, actual);
  }
}
