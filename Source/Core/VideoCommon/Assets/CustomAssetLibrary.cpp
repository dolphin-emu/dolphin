// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "VideoCommon/Assets/CustomTextureData.h"

namespace VideoCommon
{
namespace
{
std::size_t GetAssetSize(const CustomTextureData& data)
{
  std::size_t total = 0;
  for (const auto& level : data.m_levels)
  {
    total += level.data.size();
  }
  return total;
}
}  // namespace
CustomAssetLibrary::LoadInfo CustomAssetLibrary::LoadGameTexture(const AssetID& asset_id,
                                                                 CustomTextureData* data)
{
  const auto load_info = LoadTexture(asset_id, data);
  if (load_info.m_bytes_loaded == 0)
    return {};

  // Note: 'LoadTexture()' ensures we have a level loaded
  const auto& first_mip = data->m_levels[0];

  // Verify that each mip level is the correct size (divide by 2 each time).
  u32 current_mip_width = first_mip.width;
  u32 current_mip_height = first_mip.height;
  for (u32 mip_level = 1; mip_level < static_cast<u32>(data->m_levels.size()); mip_level++)
  {
    if (current_mip_width != 1 || current_mip_height != 1)
    {
      current_mip_width = std::max(current_mip_width / 2, 1u);
      current_mip_height = std::max(current_mip_height / 2, 1u);

      const VideoCommon::CustomTextureData::Level& level = data->m_levels[mip_level];
      if (current_mip_width == level.width && current_mip_height == level.height)
        continue;

      ERROR_LOG_FMT(VIDEO,
                    "Invalid custom game texture size {}x{} for texture asset {}. Mipmap level {} "
                    "must be {}x{}.",
                    level.width, level.height, asset_id, mip_level, current_mip_width,
                    current_mip_height);
    }
    else
    {
      // It is invalid to have more than a single 1x1 mipmap.
      ERROR_LOG_FMT(VIDEO,
                    "Custom game texture {} has too many 1x1 mipmaps. Skipping extra levels.",
                    asset_id);
    }

    // Drop this mip level and any others after it.
    while (data->m_levels.size() > mip_level)
      data->m_levels.pop_back();
  }

  // All levels have to have the same format.
  if (std::any_of(data->m_levels.begin(), data->m_levels.end(),
                  [&first_mip](const VideoCommon::CustomTextureData::Level& l) {
                    return l.format != first_mip.format;
                  }))
  {
    ERROR_LOG_FMT(VIDEO, "Custom game texture {} has inconsistent formats across mip levels.",
                  asset_id);

    return {};
  }

  return load_info;
}
}  // namespace VideoCommon
