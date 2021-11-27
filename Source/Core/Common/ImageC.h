// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <png.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
struct ErrorHandler
{
  void* error_list;    // std::vector<std::string>*
  void* warning_list;  // std::vector<std::string>*
  void (*StoreError)(struct ErrorHandler* self, const char* msg);
  void (*StoreWarning)(struct ErrorHandler* self, const char* msg);
};

bool SavePNG0(png_structp png_ptr, png_infop info_ptr, int color_type, png_uint_32 width,
              png_uint_32 height, int level, png_voidp io_ptr, png_rw_ptr write_fn,
              png_bytepp row_pointers);

void PngError(png_structp png_ptr, png_const_charp msg);
void PngWarning(png_structp png_ptr, png_const_charp msg);

#ifdef __cplusplus
}
#endif
