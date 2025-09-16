// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Assets/Types.h"
#include "VideoCommon/Assets/WatchableFilesystemAssetLibrary.h"

namespace VideoCommon
{
// This class implements 'CustomAssetLibrary' and loads any assets
// directly from the filesystem
class DirectFilesystemAssetLibrary final : public WatchableFilesystemAssetLibrary
{
public:
  LoadInfo LoadTexture(const AssetID& asset_id, TextureAndSamplerData* data) override;
  LoadInfo LoadTexture(const AssetID& asset_id, CustomTextureData* data) override;
  LoadInfo LoadRasterSurfaceShader(const AssetID& asset_id, RasterSurfaceShaderData* data) override;
  LoadInfo LoadMaterial(const AssetID& asset_id, MaterialData* data) override;
  LoadInfo LoadMesh(const AssetID& asset_id, MeshData* data) override;

  // Assigns the asset id to a map of files, how this map is read is dependent on the data
  // For instance, a raw texture would expect the map to have a single entry and load that
  // file as the asset.  But a model file data might have its data spread across multiple files
  void SetAssetIDMapData(const AssetID& asset_id, Assets::AssetMap asset_path_map);

private:
  void PathModified(std::string_view path) override;

  // Gets the asset map given an asset id
  Assets::AssetMap GetAssetMapForID(const AssetID& asset_id) const;

  mutable std::mutex m_asset_map_lock;
  std::map<AssetID, Assets::AssetMap> m_asset_id_to_asset_map_path;

  mutable std::mutex m_path_map_lock;
  std::map<std::string, AssetID, std::less<>> m_path_to_asset_id;
};
}  // namespace VideoCommon
