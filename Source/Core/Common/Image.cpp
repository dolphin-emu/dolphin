// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Image.h"

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
}  // namespace Common
