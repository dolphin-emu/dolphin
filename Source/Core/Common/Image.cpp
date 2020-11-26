// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Image.h"

#include <string>
#include <vector>

#include <png.h>

#include "Common/CommonTypes.h"

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

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
             u32 height, int stride)
{
  png_image png = {};
  png.version = PNG_IMAGE_VERSION;
  png.width = width;
  png.height = height;

  switch (format)
  {
  case ImageByteFormat::RGB:
    png.format = PNG_FORMAT_RGB;
    break;
  case ImageByteFormat::RGBA:
    png.format = PNG_FORMAT_RGBA;
    break;
  default:
    return false;
  }

  png_image_write_to_file(&png, path.c_str(), 0, input, stride, nullptr);
  if (png.warning_or_error & PNG_IMAGE_ERROR)
    return false;

  return true;
}
}  // namespace Common
