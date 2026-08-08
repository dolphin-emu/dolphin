// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StbImage.h"

using namespace Common;

TEST(StbImage, LoadFromFile)
{
  std::string cur_directory = File::GetExeDirectory()
#ifdef __APPLE__
                              + DIR_SEP "Tests"  // FIXME: Ugly hack.
#endif
      ;
  std::string resource_directory = cur_directory + DIR_SEP "Sys" DIR_SEP "Resources";
  auto directory = File::ScanDirectoryTree(fmt::format("{}", resource_directory), false);

  std::optional<File::IOFile> png_file;
  for (const auto& child : directory.children)
  {
    if (!child.isDirectory && child.physicalName.find(".png") != std::string::npos)
    {
      png_file = File::IOFile(child.physicalName, "rb");
      return;
    }
  }

  ASSERT_TRUE(png_file.has_value());
  auto result_success = StbImage::LoadFromFile(std::move(*png_file), 3);
  EXPECT_TRUE(result_success.has_value());

  File::IOFile non_file("_non_existent_file_.png", "rb");
  auto result_fail = StbImage::LoadFromFile(std::move(non_file), 3);
  EXPECT_FALSE(result_fail.has_value());
}

TEST(StbImage, Resize)
{
  std::vector<u8> pixels = {255, 0, 0, 255};
  StbImage image(pixels.data(), 2, 2, 1);

  image.Resize(4, 4);

  EXPECT_EQ(image.GetWidth(), 4);
  EXPECT_EQ(image.GetHeight(), 4);
  EXPECT_EQ(image.GetChannels(), 1);
  EXPECT_EQ(image.GetData().size(), 4 * 4 * 1);

  image.Resize(1, 1);

  EXPECT_EQ(image.GetWidth(), 1);
  EXPECT_EQ(image.GetHeight(), 1);
  EXPECT_EQ(image.GetChannels(), 1);
  EXPECT_EQ(image.GetData().size(), 1 * 1 * 1);
}

TEST(StbImage, EncodeToBaselineJPEG)
{
  std::vector<u8> pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  StbImage image(pixels.data(), 2, 2, 4);

  auto jpeg_data = image.EncodeToBaselineJPEG(75);
  ASSERT_TRUE(jpeg_data.has_value());

  EXPECT_GE(jpeg_data->size(), 2);
  EXPECT_EQ((*jpeg_data)[0], 0xFF);
  EXPECT_EQ((*jpeg_data)[1], 0xD8);
}
