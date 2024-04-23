// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/SettingsHandler.h"

namespace
{
// The encrypted bytes corresponding to the following settings, in order:
//   "key" = "val"
Common::SettingsHandler::Buffer BUFFER_A{0x91, 0x91, 0x90, 0xEE, 0xD1, 0x2F, 0xF0, 0x34, 0x79};

// The encrypted bytes corresponding to the following settings, in order:
//   "key1" = "val1"
//   "key2" = "val2"
//   "foo" = "bar"
Common::SettingsHandler::Buffer BUFFER_B{
    0x91, 0x91, 0x90, 0xE2, 0x9A, 0x38, 0xFD, 0x55, 0x42, 0xEA, 0xC4, 0xF6, 0x5E, 0xF,  0xDF, 0xE7,
    0xC3, 0x0A, 0xBB, 0x9C, 0x50, 0xB1, 0x10, 0x82, 0xB4, 0x8A, 0x0D, 0xBE, 0xCD, 0x72, 0xF4};

// The encrypted bytes corresponding to the following setting:
//   "\xFA" = "a"
//
// This setting triggers the edge case fixed in PR #8704: the key's first and only byte matches the
// first byte of the initial encryption key, resulting in a null encoded byte on the first attempt
// to encode the line.
Common::SettingsHandler::Buffer BUFFER_C{0xF0, 0x0E, 0xD4, 0xB2, 0xAA, 0x44};

// The encrypted bytes corresponding to the following setting:
//   "\xFA\xE9" = "a"
//
// This setting triggers the edge case fixed in PR #8704 twice:
//
// 1. The key's first byte matches the first byte of the initial encryption key, resulting in a null
// encoded byte on the first attempt to encode the line.
//
// 2. The key's second byte matches the first byte of the encryption key after two
// rotations, resulting in a null encoded byte on the second attempt to encode the line (with a
// single LF inserted before the line).
Common::SettingsHandler::Buffer BUFFER_D{0xF0, 0xFE, 0x13, 0x3A, 0x9A, 0x2F, 0x91, 0x33};
}  // namespace

TEST(SettingsHandlerTest, EncryptSingleSetting)
{
  Common::SettingsHandler handler;
  handler.AddSetting("key", "val");
  Common::SettingsHandler::Buffer buffer = handler.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_A.begin(), BUFFER_A.end()));
}

TEST(SettingsHandlerTest, DecryptSingleSetting)
{
  Common::SettingsHandler handler(BUFFER_A);
  EXPECT_EQ(handler.GetValue("key"), "val");
}

TEST(SettingsHandlerTest, EncryptMultipleSettings)
{
  Common::SettingsHandler handler;
  handler.AddSetting("key1", "val1");
  handler.AddSetting("key2", "val2");
  handler.AddSetting("foo", "bar");
  Common::SettingsHandler::Buffer buffer = handler.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_B.begin(), BUFFER_B.end()));
}

TEST(SettingsHandlerTest, DecryptMultipleSettings)
{
  Common::SettingsHandler handler(BUFFER_B);
  EXPECT_EQ(handler.GetValue("key1"), "val1");
  EXPECT_EQ(handler.GetValue("key2"), "val2");
  EXPECT_EQ(handler.GetValue("foo"), "bar");
}

TEST(SettingsHandlerTest, GetValueOnSameInstance)
{
  Common::SettingsHandler handler;
  handler.AddSetting("key", "val");
  EXPECT_EQ(handler.GetValue("key"), "");
}

TEST(SettingsHandlerTest, EncryptAddsLFOnNullChar)
{
  Common::SettingsHandler handler;
  handler.AddSetting("\xFA", "a");
  Common::SettingsHandler::Buffer buffer = handler.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_C.begin(), BUFFER_C.end()));
}

TEST(SettingsHandlerTest, EncryptAddsLFOnNullCharTwice)
{
  Common::SettingsHandler handler;
  handler.AddSetting("\xFA\xE9", "a");
  Common::SettingsHandler::Buffer buffer = handler.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_D.begin(), BUFFER_D.end()));
}

TEST(SettingsHandlerTest, DecryptSingleAddedLF)
{
  Common::SettingsHandler handler(BUFFER_C);
  EXPECT_EQ(handler.GetValue("\xFA"), "a");
}

TEST(SettingsHandlerTest, DecryptTwoAddedLFs)
{
  Common::SettingsHandler handler(BUFFER_D);
  EXPECT_EQ(handler.GetValue("\xFA\xE9"), "a");
}
