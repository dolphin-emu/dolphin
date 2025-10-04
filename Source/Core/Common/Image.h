// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"

namespace Common
{
bool LoadPNG(
    std::span<const u8> input, Common::UniqueBuffer<u8>* data_out, u32* width_out, u32* height_out);

enum class ImageByteFormat
{
  RGB,
  RGBA,
};

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
    u32 height, u32 stride, int level = 6);
bool ConvertRGBAToRGBAndSavePNG(
    const std::string& path, const u8* input, u32 width, u32 height, u32 stride, int level);
}  // namespace Common
