// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include <Common/CommonTypes.h>
#include <Common/FileUtil.h>

struct Image
{
  std::vector<u8> data;
  u32 width = 0;
  u32 height = 0;
};

bool load_png(std::vector<u8> input, Image& image);
