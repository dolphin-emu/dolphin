// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"

#include <algorithm>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
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
CustomAssetLibrary::TimeType
DirectFilesystemAssetLibrary::GetLastAssetWriteTime(const AssetID& asset_id) const
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

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadTexture(const AssetID& asset_id,
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

void DirectFilesystemAssetLibrary::SetAssetIDMapData(
    const AssetID& asset_id, std::map<std::string, std::filesystem::path> asset_path_map)
{
  m_assetid_to_asset_map_path[asset_id] = std::move(asset_path_map);
}

bool DirectFilesystemAssetLibrary::LoadMips(const std::filesystem::path& asset_path,
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
