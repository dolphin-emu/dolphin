// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/ImageC.h"

// Since libpng requires use of setjmp, and setjmp interacts poorly with destructors and other C++
// features, this is in a separate C file.

// The main purpose of this function is to allow specifying the compression level, which
// png_image_write_to_memory does not allow.  row_pointers is not modified by libpng, but also isn't
// const for some reason.
bool SavePNG0(png_structp png_ptr, png_infop info_ptr, int png_format, png_uint_32 width,
              png_uint_32 height, int level, png_voidp io_ptr, png_rw_ptr write_fn,
              png_bytepp row_pointers)
{
  if (setjmp(png_jmpbuf(png_ptr)) != 0)
    return false;

  png_set_compression_level(png_ptr, level);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, png_format, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_rows(png_ptr, info_ptr, row_pointers);
  png_set_write_fn(png_ptr, io_ptr, write_fn, NULL);
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  return true;
}
