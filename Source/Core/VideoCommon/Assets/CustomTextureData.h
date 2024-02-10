// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
class CustomTextureData
{
public:
  struct ArraySlice
  {
    struct Level
    {
      std::vector<u8> data;
      AbstractTextureFormat format = AbstractTextureFormat::RGBA8;
      u32 width = 0;
      u32 height = 0;
      u32 row_length = 0;
    };
    std::vector<Level> m_levels;
  };
  std::vector<ArraySlice> m_slices;
};

bool LoadDDSTexture(CustomTextureData* texture, const std::string& filename);
bool LoadDDSTexture(CustomTextureData::ArraySlice::Level* level, const std::string& filename,
                    u32 mip_level);
bool LoadPNGTexture(CustomTextureData::ArraySlice::Level* level, const std::string& filename);
}  // namespace VideoCommon
