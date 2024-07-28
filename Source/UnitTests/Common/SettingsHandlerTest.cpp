// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/SettingsHandler.h"

namespace
{
// The encrypted bytes corresponding to the following settings, in order:
//   "key" = "val"
Common::SettingsBuffer BUFFER_A{0x91, 0x91, 0x90, 0xEE, 0xD1, 0x2F, 0xF0, 0x34, 0x79};

// The encrypted bytes corresponding to the following settings, in order:
//   "key1" = "val1"
//   "key2" = "val2"
//   "foo" = "bar"
Common::SettingsBuffer BUFFER_B{0x91, 0x91, 0x90, 0xE2, 0x9A, 0x38, 0xFD, 0x55, 0x42, 0xEA, 0xC4,
                                0xF6, 0x5E, 0xF,  0xDF, 0xE7, 0xC3, 0x0A, 0xBB, 0x9C, 0x50, 0xB1,
                                0x10, 0x82, 0xB4, 0x8A, 0x0D, 0xBE, 0xCD, 0x72, 0xF4};

// The encrypted bytes corresponding to the following setting:
//   "\xFA" = "a"
//
// This setting triggers the edge case fixed in PR #8704: the key's first and only byte matches the
// first byte of the initial encryption key, resulting in a null encoded byte on the first attempt
// to encode the line.
Common::SettingsBuffer BUFFER_C{0xF0, 0x0E, 0xD4, 0xB2, 0xAA, 0x44};

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
Common::SettingsBuffer BUFFER_D{0xF0, 0xFE, 0x13, 0x3A, 0x9A, 0x2F, 0x91, 0x33};
}  // namespace

TEST(SettingsWriterTest, EncryptSingleSetting)
{
  Common::SettingsWriter writer;
  writer.AddSetting("key", "val");
  Common::SettingsBuffer buffer = writer.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_A.begin(), BUFFER_A.end()));
}

TEST(SettingsReaderTest, DecryptSingleSetting)
{
  Common::SettingsReader reader(BUFFER_A);
  EXPECT_EQ(reader.GetValue("key"), "val");
}

TEST(SettingsWriterTest, EncryptMultipleSettings)
{
  Common::SettingsWriter writer;
  writer.AddSetting("key1", "val1");
  writer.AddSetting("key2", "val2");
  writer.AddSetting("foo", "bar");
  Common::SettingsBuffer buffer = writer.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_B.begin(), BUFFER_B.end()));
}

TEST(SettingsReaderTest, DecryptMultipleSettings)
{
  Common::SettingsReader reader(BUFFER_B);
  EXPECT_EQ(reader.GetValue("key1"), "val1");
  EXPECT_EQ(reader.GetValue("key2"), "val2");
  EXPECT_EQ(reader.GetValue("foo"), "bar");
}

TEST(SettingsWriterTest, EncryptAddsLFOnNullChar)
{
  Common::SettingsWriter writer;
  writer.AddSetting("\xFA", "a");
  Common::SettingsBuffer buffer = writer.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_C.begin(), BUFFER_C.end()));
}

TEST(SettingsWriterTest, EncryptAddsLFOnNullCharTwice)
{
  Common::SettingsWriter writer;
  writer.AddSetting("\xFA\xE9", "a");
  Common::SettingsBuffer buffer = writer.GetBytes();

  EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(), BUFFER_D.begin(), BUFFER_D.end()));
}

TEST(SettingsReaderTest, DecryptSingleAddedLF)
{
  Common::SettingsReader reader(BUFFER_C);
  EXPECT_EQ(reader.GetValue("\xFA"), "a");
}

TEST(SettingsReaderTest, DecryptTwoAddedLFs)
{
  Common::SettingsReader reader(BUFFER_D);
  EXPECT_EQ(reader.GetValue("\xFA\xE9"), "a");
}
