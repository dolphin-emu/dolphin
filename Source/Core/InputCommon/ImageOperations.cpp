// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ImageOperations.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stack>

#include <png.h>

#include "Common/File.h"
#include "Common/FileUtil.h"
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

// For Visual Studio, ignore the error caused by the 'setjmp' call
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4611)
#endif

bool WriteImage(const std::string& path, const ImagePixelData& image)
{
  bool success = false;
  char title[] = "Dynamic Input Texture";
  char title_key[] = "Title";
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  std::vector<u8> buffer;

  // Open file for writing (binary mode)
  File::IOFile fp(path, "wb");
  if (!fp.IsOpen())
  {
    goto finalise;
  }

  // Initialize write structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (png_ptr == nullptr)
  {
    goto finalise;
  }

  // Initialize info structure
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == nullptr)
  {
    goto finalise;
  }

  // Classical libpng error handling uses longjmp to do C-style unwind.
  // Modern libpng does support a user callback, but it's required to operate
  // in the same way (just gives a chance to do stuff before the longjmp).
  // Instead of futzing with it, we use gotos specifically so the compiler
  // will still generate proper destructor calls for us (hopefully).
  // We also do not use any local variables outside the region longjmp may
  // have been called from if they were modified inside that region (they
  // would need to be volatile).
  if (setjmp(png_jmpbuf(png_ptr)))
  {
    goto finalise;
  }

  // Begin region which may call longjmp

  png_init_io(png_ptr, fp.GetHandle());

  // Write header (8 bit color depth)
  png_set_IHDR(png_ptr, info_ptr, image.width, image.height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_text title_text;
  title_text.compression = PNG_TEXT_COMPRESSION_NONE;
  title_text.key = title_key;
  title_text.text = title;
  png_set_text(png_ptr, info_ptr, &title_text, 1);

  png_write_info(png_ptr, info_ptr);

  buffer.resize(image.width * 4);

  // Write image data
  for (u32 y = 0; y < image.height; ++y)
  {
    for (u32 x = 0; x < image.width; x++)
    {
      const auto index = x + y * image.width;
      const auto pixel = image.pixels[index];

      const auto buffer_index = 4 * x;
      buffer[buffer_index] = pixel.r;
      buffer[buffer_index + 1] = pixel.g;
      buffer[buffer_index + 2] = pixel.b;
      buffer[buffer_index + 3] = pixel.a;
    }

    // The old API uses u8* instead of const u8*. It doesn't write
    // to this pointer, but to fit the API, we have to drop the const qualifier.
    png_write_row(png_ptr, const_cast<u8*>(buffer.data()));
  }

  // End write
  png_write_end(png_ptr, nullptr);

  // End region which may call longjmp

  success = true;

finalise:
  if (info_ptr != nullptr)
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  if (png_ptr != nullptr)
    png_destroy_write_struct(&png_ptr, nullptr);

  return success;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
