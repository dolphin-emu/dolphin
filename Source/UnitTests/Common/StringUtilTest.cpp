// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Common/StringUtil.h"

TEST(StringUtil, JoinStrings)
{
  EXPECT_EQ("", JoinStrings({}, ", "));
  EXPECT_EQ("a", JoinStrings({"a"}, ","));
  EXPECT_EQ("ab", JoinStrings({"a", "b"}, ""));
  EXPECT_EQ("a, bb, c", JoinStrings({"a", "bb", "c"}, ", "));
  EXPECT_EQ("???", JoinStrings({"?", "?"}, "?"));
}

TEST(StringUtil, StringBeginsWith)
{
  EXPECT_EQ(true, StringBeginsWith("abc", "a"));
  EXPECT_EQ(false, StringBeginsWith("abc", "b"));
  EXPECT_EQ(true, StringBeginsWith("abc", "ab"));
  EXPECT_EQ(false, StringBeginsWith("a", "ab"));
  EXPECT_EQ(false, StringBeginsWith("", "a"));
  EXPECT_EQ(false, StringBeginsWith("", "ab"));
  EXPECT_EQ(true, StringBeginsWith("abc", ""));
  EXPECT_EQ(true, StringBeginsWith("", ""));
}

TEST(StringUtil, StringEndsWith)
{
  EXPECT_EQ(true, StringEndsWith("abc", "c"));
  EXPECT_EQ(false, StringEndsWith("abc", "b"));
  EXPECT_EQ(true, StringEndsWith("abc", "bc"));
  EXPECT_EQ(false, StringEndsWith("a", "ab"));
  EXPECT_EQ(false, StringEndsWith("", "a"));
  EXPECT_EQ(false, StringEndsWith("", "ab"));
  EXPECT_EQ(true, StringEndsWith("abc", ""));
  EXPECT_EQ(true, StringEndsWith("", ""));
}
