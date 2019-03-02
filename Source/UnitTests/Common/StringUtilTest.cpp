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

TEST(StringUtil, StringPopBackIf)
{
  std::string abc = "abc";
  std::string empty;

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STREQ("abc", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'c');
  StringPopBackIf(&empty, 'c');
  EXPECT_STRNE("abc", abc.c_str());
  EXPECT_STREQ("ab", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'b');
  StringPopBackIf(&empty, 'b');
  EXPECT_STRNE("ab", abc.c_str());
  EXPECT_STREQ("a", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STRNE("a", abc.c_str());
  EXPECT_STREQ("", abc.c_str());
  EXPECT_STREQ(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STREQ("", abc.c_str());
  EXPECT_STREQ(empty.c_str(), abc.c_str());
}

TEST(StringUtil, UTF8ToSHIFTJIS)
{
  const std::string kirby_unicode =
      "\xe6\x98\x9f\xe3\x81\xae\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x93\xe3\x82\xa3";
  const std::string kirby_sjis = "\x90\xaf\x82\xcc\x83\x4a\x81\x5b\x83\x72\x83\x42";

  EXPECT_STREQ(SHIFTJISToUTF8(UTF8ToSHIFTJIS(kirby_unicode)).c_str(), kirby_unicode.c_str());
  EXPECT_STREQ(UTF8ToSHIFTJIS(kirby_unicode).c_str(), kirby_sjis.c_str());
}

template <typename T>
static void DoRoundTripTest(const std::vector<T>& data)
{
  for (const T& e : data)
  {
    const std::string s = ValueToString(e);
    T out;
    EXPECT_TRUE(TryParse(s, &out));
    EXPECT_EQ(e, out);
  }
}

TEST(StringUtil, ToString_TryParse_Roundtrip)
{
  DoRoundTripTest<bool>({true, false});
  DoRoundTripTest<int>({0, -1, 1, 123, -123, 123456789, -123456789});
  DoRoundTripTest<unsigned int>({0u, 1u, 123u, 123456789u, 4023456789u});
  DoRoundTripTest<float>({0.0f, 1.0f, -1.0f, -0.5f, 0.5f, -1e-3f, 1e-3f, 1e3f, -1e3f});
  DoRoundTripTest<double>({0.0, 1.0, -1.0, -0.5, 0.5, -1e-3, 1e-3, 1e3, -1e3});
}
