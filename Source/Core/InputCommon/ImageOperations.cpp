// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ImageOperations.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stack>
#include <string>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Image.h"

namespace InputCommon
{
namespace
{
Pixel SampleNearest(const ImagePixelData& src, double u, double v)
{
  const u32 x = std::clamp(static_cast<u32>(u * src.width), 0u, src.width - 1);
  const u32 y = std::clamp(static_cast<u32>(v * src.height), 0u, src.height - 1);
  return src.pixels[x + y * src.width];
}
}  // namespace

void CopyImageRegion(const ImagePixelData& src, ImagePixelData& dst, const Rect& src_region,
                     const Rect& dst_region)
{
  if (src_region.GetWidth() != dst_region.GetWidth() ||
      src_region.GetHeight() != dst_region.GetHeight())
  {
    return;
  }

  for (u32 x = 0; x < dst_region.GetWidth(); x++)
  {
    for (u32 y = 0; y < dst_region.GetHeight(); y++)
    {
      dst.pixels[(y + dst_region.top) * dst.width + x + dst_region.left] =
          src.pixels[(y + src_region.top) * src.width + x + src_region.left];
    }
  }
}

std::optional<ImagePixelData> LoadImage(const std::string& path)
{
  File::IOFile file;
  file.Open(path, "rb");
  std::vector<u8> buffer(file.GetSize());
  file.ReadBytes(buffer.data(), file.GetSize());

  ImagePixelData image;
  std::vector<u8> data;
  if (!Common::LoadPNG(buffer, &data, &image.width, &image.height))
    return std::nullopt;

  image.pixels.resize(image.width * image.height);
  for (u32 x = 0; x < image.width; x++)
  {
    for (u32 y = 0; y < image.height; y++)
    {
      const u32 index = y * image.width + x;
      const auto pixel =
          Pixel{data[index * 4], data[index * 4 + 1], data[index * 4 + 2], data[index * 4 + 3]};
      image.pixels[index] = pixel;
    }
  }

  return image;
}

bool WriteImage(const std::string& path, const ImagePixelData& image)
{
  std::vector<u8> buffer;
  buffer.reserve(image.width * image.height * 4);

  for (u32 y = 0; y < image.height; ++y)
  {
    for (u32 x = 0; x < image.width; ++x)
    {
      const auto index = x + y * image.width;
      const auto pixel = image.pixels[index];
      buffer.push_back(pixel.r);
      buffer.push_back(pixel.g);
      buffer.push_back(pixel.b);
      buffer.push_back(pixel.a);
    }
  }

  return Common::SavePNG(path, buffer.data(), Common::ImageByteFormat::RGBA, image.width,
                         image.height, image.width * 4);
}

ImagePixelData Resize(ResizeMode mode, const ImagePixelData& src, u32 new_width, u32 new_height)
{
  ImagePixelData result(new_width, new_height);

  for (u32 x = 0; x < new_width; x++)
  {
    const double u = x / static_cast<double>(new_width - 1);
    for (u32 y = 0; y < new_height; y++)
    {
      const double v = y / static_cast<double>(new_height - 1);

      switch (mode)
      {
      case ResizeMode::Nearest:
        result.pixels[y * new_width + x] = SampleNearest(src, u, v);
        break;
      }
    }
  }

  return result;
}

ImagePixelData ResizeKeepAspectRatio(ResizeMode mode, const ImagePixelData& src, u32 new_width,
                                     u32 new_height, const Pixel& background_color)
{
  ImagePixelData result(new_width, new_height, background_color);

  const double corrected_height = new_width * (src.height / static_cast<double>(src.width));
  const double corrected_width = new_height * (src.width / static_cast<double>(src.height));
  // initially no borders
  u32 top = 0;
  u32 left = 0;

  ImagePixelData resized;
  if (corrected_height <= new_height)
  {
    // Handle vertical padding

    const int diff = new_height - std::trunc(corrected_height);
    top = diff / 2;
    if (diff % 2 != 0)
    {
      // If the difference is odd, we need to have one side be slightly larger
      top += 1;
    }
    resized = Resize(mode, src, new_width, corrected_height);
  }
  else
  {
    // Handle horizontal padding

    const int diff = new_width - std::trunc(corrected_width);
    left = diff / 2;
    if (diff % 2 != 0)
    {
      // If the difference is odd, we need to have one side be slightly larger
      left += 1;
    }
    resized = Resize(mode, src, corrected_width, new_height);
  }
  CopyImageRegion(resized, result, Rect{0, 0, resized.width, resized.height},
                  Rect{left, top, left + resized.width, top + resized.height});

  return result;
}

}  // namespace InputCommon
