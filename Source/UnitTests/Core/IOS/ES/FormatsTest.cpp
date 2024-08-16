// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <vector>

#include "Core/CommonTitles.h"
#include "Core/IOS/ES/Formats.h"
#include "TestBinaryData.h"

TEST(ESFormats, TitleType)
{
  EXPECT_TRUE(IOS::ES::IsTitleType(Titles::SYSTEM_MENU, IOS::ES::TitleType::System));
  EXPECT_FALSE(IOS::ES::IsDiscTitle(Titles::SYSTEM_MENU));

  constexpr u64 ios59_title_id = 0x000000010000003b;
  EXPECT_TRUE(IOS::ES::IsTitleType(ios59_title_id, IOS::ES::TitleType::System));
  EXPECT_FALSE(IOS::ES::IsDiscTitle(ios59_title_id));

  constexpr u64 ztp_title_id = 0x00010000525a4445;
  EXPECT_TRUE(IOS::ES::IsTitleType(ztp_title_id, IOS::ES::TitleType::Game));
  EXPECT_TRUE(IOS::ES::IsDiscTitle(ztp_title_id));
}

TEST(TMDReader, Validity)
{
  IOS::ES::TMDReader tmd;
  EXPECT_FALSE(tmd.IsValid()) << "An empty TMDReader should be invalid";

  tmd = IOS::ES::TMDReader{{1, 2, 3}};
  EXPECT_FALSE(tmd.IsValid()) << "IsValid should be false when reading an invalid TMD";

  tmd = IOS::ES::TMDReader{std::vector<u8>(soup01_tmd.cbegin(), soup01_tmd.cend() - 1)};
  EXPECT_FALSE(tmd.IsValid()) << "IsValid should be false when reading an invalid TMD";

  tmd = IOS::ES::TMDReader{std::vector<u8>(soup01_tmd.cbegin(), soup01_tmd.cend())};
  EXPECT_TRUE(tmd.IsValid()) << "IsValid should be true when reading a valid TMD";

  tmd = IOS::ES::TMDReader{std::vector<u8>(ios59_tmd.cbegin(), ios59_tmd.cend())};
  EXPECT_TRUE(tmd.IsValid()) << "IsValid should be true when reading a valid TMD";
}

class TMDReaderTest : public ::testing::Test
{
protected:
  virtual void SetUp() = 0;

  // Common tests.
  void TestGeneralInfo();
  void TestRawTMDAndView();

  // Expected data.
  virtual u64 GetTitleId() const = 0;
  virtual u64 GetIOSId() const = 0;
  virtual std::string GetGameID() const = 0;
  virtual std::vector<u8> GetRawView() const = 0;

  std::vector<u8> m_raw_tmd;
  IOS::ES::TMDReader m_tmd;
};

void TMDReaderTest::SetUp()
{
  m_tmd.SetBytes(m_raw_tmd);
  ASSERT_TRUE(m_tmd.IsValid()) << "BUG: The test TMD should be valid.";
}

void TMDReaderTest::TestGeneralInfo()
{
  EXPECT_EQ(m_tmd.GetTitleId(), GetTitleId());
  EXPECT_EQ(m_tmd.GetIOSId(), GetIOSId());
  EXPECT_EQ(m_tmd.GetGameID(), GetGameID());
}

void TMDReaderTest::TestRawTMDAndView()
{
  const std::vector<u8>& dolphin_tmd_bytes = m_tmd.GetBytes();
  // Separate check because gtest prints neither the size nor the full buffer.
  EXPECT_EQ(m_raw_tmd.size(), dolphin_tmd_bytes.size());
  EXPECT_EQ(m_raw_tmd, dolphin_tmd_bytes);

  const std::vector<u8> tmd_view = GetRawView();
  const std::vector<u8> dolphin_tmd_view = m_tmd.GetRawView();
  EXPECT_EQ(dolphin_tmd_view.size(), tmd_view.size());
  EXPECT_EQ(dolphin_tmd_view, tmd_view);
}

class GameTMDReaderTest : public TMDReaderTest
{
protected:
  void SetUp() override
  {
    m_raw_tmd = std::vector<u8>(soup01_tmd.cbegin(), soup01_tmd.cend());
    TMDReaderTest::SetUp();
  }
  u64 GetTitleId() const override { return 0x00010000534f5550; }
  u64 GetIOSId() const override { return 0x0000000100000038; }
  std::string GetGameID() const override { return "SOUP01"; }
  std::vector<u8> GetRawView() const override
  {
    return std::vector<u8>(soup01_tmd_view.cbegin(), soup01_tmd_view.cend());
  }
};

TEST_F(GameTMDReaderTest, GeneralInfo)
{
  TestGeneralInfo();
}

TEST_F(GameTMDReaderTest, RawTMDAndView)
{
  TestRawTMDAndView();
}

TEST_F(GameTMDReaderTest, ContentInfo)
{
  EXPECT_EQ(m_tmd.GetNumContents(), 1U);

  const auto check_is_expected_content = [](const IOS::ES::Content& content) {
    EXPECT_EQ(content.id, 0U);
    EXPECT_EQ(content.index, 0U);
    EXPECT_EQ(content.type, 3U);
    EXPECT_EQ(content.size, 0xff7c0000ULL);
    constexpr std::array<u8, 20> expected_hash = {{
        0x77, 0x13, 0xe0, 0xac, 0xef, 0xc1, 0x01, 0x51, 0xe7, 0x8b,
        0x0b, 0x01, 0xa2, 0xfb, 0x03, 0xdb, 0x45, 0x8a, 0x0e, 0x18,
    }};
    EXPECT_EQ(content.sha1, expected_hash);
  };

  IOS::ES::Content content;

  EXPECT_TRUE(m_tmd.FindContentById(0, &content)) << "Content 0 should exist";
  check_is_expected_content(content);

  EXPECT_TRUE(m_tmd.GetContent(0, &content)) << "Content with index 0 should exist";
  check_is_expected_content(content);

  EXPECT_FALSE(m_tmd.FindContentById(1, &content)) << "Content 1 should not exist";
  EXPECT_FALSE(m_tmd.GetContent(1, &content)) << "Content with index 1 should not exist";

  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  ASSERT_EQ(contents.size(), size_t(1));
  check_is_expected_content(contents.at(0));
}

class IOSTMDReaderTest : public TMDReaderTest
{
protected:
  void SetUp() override
  {
    m_raw_tmd = std::vector<u8>(ios59_tmd.cbegin(), ios59_tmd.cend());
    TMDReaderTest::SetUp();
  }
  u64 GetTitleId() const override { return 0x000000010000003b; }
  u64 GetIOSId() const override { return 0; }
  std::string GetGameID() const override { return "000000010000003b"; }
  std::vector<u8> GetRawView() const override
  {
    return std::vector<u8>(ios59_tmd_view.cbegin(), ios59_tmd_view.cend());
  }
};

TEST_F(IOSTMDReaderTest, GeneralInfo)
{
  TestGeneralInfo();
  EXPECT_EQ(m_tmd.GetBootIndex(), 22U);
  EXPECT_EQ(m_tmd.GetTitleVersion(), 0x2421U);
}

TEST_F(IOSTMDReaderTest, RawTMDAndView)
{
  TestRawTMDAndView();
}

TEST_F(IOSTMDReaderTest, ContentInfo)
{
  EXPECT_EQ(m_tmd.GetNumContents(), 23U);

  const auto check_is_first_content = [](const IOS::ES::Content& content) {
    EXPECT_EQ(content.id, 0x00000020U);
    EXPECT_EQ(content.index, 0U);
    EXPECT_EQ(content.type, 1U);
    EXPECT_FALSE(content.IsShared());
    EXPECT_EQ(content.size, 0x40ULL);
    constexpr std::array<u8, 20> expected_hash = {{
        0x2c, 0x96, 0x97, 0x6d, 0x25, 0x2b, 0x2e, 0xa0, 0xcd, 0xc1,
        0xea, 0x16, 0x57, 0x7f, 0x3d, 0x90, 0x82, 0x59, 0xf1, 0x53,
    }};
    EXPECT_EQ(content.sha1, expected_hash);
  };

  const auto check_is_kernel = [](const IOS::ES::Content& content) {
    EXPECT_EQ(content.id, 0x00000018U);
    EXPECT_EQ(content.index, 22U);
    EXPECT_EQ(content.type, 0x8001U);
    EXPECT_TRUE(content.IsShared());
    EXPECT_EQ(content.size, 0x293d8ULL);
    constexpr std::array<u8, 20> expected_hash = {{
        0xfd, 0x77, 0x46, 0xac, 0x47, 0x00, 0xd5, 0x86, 0xa9, 0x32,
        0x84, 0xfb, 0xe3, 0x12, 0xc5, 0x2b, 0x2a, 0xc6, 0x33, 0xd3,
    }};
    EXPECT_EQ(content.sha1, expected_hash);
  };

  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  ASSERT_EQ(contents.size(), size_t(23));

  IOS::ES::Content content;

  EXPECT_FALSE(m_tmd.FindContentById(0x23, &content)) << "Content 00000023 should not exist";
  EXPECT_FALSE(m_tmd.GetContent(23, &content)) << "Content with index 23 should not exist";

  EXPECT_TRUE(m_tmd.FindContentById(0x20, &content)) << "Content 00000020 should exist";
  check_is_first_content(content);
  EXPECT_TRUE(m_tmd.GetContent(0, &content)) << "Content with index 0 should exist";
  check_is_first_content(content);
  check_is_first_content(contents.at(0));

  EXPECT_TRUE(m_tmd.FindContentById(0x18, &content)) << "Content 00000018 should exist";
  check_is_kernel(content);
  EXPECT_TRUE(m_tmd.GetContent(22, &content)) << "Content with index 22 should exist";
  check_is_kernel(content);
  check_is_kernel(contents.at(22));
}
