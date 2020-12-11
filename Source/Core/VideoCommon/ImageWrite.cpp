// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <list>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Image.h"

bool SaveData(const std::string& filename, const std::string& data)
{
  std::ofstream f;
  File::OpenFStream(f, filename, std::ios::binary);
  f << data;

  return true;
}

/*
TextureToPng

Inputs:
data      : This is an array of RGBA with 8 bits per channel. 4 bytes for each pixel.
row_stride: Determines the amount of bytes per row of pixels.
*/
bool TextureToPng(const u8* data, int row_stride, const std::string& filename, int width,
                  int height, bool save_alpha)
{
  if (!data)
    return false;

  if (save_alpha)
  {
    return Common::SavePNG(filename, data, Common::ImageByteFormat::RGBA, width, height,
                           row_stride);
  }

  std::vector<u8> buffer;
  buffer.reserve(width * height * 3);

  for (int y = 0; y < height; ++y)
  {
    const u8* pos = data + y * row_stride;
    for (int x = 0; x < width; ++x)
    {
      buffer.push_back(pos[x * 4]);
      buffer.push_back(pos[x * 4 + 1]);
      buffer.push_back(pos[x * 4 + 2]);
    }
  }

  return Common::SavePNG(filename, buffer.data(), Common::ImageByteFormat::RGB, width, height);
}
