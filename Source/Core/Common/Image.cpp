// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Image.h"

#include <string>
#include <vector>

#include <png.h>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/ImageC.h"
#include "Common/Logging/Log.h"
#include "Common/Timer.h"

namespace Common
{
bool LoadPNG(const std::vector<u8>& input, std::vector<u8>* data_out, u32* width_out,
             u32* height_out)
{
  // Using the 'Simplified API' of libpng; see section V in the libpng manual.

  // Read header
  png_image png = {};
  png.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&png, input.data(), input.size()))
    return false;

  // Prepare output vector
  png.format = PNG_FORMAT_RGBA;
  size_t png_size = PNG_IMAGE_SIZE(png);
  data_out->resize(png_size);

  // Convert to RGBA and write into output vector
  if (!png_image_finish_read(&png, nullptr, data_out->data(), 0, nullptr))
    return false;

  *width_out = png.width;
  *height_out = png.height;

  return true;
}

static void WriteCallback(png_structp png_ptr, png_bytep data, size_t length)
{
  std::vector<u8>* buffer = static_cast<std::vector<u8>*>(png_get_io_ptr(png_ptr));
  buffer->insert(buffer->end(), data, data + length);
}

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
             u32 height, int stride, int level)
{
  Common::Timer timer;
  timer.Start();

  size_t byte_per_pixel;
  int png_format;
  switch (format)
  {
  case ImageByteFormat::RGB:
    png_format = PNG_FORMAT_RGB;
    byte_per_pixel = 3;
    break;
  case ImageByteFormat::RGBA:
    png_format = PNG_FORMAT_RGBA;
    byte_per_pixel = 4;
    break;
  default:
    return false;
  }

  // libpng doesn't handle non-ASCII characters in path, so write in two steps:
  // first to memory, then to file
  std::vector<u8> buffer;
  buffer.reserve(byte_per_pixel * width * height);

  std::vector<const u8*> rows;
  rows.reserve(height);
  for (u32 row = 0; row < height; row++)
  {
    rows.push_back(&input[row * stride]);
  }

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info_ptr = png_create_info_struct(png_ptr);

  bool success = false;
  if (png_ptr != nullptr && info_ptr != nullptr)
  {
    success = SavePNG0(png_ptr, info_ptr, png_format, width, height, level, &buffer, WriteCallback,
                       const_cast<u8**>(rows.data()));
  }
  png_destroy_write_struct(&png_ptr, &info_ptr);

  if (success)
  {
    File::IOFile outfile(path, "wb");
    if (!outfile)
      return false;
    success = outfile.WriteBytes(buffer.data(), buffer.size());

    timer.Stop();
    INFO_LOG_FMT(FRAMEDUMP, "{} byte {} by {} image saved to {} at level {} in {}", buffer.size(),
                 width, height, path, level, timer.GetTimeElapsedFormatted());
  }

  return success;
}

bool ConvertRGBAToRGBAndSavePNG(const std::string& path, const u8* input, u32 width, u32 height,
                                int stride, int level)
{
  const std::vector<u8> data = RGBAToRGB(input, width, height, stride);
  return SavePNG(path, data.data(), ImageByteFormat::RGB, width, height, width * 3, level);
}

std::vector<u8> RGBAToRGB(const u8* input, u32 width, u32 height, int row_stride)
{
  std::vector<u8> buffer;
  buffer.reserve(width * height * 3);

  for (u32 y = 0; y < height; ++y)
  {
    const u8* pos = input + y * row_stride;
    for (u32 x = 0; x < width; ++x)
    {
      buffer.push_back(pos[x * 4]);
      buffer.push_back(pos[x * 4 + 1]);
      buffer.push_back(pos[x * 4 + 2]);
    }
  }
  return buffer;
}
}  // namespace Common
