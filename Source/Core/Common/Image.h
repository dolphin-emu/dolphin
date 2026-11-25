// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"

namespace Common
{
bool LoadPNG(std::span<const u8> input, Common::UniqueBuffer<u8>* data_out, u32* width_out,
             u32* height_out);

enum class ImageByteFormat
{
  RGB,
  RGBA,
};

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
             u32 height, u32 stride, int level = 6);
bool ConvertRGBAToRGBAndSavePNG(const std::string& path, const u8* input, u32 width, u32 height,
                                u32 stride, int level);

// Saves a stereoscopic pair as a PNG image with an sTER chunk, the PNG extension that marks an
// image as a stereo pair so that 3D viewers can recognize it.
// The input holds the left eye view followed by the right eye view on each row, each of them
// view_width pixels wide. The views are written in the same order (the "diverging-fuse" layout),
// with black padding columns in between when needed to start the right eye view on a column that
// is a multiple of 8, as required by the sTER specification.
bool ConvertRGBAToRGBAndSaveStereoPNG(const std::string& path, const u8* input, u32 view_width,
                                      u32 height, u32 stride, int level);
}  // namespace Common
