// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace OGL
{
class RasterFont
{
public:
  RasterFont();
  ~RasterFont();
  static int debug;

  void printMultilineText(const std::string& text, float x, float y, int bbWidth,
                          int bbHeight, u32 color);

private:
  u32 VBO;
  u32 VAO;
  u32 texture;
  u32 uniform_color_id;
  u32 uniform_offset_id;
};
}
