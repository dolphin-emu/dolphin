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
