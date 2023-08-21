// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Matrix.h"

namespace InputCommon
{
struct Pixel
{
  u8 r = 0;
  u8 g = 0;
  u8 b = 0;
  u8 a = 0;

  bool operator==(const Pixel& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
  bool operator!=(const Pixel& o) const { return !(o == *this); }
};

using Point = Common::TVec2<u32>;
using Rect = MathUtil::Rectangle<u32>;

struct ImagePixelData
{
  ImagePixelData() = default;

  explicit ImagePixelData(std::vector<Pixel> image_pixels, u32 width_, u32 height_)
      : pixels(std::move(image_pixels)), width(width_), height(height_)
  {
  }

  explicit ImagePixelData(u32 width_, u32 height_, const Pixel& default_color = Pixel{0, 0, 0, 0})
      : pixels(width_ * height_, default_color), width(width_), height(height_)
  {
  }
  std::vector<Pixel> pixels;
  u32 width = 0;
  u32 height = 0;
};

void CopyImageRegion(const ImagePixelData& src, ImagePixelData& dst, const Rect& src_region,
                     const Rect& dst_region);

std::optional<ImagePixelData> LoadImage(const std::string& path);

bool WriteImage(const std::string& path, const ImagePixelData& image);

enum class ResizeMode
{
  Nearest,
};

ImagePixelData Resize(ResizeMode mode, const ImagePixelData& src, u32 new_width, u32 new_height);

ImagePixelData ResizeKeepAspectRatio(ResizeMode mode, const ImagePixelData& src, u32 new_width,
                                     u32 new_height, const Pixel& background_color);
}  // namespace InputCommon
