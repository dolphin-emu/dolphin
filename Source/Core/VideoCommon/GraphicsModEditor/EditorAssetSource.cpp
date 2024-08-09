// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"

#include <chrono>
#include <optional>

#include <fmt/format.h>
#include <fmt/std.h>
#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "Core/System.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/CustomTextureData.h"

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

std::chrono::system_clock::time_point FileTimeToSysTime(std::filesystem::file_time_type file_time)
{
  // Note: all compilers should switch to chrono::clock_cast
  // once it is available for use
  const auto system_time_now = std::chrono::system_clock::now();
  const auto file_time_now = decltype(file_time)::clock::now();
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      file_time - file_time_now + system_time_now);
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
                                                           VideoCommon::TextureData* data)
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
    if (auto texture_data = std::get_if<std::unique_ptr<VideoCommon::TextureData>>(&asset->m_data))
    {
      data->m_sampler = texture_data->get()->m_sampler;
      data->m_type = texture_data->get()->m_type;
    }
  }

  auto ext = PathToString(texture_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds")
  {
    if (!LoadDDSTexture(&data->m_texture, PathToString(texture_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load dds texture!", asset_id);
      return {};
    }

    // SetAssetPreviewData(asset_id, data->m_texture);
    return LoadInfo{GetAssetSize(data->m_texture), GetLastAssetWriteTime(asset_id)};
  }
  else if (ext == ".png")
  {
    // PNG could support more complicated texture types in the future
    // but for now just error
    if (data->m_type != VideoCommon::TextureData::Type::Type_Texture2D)
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

    if (!LoadPNGTexture(&slice.m_levels[0], PathToString(texture_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load png texture!", asset_id);
      return {};
    }

    // SetAssetPreviewData(asset_id, data->m_texture);
    return LoadInfo{GetAssetSize(data->m_texture), GetLastAssetWriteTime(asset_id)};
  }

  return {};
}

EditorAssetSource::LoadInfo EditorAssetSource::LoadPixelShader(const AssetID& asset_id,
                                                               VideoCommon::PixelShaderData* data)
{
  std::lock_guard lk(m_asset_lock);
  auto asset = GetAssetFromID(asset_id);
  if (asset)
  {
    if (auto pixel_shader_data =
            std::get_if<std::unique_ptr<VideoCommon::PixelShaderData>>(&asset->m_data))
    {
      if (const auto it = asset->m_asset_map.find("shader"); it != asset->m_asset_map.end())
      {
        if (File::ReadFileToString(PathToString(it->second),
                                   pixel_shader_data->get()->m_shader_source))
        {
          *data = *pixel_shader_data->get();
          return LoadInfo{sizeof(VideoCommon::PixelShaderData), GetLastAssetWriteTime(asset_id)};
        }
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
      return LoadInfo{sizeof(VideoCommon::MaterialData), asset->m_last_data_write};
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
    std::vector<u8> bytes;
    bytes.reserve(file.GetSize());
    file.ReadBytes(bytes.data(), file.GetSize());
    if (!VideoCommon::MeshData::FromDolphinMesh(bytes, data))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error -  failed to load the mesh file '{}',", asset_id,
                    PathToString(mesh_path));
      return {};
    }

    // SetAssetPreviewData(asset_id, data->m_texture);
    return LoadInfo{1, GetLastAssetWriteTime(asset_id)};
  }

  return {};
}

EditorAssetSource::TimeType EditorAssetSource::GetLastAssetWriteTime(const AssetID& asset_id) const
{
  std::lock_guard lk(m_asset_lock);
  auto asset = GetAssetFromID(asset_id);
  if (asset)
  {
    CustomAssetLibrary::TimeType max_entry = asset->m_last_data_write;
    for (const auto& [name, path] : asset->m_asset_map)
    {
      std::error_code ec;
      const auto tp = std::filesystem::last_write_time(path, ec);
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

EditorAsset* EditorAssetSource::GetAssetFromPath(const std::filesystem::path& asset_path)
{
  std::lock_guard lk(m_asset_lock);
  const auto it = m_path_to_editor_asset.find(asset_path);
  if (it == m_path_to_editor_asset.end())
    return nullptr;

  return &it->second;
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

  return &it->second;
}

EditorAsset* EditorAssetSource::GetAssetFromID(const AssetID& asset_id)
{
  std::lock_guard lk(m_asset_lock);

  const auto asset_it = m_asset_id_to_file_path.find(asset_id);
  if (asset_it == m_asset_id_to_file_path.end())
    return nullptr;

  const auto it = m_path_to_editor_asset.find(asset_it->second);
  if (it == m_path_to_editor_asset.end())
    return nullptr;

  return &it->second;
}

void EditorAssetSource::AddAsset(const std::filesystem::path& asset_path)
{
  std::string uuid_str =
      fmt::format("{}", std::chrono::system_clock::now().time_since_epoch().count());
  AddAsset(asset_path, std::move(uuid_str));
}

void EditorAssetSource::AddAsset(const std::filesystem::path& asset_path, AssetID uuid)
{
  std::lock_guard lk(m_asset_lock);
  EditorAsset asset;
  asset.m_valid = true;
  bool add = false;
  const std::string filename = PathToString(asset_path.filename());
  auto ext = PathToString(asset_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds" || ext == ".png")
  {
    auto texture_data = std::make_unique<VideoCommon::TextureData>();

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
        VideoCommon::TextureData::FromJson(uuid, *json, texture_data.get());
      }
    }
    else
    {
      texture_data->m_type = VideoCommon::TextureData::Type::Type_Texture2D;

      // Write initial data to a file, so that if any changes are made
      // they get picked up and written on save
      picojson::object obj;
      VideoCommon::TextureData::ToJson(&obj, *texture_data);
      JsonToFile(PathToString(result), picojson::value{obj}, true);
    }
    asset.m_asset_map["metadata"] = result;

    add = true;
    asset.m_data = std::move(texture_data);
    asset.m_data_type = Texture;
    asset.m_asset_map["texture"] = asset_path;
  }
  else if (ext == ".glsl")
  {
    // Only valid if metadata file exists
    const auto root = asset_path.parent_path();
    const auto name = asset_path.stem();
    const auto extension = asset_path.extension();
    auto result = root / name;
    result += ".shader";
    if (std::filesystem::exists(result))
    {
      if (auto json = GetJsonObjectFromFile(result))
      {
        auto pixel_data = std::make_unique<VideoCommon::PixelShaderData>();
        if (File::ReadFileToString(PathToString(asset_path), pixel_data->m_shader_source))
        {
          if (VideoCommon::PixelShaderData::FromJson(uuid, *json, pixel_data.get()))
          {
            add = true;
            asset.m_data = std::move(pixel_data);
            asset.m_data_type = PixelShader;
            asset.m_asset_map["shader"] = asset_path;
            asset.m_asset_map["metadata"] = result;
          }
        }
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
        asset.m_data = std::move(mesh_data);
        asset.m_data_type = Mesh;
        asset.m_asset_map["mesh"] = asset_path;
      }
      asset.m_asset_map["metadata"] = result;
    }
  }
  else if (ext == ".material")
  {
    if (auto json = GetJsonObjectFromFile(asset_path))
    {
      auto material_data = std::make_unique<VideoCommon::MaterialData>();
      if (VideoCommon::MaterialData::FromJson(uuid, *json, material_data.get()))
      {
        add = true;
        asset.m_data = std::move(material_data);
        asset.m_data_type = Material;
        asset.m_asset_map["metadata"] = asset_path;
      }
    }
  }

  if (add)
  {
    asset.m_asset_id = uuid;
    asset.m_asset_path = asset_path;
    asset.m_last_data_write = std::chrono::system_clock::now();
    const auto [it, inserted] =
        m_path_to_editor_asset.insert_or_assign(asset_path, std::move(asset));
    if (inserted)
      m_assets.push_back(&it->second);
    m_asset_id_to_file_path.insert_or_assign(uuid, asset_path);
  }
}

void EditorAssetSource::RemoveAsset(const std::filesystem::path& asset_path)
{
  std::lock_guard lk(m_asset_lock);
  if (const auto it = m_path_to_editor_asset.find(asset_path); it != m_path_to_editor_asset.end())
  {
    std::erase(m_assets, &it->second);
    m_asset_id_to_file_path.erase(it->second.m_asset_id);
    m_path_to_editor_asset.erase(it);
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
  auto extracted_entry = m_path_to_editor_asset.extract(old_path);

  if (!extracted_entry)
    return false;

  extracted_entry.mapped().m_asset_path = new_path;

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

      extracted_entry.mapped().m_asset_map["metadata"] = new_result;
    }
    extracted_entry.mapped().m_asset_map["texture"] = new_path;
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

      extracted_entry.mapped().m_asset_map["metadata"] = new_result;
    }
    extracted_entry.mapped().m_asset_map["mesh"] = new_path;
  }
  else if (ext == ".glsl")
  {
    const auto old_root = old_path.parent_path();
    const auto old_name = old_path.stem();
    auto old_result = old_root / old_name;
    old_result += ".shader";
    if (std::filesystem::exists(old_result))
    {
      const auto new_root = new_path.parent_path();
      const auto new_name = new_path.stem();
      auto new_result = new_root / new_name;
      new_result += ".shader";
      std::filesystem::rename(old_result, new_result);

      extracted_entry.mapped().m_asset_map["metadata"] = new_result;
    }
    extracted_entry.mapped().m_asset_map["shader"] = new_path;
  }
  else if (ext == ".material")
  {
    extracted_entry.mapped().m_asset_map["metadata"] = new_path;
  }
  m_asset_id_to_file_path[extracted_entry.mapped().m_asset_id] = new_path;
  extracted_entry.key() = new_path;
  m_path_to_editor_asset.insert(std::move(extracted_entry));
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
    if (const auto it = m_path_to_editor_asset.find(path); it != m_path_to_editor_asset.end())
    {
      for (const auto& [name, am_path] : it->second.m_asset_map)
      {
        asset_config.m_map[name] = std::filesystem::relative(am_path, root);
      }
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
    const auto& asset = pair.second;
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
                   [&](const std::unique_ptr<VideoCommon::PixelShaderData>& pixel_shader_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::PixelShaderData::ToJson(serialized_root, *pixel_shader_data);
                       save_json_to_file(PathToString(metadata_it->second), serialized_root);
                     }
                   },
                   [&](const std::unique_ptr<VideoCommon::TextureData>& texture_data) {
                     if (const auto metadata_it = asset.m_asset_map.find("metadata");
                         metadata_it != asset.m_asset_map.end())
                     {
                       picojson::object serialized_root;
                       VideoCommon::TextureData::ToJson(&serialized_root, *texture_data);
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
                   }},
        asset.m_data);
  }
}

const std::vector<EditorAsset*>& EditorAssetSource::GetAllAssets() const
{
  std::lock_guard lk(m_asset_lock);
  return m_assets;
}

void EditorAssetSource::PathAdded(std::string_view path)
{
  // TODO: notify our preview that there is a new preview
}

void EditorAssetSource::PathModified(std::string_view path)
{
  std::lock_guard lk(m_asset_lock);
  if (const auto iter = m_path_to_editor_asset.find(path); iter != m_path_to_editor_asset.end())
  {
    auto& system = Core::System::GetInstance();
    auto& loader = system.GetCustomAssetLoader();
    loader.ReloadAsset(iter->second.m_asset_id);
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
  std::lock_guard lk(m_preview_lock);
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
  }

  return nullptr;
}

void EditorAssetSource::SetAssetPreviewData(const AssetID& asset_id,
                                            const VideoCommon::CustomTextureData& preview_data)
{
  std::lock_guard lk(m_preview_lock);
  const auto [it, inserted] = m_asset_id_to_preview.try_emplace(
      asset_id, AssetPreview{.m_preview_data = {}, .m_preview_texture = nullptr});
  it->second.m_preview_data = preview_data;
}

}  // namespace GraphicsModEditor
