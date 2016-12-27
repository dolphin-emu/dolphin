// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Image.h"

#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <png.h>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace Common
{
static int MultiplyAlpha(int alpha, int color)
{
  const int temp = (alpha * color) + 0x80;
  return ((temp + (temp >> 8)) >> 8);
}

static void PremultiplyData(png_structp png, png_row_infop row_info, png_bytep data)
{
  for (png_size_t i = 0; i < row_info->rowbytes; i += 4, data += 4)
  {
    const png_byte alpha = data[3];
    u32 w;

    if (alpha == 0)
    {
      w = 0;
    }
    else
    {
      png_byte red = data[0];
      png_byte green = data[1];
      png_byte blue = data[2];

      if (alpha != 0xff)
      {
        red = MultiplyAlpha(alpha, red);
        green = MultiplyAlpha(alpha, green);
        blue = MultiplyAlpha(alpha, blue);
      }
      w = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
    }

    std::memcpy(data, &w, sizeof(w));
  }
}

struct ReadProgress
{
  const u8* current;
  const u8* end;
};

static void ReadCallback(png_structp png, png_bytep data, png_size_t size)
{
  ReadProgress* progress = static_cast<ReadProgress*>(png_get_io_ptr(png));
  for (size_t i = 0; i < size; ++i)
  {
    if (progress->current >= progress->end)
      png_error(png, "Read beyond end of file");  // This makes us longjmp back to LoadPNG

    data[i] = *(progress->current);
    ++(progress->current);
  }
}

static void PNGErrorCallback(png_structp png, png_const_charp error_msg)
{
  ERROR_LOG(COMMON, "PNG loading error: %s", error_msg);
  longjmp(png_jmpbuf(png), 1);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4611)
// VS shows the following warning even though no C++ objects are destroyed in LoadPNG:
// "warning C4611: interaction between '_setjmp' and C++ object destruction is non-portable"
#endif

bool LoadPNG(const std::vector<u8>& input, std::vector<u8>* data_out, u32* width_out,
             u32* height_out)
{
  const bool is_png = !png_sig_cmp(input.data(), 0, input.size());
  if (!is_png)
    return false;

  png_struct* png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, PNGErrorCallback, nullptr);
  if (!png)
    return false;

  png_info* info = png_create_info_struct(png);
  if (!info)
  {
    png_destroy_read_struct(&png, &info, nullptr);
    return false;
  }

  // It would've been nice to use std::vector for this, but using setjmp safely
  // means that we have to use this manually managed volatile pointer garbage instead.
  png_byte** volatile row_pointers_volatile = nullptr;

  if (setjmp(png_jmpbuf(png)))
  {
    free(row_pointers_volatile);
    png_destroy_read_struct(&png, &info, nullptr);
    return false;
  }

  ReadProgress read_progress{input.data(), input.data() + input.size()};
  png_set_read_fn(png, &read_progress, ReadCallback);
  png_read_info(png, info);

  png_uint_32 width, height;
  int depth, color_type, interlace;
  png_get_IHDR(png, info, &width, &height, &depth, &color_type, &interlace, nullptr, nullptr);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);
  else if (color_type == PNG_COLOR_TYPE_GRAY)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  if (depth == 16)
    png_set_strip_16(png);
  else if (depth < 8)
    png_set_packing(png);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  if (interlace != PNG_INTERLACE_NONE)
    png_set_interlace_handling(png);

  png_set_bgr(png);
  png_set_filler(png, 0xff, PNG_FILLER_AFTER);
  png_set_read_user_transform_fn(png, PremultiplyData);
  png_read_update_info(png, info);
  png_get_IHDR(png, info, &width, &height, &depth, &color_type, &interlace, nullptr, nullptr);

  const size_t stride = width * 4;
  data_out->resize(stride * height);

  png_byte** row_pointers = static_cast<png_byte**>(malloc(height * sizeof(png_byte*)));
  if (!row_pointers)
  {
    png_destroy_read_struct(&png, &info, nullptr);
    return false;
  }

  for (png_uint_32 i = 0; i < height; i++)
    row_pointers[i] = &(*data_out)[i * stride];

  row_pointers_volatile = row_pointers;

  png_read_image(png, row_pointers);
  png_read_end(png, info);

  free(row_pointers);
  png_destroy_read_struct(&png, &info, nullptr);

  *width_out = width;
  *height_out = height;
  return true;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace Common
