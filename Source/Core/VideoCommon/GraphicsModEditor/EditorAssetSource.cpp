// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"

#include <chrono>
#include <optional>

#include <fmt/format.h>
#include <fmt/std.h>
#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "Core/System.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Assets/TextureAssetUtils.h"
#include "VideoCommon/Resources/CustomResourceManager.h"

namespace
{
std::size_t GetAssetSize(const VideoCommon::CustomTextureData& data)
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

std::optional<picojson::object> GetJsonObjectFromFile(const std::filesystem::path& path)
{
  picojson::value root;
  std::string error;
  if (!JsonFromFile(PathToString(path), &root, &error))
  {
    ERROR_LOG_FMT(VIDEO, "Json file at path '{}' has error '{}'!", PathToString(path), error);
    return std::nullopt;
  }

  if (!root.is<picojson::object>())
  {
    return std::nullopt;
  }

  return root.get<picojson::object>();
}
}  // namespace

namespace GraphicsModEditor
{
EditorAssetSource::LoadInfo EditorAssetSource::LoadTexture(const AssetID& asset_id,
                                                           VideoCommon::CustomTextureData* data)
{
  std::filesystem::path texture_path;
  {
    std::lock_guard lk(m_asset_lock);
    const auto asset = GetAssetFromID(asset_id);
    if (!asset)
    {
      ERROR_LOG_FMT(VIDEO, "Asset with id '{}' not found!", asset_id);
      return {};
    }

    if (const auto it = asset->m_asset_map.find("texture"); it != asset->m_asset_map.end())
    {
      texture_path = it->second;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not find 'texture' in asset map!", asset_id);
      return {};
    }
  }

  if (!VideoCommon::LoadTextureDataFromFile(asset_id, texture_path, AbstractTextureType::Texture_2D,
                                            data))
  {
    return {};
  }
  if (!VideoCommon::PurgeInvalidMipsFromTextureData(asset_id, data))
    return {};

  return LoadInfo{GetAssetSize(*data)};
}

EditorAssetSource::LoadInfo EditorAssetSource::LoadTexture(const AssetID& asset_id,
                                                           VideoCommon::TextureAndSamplerData* data)
{
  std::filesystem::path texture_path;
  {
    std::lock_guard lk(m_asset_lock);
    const auto asset = GetAssetFromID(asset_id);
    if (!asset)
    {
      ERROR_LOG_FMT(VIDEO, "Asset with id '{}' not found!", asset_id);
      return {};
    }

    if (const auto it = asset->m_asset_map.find("texture"); it != asset->m_asset_map.end())
    {
      texture_path = it->second;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not find 'texture' in asset map!", asset_id);
      return {};
    }
    if (auto texture_data =
            std::get_if<std::unique_ptr<VideoCommon::TextureAndSamplerData>>(&asset->m_data))
    {
      data->sampler = texture_data->get()->sampler;
      data->type = texture_data->get()->type;
    }
  }

  if (!VideoCommon::LoadTextureDataFromFile(asset_id, texture_path, data->type,
                                            &data->texture_data))
  {
    return {};
  }
  if (!VideoCommon::PurgeInvalidMipsFromTextureData(asset_id, &data->texture_data))
    return {};

  return LoadInfo{GetAssetSize(data->texture_data)};
}

EditorAssetSource::LoadInfo
EditorAssetSource::LoadRasterSurfaceShader(const AssetID& asset_id,
                                           VideoCommon::RasterSurfaceShaderData* data)
{
  std::lock_guard lk(m_asset_lock);
  auto asset = GetAssetFromID(asset_id);
  if (asset)
  {
    if (auto shader_data =
            std::get_if<std::unique_ptr<VideoCommon::RasterSurfaceShaderData>>(&asset->m_data))
    {
      if (const auto it = asset->m_asset_map.find("vertex_shader"); it != asset->m_asset_map.end())
      {
        if (!File::ReadFileToString(PathToString(it->second), shader_data->get()->vertex_source))
        {
          return {};
        }
      }

      if (const auto it = asset->m_asset_map.find("pixel_shader"); it != asset->m_asset_map.end())
      {
        if (!File::ReadFileToString(PathToString(it->second), shader_data->get()->pixel_source))
        {
          return {};
        }

        const std::string graphics_mod_builtin =
            File::GetSysDirectory() + GRAPHICSMOD_DIR + "/Builtin" + "/Shaders";

        data->shader_includer = std::make_unique<VideoCommon::ShaderIncluder>(
            PathToString(it->second.parent_path()), graphics_mod_builtin);
      }

      if (shader_data->get()->vertex_source != "" || shader_data->get()->pixel_source != "")
      {
        data->pixel_source = shader_data->get()->pixel_source;
        data->vertex_source = shader_data->get()->vertex_source;
        data->samplers = shader_data->get()->samplers;
        data->uniform_properties = shader_data->get()->uniform_properties;
        data->name_to_property = shader_data->get()->name_to_property;

        return LoadInfo{sizeof(VideoCommon::RasterSurfaceShaderData)};
      }
    }
  }

  return {};
}

EditorAssetSource::LoadInfo EditorAssetSource::LoadMaterial(const AssetID& asset_id,
                                                            VideoCommon::MaterialData* data)
{
  std::lock_guard lk(m_asset_lock);
  auto asset = GetAssetFromID(asset_id);
  if (asset)
  {
    if (auto material_data =
            std::get_if<std::unique_ptr<VideoCommon::MaterialData>>(&asset->m_data))
    {
      *data = *material_data->get();
      return LoadInfo{sizeof(VideoCommon::MaterialData)};
    }
  }

  return {};
}

EditorAssetSource::LoadInfo EditorAssetSource::LoadMesh(const AssetID& asset_id,
                                                        VideoCommon::MeshData* data)
{
  std::filesystem::path mesh_path;
  {
    std::lock_guard lk(m_asset_lock);
    const auto asset = GetAssetFromID(asset_id);
    if (!asset)
    {
      ERROR_LOG_FMT(VIDEO, "Asset with id '{}' not found!", asset_id);
      return {};
    }

    if (const auto it = asset->m_asset_map.find("mesh"); it != asset->m_asset_map.end())
    {
      mesh_path = it->second;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not find 'mesh' in asset map!", asset_id);
      return {};
    }
    if (auto mesh_data = std::get_if<std::unique_ptr<VideoCommon::MeshData>>(&asset->m_data))
    {
      data->m_mesh_material_to_material_asset_id =
          mesh_data->get()->m_mesh_material_to_material_asset_id;
    }
  }

  auto ext = PathToString(mesh_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dolmesh")
  {
    File::IOFile file(PathToString(mesh_path), "rb");
    if (!file.IsOpen())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - failed to open mesh file '{}'!", asset_id,
                    PathToString(mesh_path));
      return {};
    }

    std::vector<u8> bytes(file.GetSize());
    file.ReadBytes(bytes.data(), file.GetSize());
    if (!VideoCommon::MeshData::FromDolphinMesh(bytes, data))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the mesh file '{}'!", asset_id,
                    PathToString(mesh_path));
      return {};
    }

    // SetAssetPreviewData(asset_id, data->m_texture);
    return LoadInfo{1};
  }

  return {};
}

EditorAssetSource::LoadInfo EditorAssetSource::LoadRenderTarget(const AssetID& asset_id,
                                                                VideoCommon::RenderTargetData* data)
{
  std::lock_guard lk(m_asset_lock);
  auto asset = GetAssetFromID(asset_id);
  if (asset)
  {
    if (auto render_target_data =
            std::get_if<std::unique_ptr<VideoCommon::RenderTargetData>>(&asset->m_data))
    {
      *data = *render_target_data->get();
      return LoadInfo{sizeof(VideoCommon::RenderTargetData)};
    }
  }

  return {};
}

EditorAsset* EditorAssetSource::GetAssetFromPath(const std::filesystem::path& asset_path)
{
  std::lock_guard lk(m_asset_lock);
  const auto it = m_path_to_editor_asset.find(asset_path);
  if (it == m_path_to_editor_asset.end())
    return nullptr;

  return &(*it->second);
}

const EditorAsset* EditorAssetSource::GetAssetFromID(const AssetID& asset_id) const
{
  std::lock_guard lk(m_asset_lock);

  const auto asset_it = m_asset_id_to_file_path.find(asset_id);
  if (asset_it == m_asset_id_to_file_path.end())
    return nullptr;

  const auto it = m_path_to_editor_asset.find(asset_it->second);
  if (it == m_path_to_editor_asset.end())
    return nullptr;

  return &(*it->second);
}

EditorAsset* EditorAssetSource::GetAssetFromID(const AssetID& asset_id)
{
  std::lock_guard lk(m_asset_lock);

  const auto asset_it = m_asset_id_to_file_path.find(asset_id);
  if (asset_it == m_asset_id_to_file_path.end())
    return nullptr;

  const auto it = m_path_to_editor_asset.find(asset_it->second);
  if (it == m_path_to_editor_asset.end())
  {
    const auto builtin_it = m_path_to_builtin_editor_asset.find(asset_it->second);
    if (builtin_it == m_path_to_builtin_editor_asset.end())
      return nullptr;

    return &(*builtin_it->second);
  }

  return &(*it->second);
}

void EditorAssetSource::AddAsset(const std::filesystem::path& asset_path)
{
  const std::string uuid_str =
      fmt::format("{}", std::chrono::system_clock::now().time_since_epoch().count());
  AddAsset(asset_path, std::move(uuid_str));
}

void EditorAssetSource::AddBuiltinAsset(const std::filesystem::path& asset_path,
                                        const AssetID& uuid)
{
  EditorAsset asset;
  if (LoadAssetFromFile(asset_path, uuid, &asset))
  {
    std::lock_guard lk(m_asset_lock);

    m_builtin_assets.emplace_back(std::move(asset));
    for (const auto& [name, path] : m_builtin_assets.back().m_asset_map)
    {
      m_path_to_builtin_editor_asset.insert_or_assign(WithUnifiedPathSeparators(PathToString(path)),
                                                      std::prev(m_builtin_assets.end()));
    }
    m_asset_id_to_file_path.insert_or_assign(uuid, asset_path);
  }
}

void EditorAssetSource::AddAsset(const std::filesystem::path& asset_path, AssetID uuid)
{
  EditorAsset asset;
  if (LoadAssetFromFile(asset_path, uuid, &asset))
  {
    std::lock_guard lk(m_asset_lock);

    m_assets.emplace_back(std::move(asset));
    for (const auto& [name, path] : m_assets.back().m_asset_map)
    {
      m_path_to_editor_asset.insert_or_assign(WithUnifiedPathSeparators(PathToString(path)),
                                              std::prev(m_assets.end()));
    }
    m_asset_id_to_file_path.insert_or_assign(uuid, asset_path);
  }
}

bool EditorAssetSource::LoadAssetFromFile(const std::filesystem::path& asset_path,
                                          const AssetID& uuid, EditorAsset* asset)
{
  asset->m_valid = true;
  bool add = false;
  const std::string filename = PathToString(asset_path.filename());
  auto ext = PathToString(asset_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds" || ext == ".png")
  {
    auto texture_data = std::make_unique<VideoCommon::TextureAndSamplerData>();

    // Metadata is optional and will be created
    // if it doesn't exist
    const auto root = asset_path.parent_path();
    const auto name = asset_path.stem();
    const auto extension = asset_path.extension();
    auto result = root / name;
    result += ".texture";
    if (std::filesystem::exists(result))
    {
      if (auto json = GetJsonObjectFromFile(result))
      {
        VideoCommon::TextureAndSamplerData::FromJson(uuid, *json, texture_data.get());
      }
    }
    else
    {
      texture_data->type = AbstractTextureType::Texture_2D;

      // Write initial data to a file, so that if any changes are made
      // they get picked up and written on save
      picojson::object obj;
      VideoCommon::TextureAndSamplerData::ToJson(&obj, *texture_data);
      JsonToFile(PathToString(result), picojson::value{obj}, true);
    }
    asset->m_asset_map["metadata"] = result;

    add = true;
    asset->m_data = std::move(texture_data);
    asset->m_data_type = Texture;
    asset->m_asset_map["texture"] = asset_path;
  }
  else if (ext == ".rastershader")
  {
    // Either the vertex or pixel shaders may exist
    const auto root = asset_path.parent_path();
    const auto name = asset_path.stem();
    const auto extension = asset_path.extension();
    const auto pixel_shader = (root / name).replace_extension(".ps.glsl");
    const auto vertex_shader = (root / name).replace_extension(".vs.glsl");

    if (auto json = GetJsonObjectFromFile(asset_path))
    {
      auto shader_data = std::make_unique<VideoCommon::RasterSurfaceShaderData>();

      bool uses_custom_shader = false;

      if (File::ReadFileToString(PathToString(pixel_shader), shader_data->pixel_source))
      {
        asset->m_asset_map["pixel_shader"] = pixel_shader;
        uses_custom_shader = true;
      }

      if (File::ReadFileToString(PathToString(vertex_shader), shader_data->vertex_source))
      {
        asset->m_asset_map["vertex_shader"] = vertex_shader;
        uses_custom_shader = true;
      }

      if (uses_custom_shader &&
          VideoCommon::RasterSurfaceShaderData::FromJson(uuid, *json, shader_data.get()))
      {
        asset->m_data = std::move(shader_data);
        asset->m_data_type = Shader;
        asset->m_asset_map["metadata"] = asset_path;
        add = true;
      }
    }
  }
  else if (ext == ".dolmesh")
  {
    auto mesh_data = std::make_unique<VideoCommon::MeshData>();

    // Only valid if metadata file exists
    const auto root = asset_path.parent_path();
    const auto name = asset_path.stem();
    const auto extension = asset_path.extension();
    auto result = root / name;
    result += ".metadata";
    if (std::filesystem::exists(result))
    {
      if (auto json = GetJsonObjectFromFile(result))
      {
        VideoCommon::MeshData::FromJson(uuid, *json, mesh_data.get());
        add = true;
        asset->m_data = std::move(mesh_data);
        asset->m_data_type = Mesh;
        asset->m_asset_map["mesh"] = asset_path;
      }
      asset->m_asset_map["metadata"] = result;
    }
  }
  else if (ext == ".material" || ext == ".rastermaterial")
  {
    if (auto json = GetJsonObjectFromFile(asset_path))
    {
      auto material_data = std::make_unique<VideoCommon::MaterialData>();
      if (VideoCommon::MaterialData::FromJson(uuid, *json, material_data.get()))
      {
        add = true;
        asset->m_data = std::move(material_data);
        asset->m_data_type = Material;
        asset->m_asset_map["metadata"] = asset_path;
      }
    }
  }
  else if (ext == ".rendertarget")
  {
    if (auto json = GetJsonObjectFromFile(asset_path))
    {
      auto render_target_data = std::make_unique<VideoCommon::RenderTargetData>();
      if (VideoCommon::RenderTargetData::FromJson(uuid, *json, render_target_data.get()))
      {
        add = true;
        asset->m_data = std::move(render_target_data);
        asset->m_data_type = RenderTarget;
        asset->m_asset_map["metadata"] = asset_path;
      }
    }
  }

  if (add)
  {
    asset->m_asset_id = uuid;
    asset->m_asset_path = asset_path;
    asset->m_last_data_write = VideoCommon::CustomAsset::ClockType::now();
  }

  return add;
}

void EditorAssetSource::RemoveAsset(const std::filesystem::path& asset_path)
{
  std::lock_guard lk(m_asset_lock);
  if (const auto it = m_path_to_editor_asset.find(asset_path); it != m_path_to_editor_asset.end())
  {
    const auto& asset_it = it->second;
    m_asset_id_to_file_path.erase(asset_it->m_asset_id);

    // Make a copy as we are going to remove the found iterator
    const auto asset_map = asset_it->m_asset_map;
    for (const auto& [name, path] : asset_map)
    {
      m_path_to_editor_asset.erase(path);
    }

    m_assets.erase(asset_it);
  }
}

bool EditorAssetSource::RenameAsset(const std::filesystem::path& old_path,
                                    const std::filesystem::path& new_path)
{
  // If old path and new path are the same
  // abort the rename
  if (old_path == new_path)
    return true;

  if (std::filesystem::exists(new_path))
    return false;

  std::lock_guard lk(m_asset_lock);
  const auto it = m_path_to_editor_asset.find(old_path);
  if (it == m_path_to_editor_asset.end())
    return false;

  // Remove old entries, they will be rebuilt
  for (const auto& [name, path] : it->second->m_asset_map)
  {
    m_path_to_editor_asset.erase(path);
  }

  it->second->m_asset_path = new_path;

  auto ext = PathToString(old_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds" || ext == ".png")
  {
    const auto old_root = old_path.parent_path();
    const auto old_name = old_path.stem();
    auto old_result = old_root / old_name;
    old_result += ".texture";
    if (std::filesystem::exists(old_result))
    {
      const auto new_root = new_path.parent_path();
      const auto new_name = new_path.stem();
      auto new_result = new_root / new_name;
      new_result += ".texture";
      std::filesystem::rename(old_result, new_result);

      it->second->m_asset_map["metadata"] = new_result;
    }
    it->second->m_asset_map["texture"] = new_path;
  }
  else if (ext == ".gltf")
  {
    const auto old_root = old_path.parent_path();
    const auto old_name = old_path.stem();
    auto old_result = old_root / old_name;
    old_result += ".metadata";
    if (std::filesystem::exists(old_result))
    {
      const auto new_root = new_path.parent_path();
      const auto new_name = new_path.stem();
      auto new_result = new_root / new_name;
      new_result += ".metadata";
      std::filesystem::rename(old_result, new_result);

      it->second->m_asset_map["metadata"] = new_result;
    }
    it->second->m_asset_map["mesh"] = new_path;
  }
  else if (ext == ".rastershader")
  {
    it->second->m_asset_map["metadata"] = new_path;

    const auto old_root = old_path.parent_path();
    const auto old_name = old_path.stem();
    const auto old_pixel_shader_path = (old_root / old_name).replace_extension(".ps.glsl");
    if (std::filesystem::exists(old_pixel_shader_path))
    {
      const auto new_root = new_path.parent_path();
      const auto new_name = new_path.stem();
      const auto new_pixel_shader_path = (new_root / new_name).replace_extension(".ps.glsl");
      std::filesystem::rename(old_pixel_shader_path, new_pixel_shader_path);

      it->second->m_asset_map["pixel_shader"] = new_pixel_shader_path;
    }
    const auto old_vertex_shader_path = (old_root / old_name).replace_extension(".vs.glsl");
    if (std::filesystem::exists(old_vertex_shader_path))
    {
      const auto new_root = new_path.parent_path();
      const auto new_name = new_path.stem();
      const auto new_vertex_shader_path = (new_root / new_name).replace_extension(".vs.glsl");
      std::filesystem::rename(old_vertex_shader_path, new_vertex_shader_path);

      it->second->m_asset_map["vertex_shader"] = new_vertex_shader_path;
    }
  }
  else if (ext == ".material")
  {
    it->second->m_asset_map["metadata"] = new_path;
  }
  else if (ext == ".rastermaterial")
  {
    it->second->m_asset_map["metadata"] = new_path;
  }
  else if (ext == ".rendertarget")
  {
    it->second->m_asset_map["metadata"] = new_path;
  }
  m_asset_id_to_file_path[it->second->m_asset_id] = new_path;

  // Rebuild with our new paths
  for (const auto& [name, path] : it->second->m_asset_map)
  {
    m_path_to_editor_asset.insert_or_assign(path, it->second);
  }

  std::filesystem::rename(old_path, new_path);
  return true;
}

void EditorAssetSource::AddAssets(
    std::span<const GraphicsModSystem::Config::GraphicsModAsset> assets,
    const std::filesystem::path& root)
{
  for (const auto& asset : assets)
  {
    for (const auto& [name, path] : asset.m_map)
    {
      AddAsset(root / path, asset.m_asset_id);
    }
  }
}

std::vector<GraphicsModSystem::Config::GraphicsModAsset>
EditorAssetSource::GetAssets(const std::filesystem::path& root) const
{
  std::lock_guard lk(m_asset_lock);
  std::vector<GraphicsModSystem::Config::GraphicsModAsset> assets;
  for (const auto& [asset_id, path] : m_asset_id_to_file_path)
  {
    GraphicsModSystem::Config::GraphicsModAsset asset_config;
    asset_config.m_asset_id = asset_id;
    const auto it = m_path_to_editor_asset.find(path);

    // Skip any builtin assets
    if (it == m_path_to_editor_asset.end())
      continue;
    for (const auto& [name, am_path] : it->second->m_asset_map)
    {
      asset_config.m_map[name] = std::filesystem::relative(am_path, root);
    }
    assets.push_back(std::move(asset_config));
  }
  return assets;
}

void EditorAssetSource::SaveAssetDataAsFiles() const
{
  const auto save_json_to_file = [](const std::string& file_path,
                                    const picojson::object& serialized_root) {
    std::ofstream json_stream;
    File::OpenFStream(json_stream, file_path, std::ios_base::out);
    if (!json_stream.is_open())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to open file '{}' for writing", file_path);
      return;
    }

    const auto output = picojson::value{serialized_root}.serialize(true);
    json_stream << output;
  };
  std::lock_guard lk(m_asset_lock);
  for (const auto& pair : m_path_to_editor_asset)
  {
    // Workaround for some compilers not being able
    // to capture structured bindings in lambda
    const auto& asset = *pair.second;
    std::visit(
        overloaded{[&](const std::unique_ptr<VideoCommon::MaterialData>& material_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::MaterialData::ToJson(&serialized_root, *material_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   },
                   [&](const std::unique_ptr<VideoCommon::RasterSurfaceShaderData>& shader_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::RasterSurfaceShaderData::ToJson(serialized_root, *shader_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   },
                   [&](const std::unique_ptr<VideoCommon::TextureAndSamplerData>& texture_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::TextureAndSamplerData::ToJson(&serialized_root, *texture_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   },
                   [&](const std::unique_ptr<VideoCommon::MeshData>& mesh_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::MeshData::ToJson(serialized_root, *mesh_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   },
                   [&](const std::unique_ptr<VideoCommon::RenderTargetData>& render_target_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::RenderTargetData::ToJson(&serialized_root, *render_target_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   }},
        asset.m_data);
  }
}

const std::list<EditorAsset>& EditorAssetSource::GetAllAssets() const
{
  std::lock_guard lk(m_asset_lock);
  return m_assets;
}

void EditorAssetSource::PathAdded(std::string_view path)
{
  // TODO: notify our preview that it needs to be calculated
}

void EditorAssetSource::PathModified(std::string_view path)
{
  std::lock_guard lk(m_asset_lock);
  if (const auto iter = m_path_to_editor_asset.find(path); iter != m_path_to_editor_asset.end())
  {
    auto& system = Core::System::GetInstance();
    auto& resource_manager = system.GetCustomResourceManager();
    resource_manager.MarkAssetDirty(iter->second->m_asset_id);
  }

  if (const auto iter = m_path_to_builtin_editor_asset.find(path);
      iter != m_path_to_builtin_editor_asset.end())
  {
    auto& system = Core::System::GetInstance();
    auto& resource_manager = system.GetCustomResourceManager();
    resource_manager.MarkAssetDirty(iter->second->m_asset_id);
  }

  // TODO: notify our preview that something changed
}

void EditorAssetSource::PathRenamed(std::string_view old_path, std::string_view new_path)
{
  // TODO: notify our preview that our path changed
}

void EditorAssetSource::PathDeleted(std::string_view path)
{
  // TODO: notify our preview that we no longer care about this path
}

AbstractTexture* EditorAssetSource::GetAssetPreview(const AssetID& asset_id)
{
  /*std::lock_guard lk(m_preview_lock);
  if (const auto it = m_asset_id_to_preview.find(asset_id); it != m_asset_id_to_preview.end())
  {
    if (it->second.m_preview_data)
    {
      if (it->second.m_preview_data->m_slices.empty()) [[unlikely]]
        return nullptr;

      auto& first_slice = it->second.m_preview_data->m_slices[0];
      if (first_slice.m_levels.empty()) [[unlikely]]
        return nullptr;

      auto& first_level = first_slice.m_levels[0];
      it->second.m_preview_texture = g_gfx->CreateTexture(
          TextureConfig{first_level.width, first_level.height, 1, 1, 1,
                        AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2DArray});

      it->second.m_preview_texture->Load(0, first_level.width, first_level.height,
                                         first_level.row_length, first_level.data.data(),
                                         first_level.data.size());

      it->second.m_preview_data.reset();
    }

    return it->second.m_preview_texture.get();
  }*/

  return nullptr;
}

void EditorAssetSource::SetAssetPreviewData(const AssetID&, const VideoCommon::CustomTextureData&)
{
  /*std::lock_guard lk(m_preview_lock);
  const auto [it, inserted] = m_asset_id_to_preview.try_emplace(
      asset_id, AssetPreview{.m_preview_data = {}, .m_preview_texture = nullptr});
  it->second.m_preview_data = preview_data;*/
}

}  // namespace GraphicsModEditor
