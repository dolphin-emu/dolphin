// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <picojson.h>

#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/Assets/RenderTargetAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/WatchableFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"

class AbstractTexture;
namespace VideoCommon
{
class CustomAsset;
class CustomTextureData;
struct TextureAndSamplerData;
}  // namespace VideoCommon

namespace GraphicsModEditor
{
class EditorAssetSource final : public VideoCommon::WatchableFilesystemAssetLibrary
{
public:
  // CustomAssetLibrary interface
  LoadInfo LoadTexture(const AssetID& asset_id, VideoCommon::CustomTextureData* data) override;
  LoadInfo LoadTexture(const AssetID& asset_id, VideoCommon::TextureAndSamplerData* data) override;
  LoadInfo LoadRasterSurfaceShader(const AssetID& asset_id,
                                   VideoCommon::RasterSurfaceShaderData* data) override;
  LoadInfo LoadMaterial(const AssetID& asset_id, VideoCommon::MaterialData* data) override;
  LoadInfo LoadMesh(const AssetID& asset_id, VideoCommon::MeshData* data) override;
  LoadInfo LoadRenderTarget(const AssetID& asset_id, VideoCommon::RenderTargetData* data) override;

  // Editor interface
  EditorAsset* GetAssetFromPath(const std::filesystem::path& asset_path);
  const EditorAsset* GetAssetFromID(const AssetID& asset_id) const;
  EditorAsset* GetAssetFromID(const AssetID& asset_id);

  void AddAsset(const std::filesystem::path& asset_path);
  void AddBuiltinAsset(const std::filesystem::path& asset_path, const AssetID& uuid);
  void RemoveAsset(const std::filesystem::path& asset_path);
  bool RenameAsset(const std::filesystem::path& old_path, const std::filesystem::path& new_path);

  void AddAssets(std::span<const GraphicsModSystem::Config::GraphicsModAsset> assets,
                 const std::filesystem::path& root);
  std::vector<GraphicsModSystem::Config::GraphicsModAsset>
  GetAssets(const std::filesystem::path& root) const;
  void SaveAssetDataAsFiles() const;

  const std::list<EditorAsset>& GetAllAssets() const;

  // Gets a texture associated with the asset preview
  // Will create a new texture if data is available
  AbstractTexture* GetAssetPreview(const AssetID& asset_id);

private:
  void PathAdded(std::string_view path) override;
  void PathModified(std::string_view path) override;
  void PathRenamed(std::string_view old_path, std::string_view new_path) override;
  void PathDeleted(std::string_view path) override;

  void AddAsset(const std::filesystem::path& asset_path, AssetID uuid);
  bool LoadAssetFromFile(const std::filesystem::path& asset_path, const AssetID& uuid,
                         EditorAsset* asset);
  void SetAssetPreviewData(const AssetID& asset_id,
                           const VideoCommon::CustomTextureData& preview_data);

  std::map<AssetID, std::filesystem::path> m_asset_id_to_file_path;
  std::map<std::filesystem::path, std::list<EditorAsset>::iterator> m_path_to_editor_asset;
  std::map<std::filesystem::path, std::list<EditorAsset>::iterator> m_path_to_builtin_editor_asset;
  std::list<EditorAsset> m_builtin_assets;
  std::list<EditorAsset> m_assets;
  mutable std::recursive_mutex m_asset_lock;

  struct AssetPreview
  {
    std::optional<VideoCommon::CustomTextureData> m_preview_data;
    std::unique_ptr<AbstractTexture> m_preview_texture;
  };
  std::map<AssetID, AssetPreview> m_asset_id_to_preview;
  mutable std::mutex m_preview_lock;
};
}  // namespace GraphicsModEditor
