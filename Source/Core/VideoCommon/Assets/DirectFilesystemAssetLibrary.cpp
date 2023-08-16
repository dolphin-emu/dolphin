// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"

#include <algorithm>
#include <fmt/os.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"

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
  std::lock_guard lk(m_lock);
  if (auto iter = m_assetid_to_asset_map_path.find(asset_id);
      iter != m_assetid_to_asset_map_path.end())
  {
    const auto& asset_map_path = iter->second;
    CustomAssetLibrary::TimeType max_entry;
    for (const auto& [key, value] : asset_map_path)
    {
      std::error_code ec;
      const auto tp = std::filesystem::last_write_time(value, ec);
      if (ec)
        continue;
      if (tp > max_entry)
        max_entry = tp;
    }
    return max_entry;
  }

  return {};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadPixelShader(const AssetID& asset_id,
                                                                           PixelShaderData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Asset map for a pixel shader is the shader and some metadata
  if (asset_map.size() != 2)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have two files mapped!", asset_id);
    return {};
  }

  const auto metadata = asset_map.find("metadata");
  const auto shader = asset_map.find("shader");
  if (metadata == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a metadata entry mapped!", asset_id);
    return {};
  }

  if (shader == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a shader entry mapped!", asset_id);
    return {};
  }

  std::size_t metadata_size;
  {
    std::error_code ec;
    metadata_size = std::filesystem::file_size(metadata->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' error - failed to get shader metadata file size with error '{}'!",
                    asset_id, ec);
      return {};
    }
  }
  std::size_t shader_size;
  {
    std::error_code ec;
    shader_size = std::filesystem::file_size(shader->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' error - failed to get shader source file size with error '{}'!",
                    asset_id, ec);
      return {};
    }
  }
  const auto approx_mem_size = metadata_size + shader_size;

  if (!File::ReadFileToString(PathToString(shader->second), data->m_shader_source))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the shader file '{}',", asset_id,
                  PathToString(shader->second));
    return {};
  }

  std::string json_data;
  if (!File::ReadFileToString(PathToString(metadata->second), json_data))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the json file '{}',", asset_id,
                  PathToString(metadata->second));
    return {};
  }

  picojson::value root;
  const auto error = picojson::parse(root, json_data);

  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' error -  failed to load the json file '{}', due to parse error: {}",
                  asset_id, PathToString(metadata->second), error);
    return {};
  }
  if (!root.is<picojson::object>())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Asset '{}' error -  failed to load the json file '{}', due to root not being an object!",
        asset_id, PathToString(metadata->second));
    return {};
  }

  const auto& root_obj = root.get<picojson::object>();

  if (!PixelShaderData::FromJson(asset_id, root_obj, data))
    return {};

  return LoadInfo{approx_mem_size, GetLastAssetWriteTime(asset_id)};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadMaterial(const AssetID& asset_id,
                                                                        MaterialData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Material is expected to have one asset mapped
  if (asset_map.empty() || asset_map.size() > 1)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - material expected to have one file mapped!", asset_id);
    return {};
  }
  const auto& asset_path = asset_map.begin()->second;

  std::string json_data;
  if (!File::ReadFileToString(PathToString(asset_path), json_data))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  material failed to load the json file '{}',",
                  asset_id, PathToString(asset_path));
    return {};
  }

  picojson::value root;
  const auto error = picojson::parse(root, json_data);

  if (!error.empty())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Asset '{}' error -  material failed to load the json file '{}', due to parse error: {}",
        asset_id, PathToString(asset_path), error);
    return {};
  }
  if (!root.is<picojson::object>())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' error - material failed to load the json file '{}', due to root not "
                  "being an object!",
                  asset_id, PathToString(asset_path));
    return {};
  }

  const auto& root_obj = root.get<picojson::object>();

  if (!MaterialData::FromJson(asset_id, root_obj, data))
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' error -  material failed to load the json file '{}', as material "
                  "json could not be parsed!",
                  asset_id, PathToString(asset_path));
    return {};
  }

  return LoadInfo{json_data.size(), GetLastAssetWriteTime(asset_id)};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadTexture(const AssetID& asset_id,
                                                                       CustomTextureData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Raw texture is expected to have one asset mapped
  if (asset_map.empty() || asset_map.size() > 1)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - raw texture expected to have one file mapped!",
                  asset_id);
    return {};
  }
  const auto& asset_path = asset_map.begin()->second;

  std::error_code ec;
  const auto last_loaded_time = std::filesystem::last_write_time(asset_path, ec);
  if (ec)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - failed to get last write time with error '{}'!",
                  asset_id, ec);
    return {};
  }
  auto ext = PathToString(asset_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds")
  {
    if (!LoadDDSTexture(data, PathToString(asset_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load dds texture!", asset_id);
      return {};
    }

    if (!LoadMips(asset_path, data))
      return {};

    return LoadInfo{GetAssetSize(*data), last_loaded_time};
  }
  else if (ext == ".png")
  {
    // If we have no levels, create one to pass into LoadPNGTexture
    if (data->m_levels.empty())
      data->m_levels.push_back({});

    if (!LoadPNGTexture(&data->m_levels[0], PathToString(asset_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load png texture!", asset_id);
      return {};
    }

    if (!LoadMips(asset_path, data))
      return {};

    return LoadInfo{GetAssetSize(*data), last_loaded_time};
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' error - extension '{}' unknown!", asset_id, ext);
  return {};
}

void DirectFilesystemAssetLibrary::SetAssetIDMapData(const AssetID& asset_id,
                                                     AssetMap asset_path_map)
{
  std::lock_guard lk(m_lock);
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
  SplitPath(PathToString(asset_path), &path, &filename, &extension);

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

DirectFilesystemAssetLibrary::AssetMap
DirectFilesystemAssetLibrary::GetAssetMapForID(const AssetID& asset_id) const
{
  std::lock_guard lk(m_lock);
  if (auto iter = m_assetid_to_asset_map_path.find(asset_id);
      iter != m_assetid_to_asset_map_path.end())
  {
    return iter->second;
  }
  return {};
}
}  // namespace VideoCommon
