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
#include "VideoCommon/Assets/CustomResourceManager.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Assets/TextureAssetUtils.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
namespace
{
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

CustomAssetLibrary::LoadInfo
DirectFilesystemAssetLibrary::LoadRasterSurfaceShader(const AssetID& asset_id,
                                                      RasterSurfaceShaderData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);

  // Asset map for a pixel shader is the shader and some metadata
  if (asset_map.size() != 3)
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have three files mapped!", asset_id);
    return {};
  }

  const auto metadata = asset_map.find("metadata");
  if (metadata == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a metadata entry mapped!", asset_id);
    return {};
  }

  const auto vertex_shader = asset_map.find("vertex_shader");
  if (vertex_shader == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a vertex shader entry mapped!", asset_id);
    return {};
  }

  const auto pixel_shader = asset_map.find("pixel_shader");
  if (pixel_shader == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a pixel shader entry mapped!", asset_id);
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
  std::size_t vertex_shader_size;
  {
    std::error_code ec;
    vertex_shader_size = std::filesystem::file_size(vertex_shader->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(
          VIDEO, "Asset '{}' error - failed to get vertex shader source file size with error '{}'!",
          asset_id, ec);
      return {};
    }
  }
  std::size_t pixel_shader_size;
  {
    std::error_code ec;
    pixel_shader_size = std::filesystem::file_size(pixel_shader->second, ec);
    if (ec)
    {
      ERROR_LOG_FMT(
          VIDEO, "Asset '{}' error - failed to get pixel shader source file size with error '{}'!",
          asset_id, ec);
      return {};
    }
  }
  const auto approx_mem_size = metadata_size + vertex_shader_size + pixel_shader_size;

  if (!File::ReadFileToString(PathToString(vertex_shader->second), data->vertex_source))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the vertex shader file '{}',",
                  asset_id, PathToString(vertex_shader->second));
    return {};
  }

  if (!File::ReadFileToString(PathToString(pixel_shader->second), data->pixel_source))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the pixel shader file '{}',", asset_id,
                  PathToString(pixel_shader->second));
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

  if (!RasterSurfaceShaderData::FromJson(asset_id, root_obj, data))
    return {};

  return LoadInfo{approx_mem_size};
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

  return LoadInfo{metadata_size};
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

  return LoadInfo{approx_mem_size};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadTexture(const AssetID& asset_id,
                                                                       CustomTextureData* data)
{
  const auto asset_map = GetAssetMapForID(asset_id);
  if (asset_map.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' error - raw texture expected to have one or two files mapped!",
                  asset_id);
    return {};
  }

  const auto texture_path = asset_map.find("texture");

  if (texture_path == asset_map.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' expected to have a texture entry mapped!", asset_id);
    return {};
  }

  if (!LoadTextureDataFromFile(asset_id, texture_path->second, AbstractTextureType::Texture_2D,
                               data))
  {
    return {};
  }
  if (!PurgeInvalidMipsFromTextureData(asset_id, data))
    return {};

  return LoadInfo{GetAssetSize(*data)};
}

CustomAssetLibrary::LoadInfo DirectFilesystemAssetLibrary::LoadTexture(const AssetID& asset_id,
                                                                       TextureAndSamplerData* data)
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
    if (!TextureAndSamplerData::FromJson(asset_id, root_obj, data))
    {
      return {};
    }
  }
  else
  {
    data->sampler = RenderState::GetLinearSamplerState();
    data->type = AbstractTextureType::Texture_2D;
  }

  if (!LoadTextureDataFromFile(asset_id, texture_path->second, data->type, &data->texture_data))
    return {};
  if (!PurgeInvalidMipsFromTextureData(asset_id, &data->texture_data))
    return {};

  return LoadInfo{GetAssetSize(data->texture_data) + metadata_size};
}

void DirectFilesystemAssetLibrary::SetAssetIDMapData(const AssetID& asset_id,
                                                     VideoCommon::Assets::AssetMap asset_path_map)
{
  VideoCommon::Assets::AssetMap previous_asset_map;
  {
    std::lock_guard lk(m_asset_map_lock);
    previous_asset_map = m_asset_id_to_asset_map_path[asset_id];
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
    m_asset_id_to_asset_map_path[asset_id] = std::move(asset_path_map);
  }
}

void DirectFilesystemAssetLibrary::PathModified(std::string_view path)
{
  std::lock_guard lk(m_path_map_lock);
  if (const auto iter = m_path_to_asset_id.find(path); iter != m_path_to_asset_id.end())
  {
    auto& system = Core::System::GetInstance();
    auto& resource_manager = system.GetCustomResourceManager();
    resource_manager.MarkAssetDirty(iter->second);
  }
}

VideoCommon::Assets::AssetMap
DirectFilesystemAssetLibrary::GetAssetMapForID(const AssetID& asset_id) const
{
  std::lock_guard lk(m_asset_map_lock);
  if (auto iter = m_asset_id_to_asset_map_path.find(asset_id);
      iter != m_asset_id_to_asset_map_path.end())
  {
    return iter->second;
  }
  return {};
}
}  // namespace VideoCommon
