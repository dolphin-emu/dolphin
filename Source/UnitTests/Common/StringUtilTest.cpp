// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <limits>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

TEST(StringUtil, TryParse)
{
	// Signed numbers
	s32 signed_result = 0;

	EXPECT_TRUE(TryParse("-4028", &signed_result));
	EXPECT_EQ(signed_result, -4028);

	signed_result = 0;
	EXPECT_TRUE(TryParse("4028", &signed_result));
	EXPECT_EQ(signed_result, 4028);

	signed_result = 0;
	EXPECT_FALSE(TryParse("1000000000000", &signed_result));
	EXPECT_EQ(signed_result, 0);

	// Unsigned numbers
	u32 unsigned_result = 0;

	EXPECT_TRUE(TryParse("10", &unsigned_result));
	EXPECT_EQ(unsigned_result, 10U);

	unsigned_result = 0;
	EXPECT_FALSE(TryParse("10000000000000", &unsigned_result));
	EXPECT_EQ(unsigned_result, 0U);

	unsigned_result = 0;
	EXPECT_TRUE(TryParse("-1", &unsigned_result));
	EXPECT_TRUE(std::numeric_limits<u32>::max() == unsigned_result);

	// Floating point numbers
	float float_result = 0.0f;
	double double_result = 0.0;

	EXPECT_TRUE(TryParse("0.01", &float_result));
	EXPECT_TRUE(TryParse("0.0123456789", &double_result));

	EXPECT_EQ(0.01f, float_result);
	EXPECT_EQ(0.0123456789, double_result);

	// Boolean specialization of TryParse
	bool bool_result = false;

	EXPECT_TRUE(TryParse("1", &bool_result));
	EXPECT_TRUE(bool_result);

	EXPECT_TRUE(TryParse("0", &bool_result));
	EXPECT_FALSE(bool_result);

	EXPECT_TRUE(TryParse("true", &bool_result));
	EXPECT_TRUE(bool_result);

	EXPECT_TRUE(TryParse("false", &bool_result));
	EXPECT_FALSE(bool_result);

	EXPECT_TRUE(TryParse("True", &bool_result));
	EXPECT_TRUE(bool_result);

	EXPECT_TRUE(TryParse("False", &bool_result));
	EXPECT_FALSE(bool_result);

	EXPECT_FALSE(TryParse("Nope", &bool_result));
	EXPECT_FALSE(bool_result);
}
