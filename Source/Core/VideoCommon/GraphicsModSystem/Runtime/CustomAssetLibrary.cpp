// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomAssetLibrary.h"

#include <algorithm>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"

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
                    level.width, level.height, PathToString(asset_id), mip_level, current_mip_width,
                    current_mip_height);
    }
    else
    {
      // It is invalid to have more than a single 1x1 mipmap.
      ERROR_LOG_FMT(VIDEO,
                    "Custom game texture {} has too many 1x1 mipmaps. Skipping extra levels.",
                    PathToString(asset_id));
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
                  PathToString(asset_id));

    return {};
  }

  return load_info;
}

CustomAssetLibrary::TimeType
DirectFilesystemLibrary::GetLastAssetWriteTime(const AssetID& asset_id) const
{
  if (auto iter = m_assetid_to_asset_map_path.find(asset_id);
      iter != m_assetid_to_asset_map_path.end())
  {
    const auto& asset_map_path = iter->second;
    CustomAssetLibrary::TimeType max_entry;
    for (const auto& [key, value] : asset_map_path)
    {
      const auto tp = std::filesystem::last_write_time(value);
      if (tp > max_entry)
        max_entry = tp;
    }
    return max_entry;
  }

  return {};
}

CustomAssetLibrary::LoadInfo DirectFilesystemLibrary::LoadTexture(const AssetID& asset_id,
                                                                  CustomTextureData* data)
{
  if (auto iter = m_assetid_to_asset_map_path.find(asset_id);
      iter != m_assetid_to_asset_map_path.end())
  {
    const auto& asset_map_path = iter->second;

    // Raw texture is expected to have one asset mapped
    if (asset_map_path.empty() || asset_map_path.size() > 1)
      return {};
    const auto& asset_path = asset_map_path.begin()->second;

    const auto last_loaded_time = std::filesystem::last_write_time(asset_path);
    auto ext = asset_path.extension().string();
    Common::ToLower(&ext);
    if (ext == ".dds")
    {
      LoadDDSTexture(data, asset_path.string());
      if (data->m_levels.empty()) [[unlikely]]
        return {};
      if (!LoadMips(asset_path, data))
        return {};

      return LoadInfo{GetAssetSize(*data), last_loaded_time};
    }
    else if (ext == ".png")
    {
      LoadPNGTexture(data, asset_path.string());
      if (data->m_levels.empty()) [[unlikely]]
        return {};
      if (!LoadMips(asset_path, data))
        return {};

      return LoadInfo{GetAssetSize(*data), last_loaded_time};
    }
  }

  return {};
}

void DirectFilesystemLibrary::SetAssetIDMapData(
    const AssetID& asset_id, std::map<std::string, std::filesystem::path> asset_path_map)
{
  m_assetid_to_asset_map_path[asset_id] = std::move(asset_path_map);
}

bool DirectFilesystemLibrary::LoadMips(const std::filesystem::path& asset_path,
                                       CustomTextureData* data)
{
  if (!data) [[unlikely]]
    return false;

  std::string path;
  std::string filename;
  std::string extension;
  SplitPath(asset_path.string(), &path, &filename, &extension);

  std::string extension_lower = extension;
  Common::ToLower(&extension_lower);

  // Load additional mip levels
  for (u32 mip_level = static_cast<u32>(data->m_levels.size());; mip_level++)
  {
    const auto mip_level_filename = filename + fmt::format("_mip{}", mip_level);

    const auto full_path = path + mip_level_filename + extension;
    if (!File::Exists(full_path))
      return true;

    VideoCommon::CustomTextureData::Level level;
    if (extension_lower == ".dds")
    {
      if (!LoadDDSTexture(&level, full_path, mip_level))
      {
        ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' failed to load", mip_level_filename);
        return false;
      }
    }
    else if (extension_lower == ".png")
    {
      if (!LoadPNGTexture(&level, full_path))
      {
        ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' failed to load", mip_level_filename);
        return false;
      }
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' has unsupported extension", mip_level_filename);
      return false;
    }

    data->m_levels.push_back(std::move(level));
  }

  return true;
}
}  // namespace VideoCommon
