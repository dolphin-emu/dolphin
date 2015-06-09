// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>
#include <random>
#include <gtest/gtest.h>

#include "Common/MathUtil.h"

TEST(MathUtil, Clamp)
{
	EXPECT_EQ(1, MathUtil::Clamp(1, 0, 2));
	EXPECT_EQ(1.0, MathUtil::Clamp(1.0, 0.0, 2.0));

	EXPECT_EQ(2, MathUtil::Clamp(4, 0, 2));
	EXPECT_EQ(2.0, MathUtil::Clamp(4.0, 0.0, 2.0));

	EXPECT_EQ(0, MathUtil::Clamp(-1, 0, 2));
	EXPECT_EQ(0.0, MathUtil::Clamp(-1.0, 0.0, 2.0));
}

TEST(MathUtil, IsINF)
{
	EXPECT_TRUE(MathUtil::IsINF(+std::numeric_limits<double>::infinity()));
	EXPECT_TRUE(MathUtil::IsINF(-std::numeric_limits<double>::infinity()));
}

TEST(MathUtil, IsNAN)
{
	EXPECT_TRUE(MathUtil::IsNAN(std::numeric_limits<double>::quiet_NaN()));
	EXPECT_TRUE(MathUtil::IsNAN(std::numeric_limits<double>::signaling_NaN()));
}

TEST(MathUtil, IsQNAN)
{
	EXPECT_TRUE(MathUtil::IsQNAN(std::numeric_limits<double>::quiet_NaN()));
	EXPECT_FALSE(MathUtil::IsQNAN(std::numeric_limits<double>::signaling_NaN()));
}

TEST(MathUtil, IsSNAN)
{
	EXPECT_FALSE(MathUtil::IsSNAN(std::numeric_limits<double>::quiet_NaN()));
	EXPECT_TRUE(MathUtil::IsSNAN(std::numeric_limits<double>::signaling_NaN()));
}

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

TEST(MathUtil, FlushToZero)
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

	EXPECT_EQ(+0.0, MathUtil::FlushToZero(+std::numeric_limits<double>::denorm_min()));
	EXPECT_EQ(-0.0, MathUtil::FlushToZero(-std::numeric_limits<double>::denorm_min()));
	EXPECT_EQ(+0.0, MathUtil::FlushToZero(+std::numeric_limits<double>::min() / 2));
	EXPECT_EQ(-0.0, MathUtil::FlushToZero(-std::numeric_limits<double>::min() / 2));
	EXPECT_EQ(std::numeric_limits<double>::min(), MathUtil::FlushToZero(std::numeric_limits<double>::min()));
	EXPECT_EQ(std::numeric_limits<double>::max(), MathUtil::FlushToZero(std::numeric_limits<double>::max()));
	EXPECT_EQ(+std::numeric_limits<double>::infinity(), MathUtil::FlushToZero(+std::numeric_limits<double>::infinity()));
	EXPECT_EQ(-std::numeric_limits<double>::infinity(), MathUtil::FlushToZero(-std::numeric_limits<double>::infinity()));

	// Test all subnormals as well as an equally large set of random normal floats.
	std::default_random_engine engine(0);
	std::uniform_int_distribution<u32> dist(0x00800000u, 0x7fffffffu);
	for (u32 i = 0; i <= 0x007fffffu; ++i)
	{
		MathUtil::IntFloat x(i);
		EXPECT_EQ(+0.f, MathUtil::FlushToZero(x.f));

		x.i = i | 0x80000000u;
		EXPECT_EQ(-0.f, MathUtil::FlushToZero(x.f));

		x.i = dist(engine);
		MathUtil::IntFloat y(MathUtil::FlushToZero(x.f));
		EXPECT_EQ(x.i, y.i);

		x.i |= 0x80000000u;
		y.f = MathUtil::FlushToZero(x.f);
		EXPECT_EQ(x.i, y.i);
	}
}
