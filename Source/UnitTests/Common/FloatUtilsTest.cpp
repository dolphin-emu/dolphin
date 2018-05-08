// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>
#include <random>

#include <gtest/gtest.h>

#include "Common/FloatUtils.h"

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
  // Casting away the volatile attribute is required in order for msvc to resolve this to the
  // correct instance of the comparison function.
  EXPECT_LT(0.f, (float)(s * 2));
  EXPECT_LT(0.0, (double)(d * 2));

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
    Common::IntFloat x(i);
    EXPECT_EQ(+0.f, Common::FlushToZero(x.f));

    x.i = i | 0x80000000u;
    EXPECT_EQ(-0.f, Common::FlushToZero(x.f));

    x.i = dist(engine);
    Common::IntFloat y(Common::FlushToZero(x.f));
    EXPECT_EQ(x.i, y.i);

    x.i |= 0x80000000u;
    y.f = Common::FlushToZero(x.f);
    EXPECT_EQ(x.i, y.i);
  }
}
