// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Image.h"

namespace
{
struct PNGChunk
{
  std::string type;
  std::string data;
};

u32 ReadBigEndian32(std::string_view data)
{
  return static_cast<u32>(static_cast<u8>(data[0])) << 24 |
         static_cast<u32>(static_cast<u8>(data[1])) << 16 |
         static_cast<u32>(static_cast<u8>(data[2])) << 8 |
         static_cast<u32>(static_cast<u8>(data[3]));
}

// Splits a PNG datastream into its chunks, or returns nothing if it's malformed.
std::vector<PNGChunk> ReadPNGChunks(std::string_view png)
{
  constexpr std::string_view SIGNATURE{"\x89PNG\r\n\x1a\n", 8};
  if (!png.starts_with(SIGNATURE))
    return {};

  std::vector<PNGChunk> chunks;
  png.remove_prefix(SIGNATURE.size());
  while (png.size() >= 12)
  {
    const u32 length = ReadBigEndian32(png);
    if (png.size() < 12 + static_cast<size_t>(length))
      return {};
    chunks.push_back({std::string(png.substr(4, 4)), std::string(png.substr(8, length))});
    png.remove_prefix(12 + length);
  }
  if (!png.empty() || chunks.empty() || chunks.back().type != "IEND")
    return {};
  return chunks;
}

// Returns the position of the first chunk of the given type, or chunks.size() if there is none.
size_t FindChunk(const std::vector<PNGChunk>& chunks, std::string_view type)
{
  return std::ranges::find(chunks, type, &PNGChunk::type) - chunks.begin();
}

constexpr u32 PIXEL_SIZE = 4;

// Fills an RGBA image holding two views next to each other with pixels that identify their view
// and position. The alpha channel holds garbage, which must not end up in the PNG.
std::vector<u8> MakeSideBySideImage(u32 view_width, u32 height, u32 stride)
{
  std::vector<u8> image(stride * height, 0xCD);
  for (u32 y = 0; y < height; ++y)
  {
    for (u32 x = 0; x < view_width * 2; ++x)
    {
      const bool is_right_view = x >= view_width;
      u8* const pixel = &image[y * stride + x * PIXEL_SIZE];
      pixel[0] = static_cast<u8>((is_right_view ? 0x80 : 0x00) + x % view_width);
      pixel[1] = static_cast<u8>(y);
      pixel[2] = is_right_view ? 0xAA : 0x55;
      pixel[3] = static_cast<u8>(x + y);
    }
  }
  return image;
}

class ImageTest : public testing::Test
{
protected:
  ImageTest() : m_directory(File::CreateTempDir()), m_path(m_directory + "/image.png") {}

  ~ImageTest() override
  {
    if (!m_directory.empty())
      File::DeleteDirRecursively(m_directory);
  }

  void SetUp() override { ASSERT_FALSE(m_directory.empty()); }

  std::vector<PNGChunk> ReadChunks() const
  {
    std::string png;
    if (!File::ReadFileToString(m_path, png))
      return {};
    return ReadPNGChunks(png);
  }

  // Checks that the image saved by ConvertRGBAToRGBAndSaveStereoPNG holds both views at the right
  // place and that it's marked as a stereo pair in the diverging-fuse layout.
  void CheckStereoPNG(u32 view_width, u32 height, u32 stride, u32 expected_padding)
  {
    const std::vector<u8> input = MakeSideBySideImage(view_width, height, stride);
    ASSERT_TRUE(Common::ConvertRGBAToRGBAndSaveStereoPNG(m_path, input.data(), view_width, height,
                                                         stride, 6));

    const u32 expected_width = view_width * 2 + expected_padding;
    // The left edge of the right view must be on a column that is a multiple of 8.
    ASSERT_EQ((view_width + expected_padding) % 8, 0u);

    const std::vector<PNGChunk> chunks = ReadChunks();
    ASSERT_FALSE(chunks.empty());
    ASSERT_EQ(chunks[0].type, "IHDR");
    EXPECT_EQ(ReadBigEndian32(chunks[0].data), expected_width);
    EXPECT_EQ(ReadBigEndian32(std::string_view(chunks[0].data).substr(4)), height);

    const size_t ster = FindChunk(chunks, "sTER");
    ASSERT_LT(ster, chunks.size());
    EXPECT_EQ(std::ranges::count(chunks, std::string("sTER"), &PNGChunk::type), 1);
    EXPECT_LT(ster, FindChunk(chunks, "IDAT"));
    ASSERT_EQ(chunks[ster].data.size(), 1u);
    EXPECT_EQ(chunks[ster].data[0], 1);

    std::string png;
    ASSERT_TRUE(File::ReadFileToString(m_path, png));
    Common::UniqueBuffer<u8> decoded;
    u32 decoded_width = 0;
    u32 decoded_height = 0;
    ASSERT_TRUE(Common::LoadPNG(std::span(reinterpret_cast<const u8*>(png.data()), png.size()),
                                &decoded, &decoded_width, &decoded_height));
    ASSERT_EQ(decoded_width, expected_width);
    ASSERT_EQ(decoded_height, height);

    for (u32 y = 0; y < height; ++y)
    {
      for (u32 x = 0; x < expected_width; ++x)
      {
        const u8* const pixel = &decoded[(y * expected_width + x) * PIXEL_SIZE];
        u32 expected_rgb;
        if (x < view_width)
          expected_rgb = x << 16 | y << 8 | 0x55;
        else if (x < view_width + expected_padding)
          expected_rgb = 0;
        else
          expected_rgb = (0x80 + x - view_width - expected_padding) << 16 | y << 8 | 0xAA;

        SCOPED_TRACE(testing::Message() << "x = " << x << ", y = " << y);
        EXPECT_EQ(static_cast<u32>(pixel[0] << 16 | pixel[1] << 8 | pixel[2]), expected_rgb);
        EXPECT_EQ(pixel[3], 0xFF);
      }
    }
  }

  std::string m_directory;
  std::string m_path;
};
}  // namespace

TEST_F(ImageTest, StereoPNGPadsRightView)
{
  // 5 pixel wide views need 3 padding columns, with extra bytes at the end of each input row.
  CheckStereoPNG(5, 3, 2 * 5 * PIXEL_SIZE + 12, 3);
}

TEST_F(ImageTest, StereoPNGWithAlignedViews)
{
  CheckStereoPNG(16, 2, 2 * 16 * PIXEL_SIZE, 0);
}

TEST_F(ImageTest, StereoPNGWithOddViewWidth)
{
  CheckStereoPNG(9, 1, 2 * 9 * PIXEL_SIZE, 7);
}

TEST_F(ImageTest, MonoPNGIsNotMarkedAsStereo)
{
  const std::vector<u8> input = MakeSideBySideImage(4, 2, 2 * 4 * PIXEL_SIZE);
  ASSERT_TRUE(Common::ConvertRGBAToRGBAndSavePNG(m_path, input.data(), 8, 2, 8 * PIXEL_SIZE, 6));

  const std::vector<PNGChunk> chunks = ReadChunks();
  ASSERT_FALSE(chunks.empty());
  EXPECT_EQ(ReadBigEndian32(chunks[0].data), 8u);
  EXPECT_EQ(FindChunk(chunks, "sTER"), chunks.size());
}
