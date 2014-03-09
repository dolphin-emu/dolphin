// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <limits>

#include "Common/MathUtil.h"

template <typename T>
T ClampAndReturn(const T& val, const T& min, const T& max)
{
	T ret = val;
	MathUtil::Clamp(&ret, min, max);
	return ret;
}

TEST(MathUtil, Clamp)
{
	EXPECT_EQ(1, ClampAndReturn(1, 0, 2));
	EXPECT_EQ(1.0, ClampAndReturn(1.0, 0.0, 2.0));

	EXPECT_EQ(2, ClampAndReturn(4, 0, 2));
	EXPECT_EQ(2.0, ClampAndReturn(4.0, 0.0, 2.0));

	EXPECT_EQ(0, ClampAndReturn(-1, 0, 2));
	EXPECT_EQ(0.0, ClampAndReturn(-1.0, 0.0, 2.0));
}

TEST(MathUtil, IsINF)
{
	EXPECT_TRUE(MathUtil::IsINF( std::numeric_limits<double>::infinity()));
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

TEST(MathUtil, Log2)
{
	EXPECT_EQ(0, Log2(1));
	EXPECT_EQ(1, Log2(2));
	EXPECT_EQ(2, Log2(4));
	EXPECT_EQ(3, Log2(8));
	EXPECT_EQ(63, Log2(0x8000000000000000ull));

	// Rounding behavior.
	EXPECT_EQ(3, Log2(15));
	EXPECT_EQ(63, Log2(0xFFFFFFFFFFFFFFFFull));
}
