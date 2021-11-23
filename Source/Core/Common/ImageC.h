// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <png.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
#endif
    bool
    SavePNG0(png_structp png_ptr, png_infop info_ptr, int png_format, png_uint_32 width,
             png_uint_32 height, int level, png_voidp io_ptr, png_rw_ptr write_fn,
             png_bytepp row_pointers);
