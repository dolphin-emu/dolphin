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
  EXPECT_TRUE(StringBeginsWith("abc", "a"));
  EXPECT_FALSE(StringBeginsWith("abc", "b"));
  EXPECT_TRUE(StringBeginsWith("abc", "ab"));
  EXPECT_FALSE(StringBeginsWith("a", "ab"));
  EXPECT_FALSE(StringBeginsWith("", "a"));
  EXPECT_FALSE(StringBeginsWith("", "ab"));
  EXPECT_TRUE(StringBeginsWith("abc", ""));
  EXPECT_TRUE(StringBeginsWith("", ""));
}

TEST(StringUtil, StringEndsWith)
{
  EXPECT_TRUE(StringEndsWith("abc", "c"));
  EXPECT_FALSE(StringEndsWith("abc", "b"));
  EXPECT_TRUE(StringEndsWith("abc", "bc"));
  EXPECT_FALSE(StringEndsWith("a", "ab"));
  EXPECT_FALSE(StringEndsWith("", "a"));
  EXPECT_FALSE(StringEndsWith("", "ab"));
  EXPECT_TRUE(StringEndsWith("abc", ""));
  EXPECT_TRUE(StringEndsWith("", ""));
}

TEST(StringUtil, UTF8ToSHIFTJIS)
{
  const std::string kirby_unicode = "\xe6\x98\x9f\xe3\x81\xae\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x93\xe3\x82\xa3";
  const std::string kirby_sjis = "\x90\xaf\x82\xcc\x83\x4a\x81\x5b\x83\x72\x83\x42";
  
  EXPECT_STREQ(SHIFTJISToUTF8(UTF8ToSHIFTJIS(kirby_unicode)).c_str(), kirby_unicode.c_str());
  EXPECT_STREQ(UTF8ToSHIFTJIS(kirby_unicode).c_str(), kirby_sjis.c_str());
}
