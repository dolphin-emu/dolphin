// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "VideoCommon/Assets/TextureAsset.h"

namespace VideoCommon
{
CustomAssetLibrary::LoadInfo CustomAssetLibrary::LoadGameTexture(const AssetID& asset_id,
                                                                 TextureData* data)
{
  const auto load_info = LoadTexture(asset_id, data);
  if (load_info.m_bytes_loaded == 0)
    return {};

  if (data->m_type != TextureData::Type::Type_Texture2D)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Custom asset '{}' is not a valid game texture, it is expected to be a 2d texture "
        "but was a '{}'.",
        asset_id, data->m_type);
    return {};
  }

  // Note: 'LoadTexture()' ensures we have a level loaded
  for (std::size_t slice_index = 0; slice_index < data->m_texture.m_slices.size(); slice_index++)
  {
    auto& slice = data->m_texture.m_slices[slice_index];
    const auto& first_mip = slice.m_levels[0];

    // Verify that each mip level is the correct size (divide by 2 each time).
    u32 current_mip_width = first_mip.width;
    u32 current_mip_height = first_mip.height;
    for (u32 mip_level = 1; mip_level < static_cast<u32>(slice.m_levels.size()); mip_level++)
    {
      if (current_mip_width != 1 || current_mip_height != 1)
      {
        current_mip_width = std::max(current_mip_width / 2, 1u);
        current_mip_height = std::max(current_mip_height / 2, 1u);

        const VideoCommon::CustomTextureData::ArraySlice::Level& level = slice.m_levels[mip_level];
        if (current_mip_width == level.width && current_mip_height == level.height)
          continue;

        ERROR_LOG_FMT(VIDEO,
                      "Invalid custom game texture size {}x{} for texture asset {}. Slice {} with "
                      "mipmap level {} "
                      "must be {}x{}.",
                      level.width, level.height, asset_id, slice_index, mip_level,
                      current_mip_width, current_mip_height);
      }
      else
      {
        // It is invalid to have more than a single 1x1 mipmap.
        ERROR_LOG_FMT(
            VIDEO,
            "Custom game texture {} has too many 1x1 mipmaps for slice {}. Skipping extra levels.",
            asset_id, slice_index);
      }

      // Drop this mip level and any others after it.
      while (slice.m_levels.size() > mip_level)
        slice.m_levels.pop_back();
    }

    // All levels have to have the same format.
    if (std::ranges::any_of(
            slice.m_levels,
            [&first_mip](const VideoCommon::CustomTextureData::ArraySlice::Level& l) {
              return l.format != first_mip.format;
            }))
    {
      ERROR_LOG_FMT(
          VIDEO, "Custom game texture {} has inconsistent formats across mip levels for slice {}.",
          asset_id, slice_index);

      return {};
    }
  }

  return load_info;
}
}  // namespace VideoCommon
