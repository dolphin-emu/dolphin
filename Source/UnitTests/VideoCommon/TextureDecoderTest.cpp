// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureDecoder.h"

namespace
{
// The generic decoder, with each operation rounded on its own, as on hosts without fused
// multiply-add, whatever the compiler does with this file.
u8 Clamp(float value)
{
  return static_cast<u8>(std::clamp(static_cast<int>(value), 0, 255));
}

void ReferenceDecodeXFB(u8* dst, const u8* src, u32 width, u32 height, u32 stride)
{
  for (u32 y = 0; y < height; y++)
  {
    const u8* row = src + y * stride;
    for (u32 x = 0; x < width; x += 2)
    {
      const int y1 = row[0] - 16;
      const int u = row[1] - 128;
      const int y2 = row[2] - 16;
      const int v = row[3] - 128;
      row += 4;
      for (const int luma : {y1, y2})
      {
        volatile float yy = 1.164f * luma;
        volatile float rv = 1.596f * v;
        volatile float gu = 0.392f * u;
        volatile float gv = 0.813f * v;
        volatile float bu = 2.017f * u;
        volatile float g = yy - gu;
        *dst++ = Clamp(yy + rv);
        *dst++ = Clamp(g - gv);
        *dst++ = Clamp(yy + bu);
        *dst++ = 0xff;
      }
    }
  }
}
}  // namespace

TEST(TextureDecoder, DecodeXFBMatchesForAllColors)
{
  // Every Y with every U and V, in rows of 256 color samples, one row for each U.
  constexpr u32 width = 2 * 256 * 256;
  std::vector<u8> src(2 * width);
  for (u32 u = 0; u < 256; u++)
  {
    for (u32 i = 0; i < width / 2; i++)
    {
      src[4 * i + 0] = static_cast<u8>(i);
      src[4 * i + 1] = static_cast<u8>(u);
      src[4 * i + 2] = static_cast<u8>(i * 7 + 3);
      src[4 * i + 3] = static_cast<u8>(i >> 8);
    }
    std::vector<u8> expected(4 * width), actual(4 * width);
    ReferenceDecodeXFB(expected.data(), src.data(), width, 1, 2 * width);
    TexDecoder_DecodeXFB(actual.data(), src.data(), width, 1, 2 * width);
    ASSERT_EQ(expected, actual) << "U " << u;
  }
}

TEST(TextureDecoder, DecodeXFBSizesAndStrides)
{
  std::mt19937 rng(1);
  for (u32 width = 1; width <= 40; width++)
  {
    const u32 height = 3;
    const u32 stride = 2 * width + 2 + 4 * (width % 3);
    std::vector<u8> src(stride * height + 4);
    for (u8& byte : src)
      byte = static_cast<u8>(rng());
    // The decoders write two pixels for each color sample.
    const u32 pixels = (width + 1) / 2 * 2 * height;
    std::vector<u8> expected(4 * pixels + 64, 0x55), actual(4 * pixels + 64, 0x55);
    ReferenceDecodeXFB(expected.data(), src.data(), width, height, stride);
    TexDecoder_DecodeXFB(actual.data(), src.data(), width, height, stride);
    EXPECT_EQ(expected, actual) << "width " << width;
  }
}
