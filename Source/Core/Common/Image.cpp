// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <png.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include <Common/Image.h>

static inline int multiply_alpha(int alpha, int color)
{
  int temp = (alpha * color) + 0x80;

  return ((temp + (temp >> 8)) >> 8);
}

static void premultiply_data(png_structp png, png_row_infop row_info, png_bytep data)
{
  unsigned int i;
  png_bytep p;

  for (i = 0, p = data; i < row_info->rowbytes; i += 4, p += 4)
  {
    png_byte alpha = p[3];
    uint32_t w;

    if (alpha == 0)
    {
      w = 0;
    }
    else
    {
      png_byte red = p[0];
      png_byte green = p[1];
      png_byte blue = p[2];

      if (alpha != 0xff)
      {
        red = multiply_alpha(alpha, red);
        green = multiply_alpha(alpha, green);
        blue = multiply_alpha(alpha, blue);
      }
      w = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
    }

    *(uint32_t*)p = w;
  }
}

static void read_func(png_structp png, png_bytep data, png_size_t size)
{
  std::vector<u8>::iterator* it = static_cast<std::vector<u8>::iterator*>(png_get_io_ptr(png));

  for (size_t i = 0; i < size; ++i)
  {
    data[i] = **it;
    (*it)++;
  }
}

static void png_error_callback(png_structp png, png_const_charp error_msg)
{
  longjmp(png_jmpbuf(png), 1);
}

bool load_png(std::vector<u8> input, Image& image)
{
  png_struct* png;
  png_info* info;
  png_byte** row_pointers = NULL;
  png_uint_32 width, height;
  int depth, color_type, interlace, stride;
  unsigned int i;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_callback, NULL);
  if (!png)
    return false;

  info = png_create_info_struct(png);
  if (!info)
  {
    png_destroy_read_struct(&png, &info, NULL);
    return false;
  }

  if (setjmp(png_jmpbuf(png)))
  {
    if (row_pointers)
      free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    return false;
  }

  auto it = input.begin();
  png_set_read_fn(png, &it, read_func);
  png_read_info(png, info);
  png_get_IHDR(png, info, &width, &height, &depth, &color_type, &interlace, NULL, NULL);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  if (color_type == PNG_COLOR_TYPE_GRAY)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  if (depth == 16)
    png_set_strip_16(png);

  if (depth < 8)
    png_set_packing(png);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  if (interlace != PNG_INTERLACE_NONE)
    png_set_interlace_handling(png);

  png_set_bgr(png);
  png_set_filler(png, 0xff, PNG_FILLER_AFTER);
  png_set_read_user_transform_fn(png, premultiply_data);
  png_read_update_info(png, info);
  png_get_IHDR(png, info, &width, &height, &depth, &color_type, &interlace, NULL, NULL);

  stride = width * 4;
  image.data.reserve(stride * height);
  image.data.resize(stride * height);

  row_pointers = reinterpret_cast<u8**>(malloc(height * sizeof row_pointers[0]));
  if (row_pointers == NULL)
  {
    png_destroy_read_struct(&png, &info, NULL);
    return false;
  }

  for (i = 0; i < height; i++)
    row_pointers[i] = &image.data[i * stride];

  png_read_image(png, row_pointers);
  png_read_end(png, info);

  free(row_pointers);
  png_destroy_read_struct(&png, &info, NULL);

  image.width = width;
  image.height = height;
  return true;
}
