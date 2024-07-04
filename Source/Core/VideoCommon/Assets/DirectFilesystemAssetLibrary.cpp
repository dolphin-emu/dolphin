// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"

#include <algorithm>
#include <vector>

#include <fmt/std.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/System.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
namespace
{
std::chrono::system_clock::time_point FileTimeToSysTime(std::filesystem::file_time_type file_time)
{
#ifdef _WIN32
  return std::chrono::clock_cast<std::chrono::system_clock>(file_time);
#else
  // Note: all compilers should switch to chrono::clock_cast
  // once it is available for use
  const auto system_time_now = std::chrono::system_clock::now();
  const auto file_time_now = decltype(file_time)::clock::now();
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      file_time - file_time_now + system_time_now);
#endif
}

std::size_t GetAssetSize(const CustomTextureData& data)
{
  std::size_t total = 0;
  for (const auto& slice : data.m_slices)
  {
    for (const auto& level : slice.m_levels)
    {
      total += level.data.size();
    }
  }
  return total;
}
}  // namespace
CustomAssetLibrary::TimeType
DirectFilesystemAssetLibrary::GetLastAssetWriteTime(const AssetID& asset_id) const
{
  std::lock_guard lk(m_asset_map_lock);
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
      auto tp_sys = FileTimeToSysTime(tp);
      if (tp_sys > max_entry)
        max_entry = tp_sys;
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

  picojson::value root;
  std::string error;
  if (!JsonFromFile(PathToString(metadata->second), &root, &error))
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

  std::size_t metadata_size;
  {
    std::error_code ec;
    metadata_size = std::filesystem::file_size(asset_path, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - failed to get material file size with error '{}'!",
                    asset_id, ec);
      return {};
    }
  }

  picojson::value root;
  std::string error;
  if (!JsonFromFile(PathToString(asset_path), &root, &error))
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

  return LoadInfo{metadata_size, GetLastAssetWriteTime(asset_id)};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadMesh(const AssetID& asset_id,
                                                                    MeshData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Asset map for a mesh is the mesh and some metadata
  if (asset_map.size() != 2)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have two files mapped!", asset_id);
    return {};
  }

  const auto metadata = asset_map.find("metadata");
  const auto mesh = asset_map.find("mesh");
  if (metadata == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a metadata entry mapped!", asset_id);
    return {};
  }

  if (mesh == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a mesh entry mapped!", asset_id);
    return {};
  }

  std::size_t metadata_size;
  {
    std::error_code ec;
    metadata_size = std::filesystem::file_size(metadata->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' error - failed to get mesh metadata file size with error '{}'!",
                    asset_id, ec);
      return {};
    }
  }
  std::size_t mesh_size;
  {
    std::error_code ec;
    mesh_size = std::filesystem::file_size(mesh->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - failed to get mesh file size with error '{}'!",
                    asset_id, ec);
      return {};
    }
  }
  const auto approx_mem_size = metadata_size + mesh_size;

  File::IOFile file(PathToString(mesh->second), "rb");
  if (!file.IsOpen())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - failed to open mesh file '{}'!", asset_id,
                  PathToString(mesh->second));
    return {};
  }

  std::vector<u8> bytes;
  bytes.reserve(file.GetSize());
  file.ReadBytes(bytes.data(), file.GetSize());
  if (!MeshData::FromDolphinMesh(bytes, data))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the mesh file '{}'!", asset_id,
                  PathToString(mesh->second));
    return {};
  }

  picojson::value root;
  std::string error;
  if (!JsonFromFile(PathToString(metadata->second), &root, &error))
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

  if (!MeshData::FromJson(asset_id, root_obj, data))
    return {};

  return LoadInfo{approx_mem_size, GetLastAssetWriteTime(asset_id)};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadTexture(const AssetID& asset_id,
                                                                       TextureData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Texture can optionally have a metadata file as well
  if (asset_map.empty() || asset_map.size() > 2)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - raw texture expected to have one or two files mapped!",
                  asset_id);
    return {};
  }

  const auto metadata = asset_map.find("metadata");
  const auto texture_path = asset_map.find("texture");

  if (texture_path == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a texture entry mapped!", asset_id);
    return {};
  }

  std::size_t metadata_size = 0;
  if (metadata != asset_map.end())
  {
    std::error_code ec;
    metadata_size = std::filesystem::file_size(metadata->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' error - failed to get texture metadata file size with error '{}'!",
                    asset_id, ec);
      return {};
    }

    picojson::value root;
    std::string error;
    if (!JsonFromFile(PathToString(metadata->second), &root, &error))
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
    if (!TextureData::FromJson(asset_id, root_obj, data))
    {
      return {};
    }
  }
  else
  {
    data->m_sampler = RenderState::GetLinearSamplerState();
    data->m_type = TextureData::Type::Type_Texture2D;
  }

  auto ext = PathToString(texture_path->second.extension());
  Common::ToLower(&ext);
  if (ext == ".dds")
  {
    if (!LoadDDSTexture(&data->m_texture, PathToString(texture_path->second)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load dds texture!", asset_id);
      return {};
    }

    if (data->m_texture.m_slices.empty()) [[unlikely]]
      data->m_texture.m_slices.push_back({});

    if (!LoadMips(texture_path->second, &data->m_texture.m_slices[0]))
      return {};

    return LoadInfo{GetAssetSize(data->m_texture) + metadata_size, GetLastAssetWriteTime(asset_id)};
  }
  else if (ext == ".png")
  {
    // PNG could support more complicated texture types in the future
    // but for now just error
    if (data->m_type != TextureData::Type::Type_Texture2D)
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - PNG is not supported for texture type '{}'!",
                    asset_id, data->m_type);
      return {};
    }

    // If we have no slices, create one
    if (data->m_texture.m_slices.empty())
      data->m_texture.m_slices.push_back({});

    auto& slice = data->m_texture.m_slices[0];
    // If we have no levels, create one to pass into LoadPNGTexture
    if (slice.m_levels.empty())
      slice.m_levels.push_back({});

    if (!LoadPNGTexture(&slice.m_levels[0], PathToString(texture_path->second)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load png texture!", asset_id);
      return {};
    }

    if (!LoadMips(texture_path->second, &slice))
      return {};

    return LoadInfo{GetAssetSize(data->m_texture) + metadata_size, GetLastAssetWriteTime(asset_id)};
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' error - extension '{}' unknown!", asset_id, ext);
  return {};
}

void DirectFilesystemAssetLibrary::SetAssetIDMapData(const AssetID& asset_id,
                                                     AssetMap asset_path_map)
{
  AssetMap previous_asset_map;
  {
    std::lock_guard lk(m_asset_map_lock);
    previous_asset_map = m_assetid_to_asset_map_path[asset_id];
  }

  {
    std::lock_guard lk(m_path_map_lock);
    for (const auto& [name, path] : previous_asset_map)
    {
      m_path_to_asset_id.erase(PathToString(path));
    }

    for (const auto& [name, path] : asset_path_map)
    {
      m_path_to_asset_id[PathToString(path)] = asset_id;
    }
  }

  {
    std::lock_guard lk(m_asset_map_lock);
    m_assetid_to_asset_map_path[asset_id] = std::move(asset_path_map);
  }
}

void DirectFilesystemAssetLibrary::PathModified(std::string_view path)
{
  std::lock_guard lk(m_path_map_lock);
  if (const auto iter = m_path_to_asset_id.find(path); iter != m_path_to_asset_id.end())
  {
    auto& system = Core::System::GetInstance();
    auto& loader = system.GetCustomAssetLoader();
    loader.ReloadAsset(iter->second);
  }
}

bool DirectFilesystemAssetLibrary::LoadMips(const std::filesystem::path& asset_path,
                                            CustomTextureData::ArraySlice* data)
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

    VideoCommon::CustomTextureData::ArraySlice::Level level;
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
  std::lock_guard lk(m_asset_map_lock);
  if (auto iter = m_assetid_to_asset_map_path.find(asset_id);
      iter != m_assetid_to_asset_map_path.end())
  {
    return iter->second;
  }
  return {};
}
}  // namespace VideoCommon
