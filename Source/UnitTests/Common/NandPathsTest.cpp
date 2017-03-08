// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <string>

#include "Common/NandPaths.h"

static void TestEscapeAndUnescape(const std::string& unescaped, const std::string& escaped)
{
  EXPECT_EQ(escaped, Common::EscapeFileName(unescaped));
  EXPECT_EQ(unescaped, Common::UnescapeFileName(escaped));
}

TEST(NandPaths, EscapeUnescape)
{
  EXPECT_EQ("/a/__2e__/b/__3e__", Common::EscapePath("/a/./b/>"));
  EXPECT_EQ("/a/__2e____2e__/b/__3e__", Common::EscapePath("/a/../b/>"));
  EXPECT_EQ("/a/__2e____2e____2e__/b/__3e__", Common::EscapePath("/a/.../b/>"));
  EXPECT_EQ("/a/__2e____2e____2e____2e__/b/__3e__", Common::EscapePath("/a/..../b/>"));

  EXPECT_EQ("", Common::EscapePath(""));
  TestEscapeAndUnescape("", "");

  TestEscapeAndUnescape("regular string", "regular string");
  TestEscapeAndUnescape("a/../1b|", "a__2f__..__2f__1b__7c__");
  TestEscapeAndUnescape("__22__", "__5f____5f__22__5f____5f__");

  // The \0 can't be written in a regular string literal, otherwise it'll be treated as the end
  TestEscapeAndUnescape(std::string{'\0'} +
                            "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
                            "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
                            "\"*/:<>?\\_|\x7f",
                        "__00____01____02____03____04____05____06____07__"
                        "__08____09____0a____0b____0c____0d____0e____0f__"
                        "__10____11____12____13____14____15____16____17__"
                        "__18____19____1a____1b____1c____1d____1e____1f__"
                        "__22____2a____2f____3a____3c____3e____3f____5c_____7c____7f__");
  //                                                                     ^
  // There is a normal underscore here (not part of an escape sequence): |
}
