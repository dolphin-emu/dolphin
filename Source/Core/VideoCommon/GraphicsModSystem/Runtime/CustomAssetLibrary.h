// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <string>

namespace VideoCommon
{
class CustomTextureData;

// This class provides functionality to load
// specific data (like textures).  Where this data
// is loaded from is implementation defined
class CustomAssetLibrary
{
public:
  // TODO: this should be std::chrono::system_clock::time_point to
  // support any type of loader where the time isn't from the filesystem
  // but there's no way to convert filesystem times to system times
  // without 'clock_cast', once our builders catch up
  // to support 'clock_cast' we should update this
  // For now, it's fine as a filesystem library is all that is
  // available
  using TimeType = std::filesystem::file_time_type;

  // The AssetID is a unique identifier for a particular asset
  using AssetID = std::string;

  struct LoadInfo
  {
    std::size_t m_bytes_loaded;
    CustomAssetLibrary::TimeType m_load_time;
  };

  // Loads a texture, if there are no levels, bytes loaded will be empty
  virtual LoadInfo LoadTexture(const AssetID& asset_id, CustomTextureData* data) = 0;

  // Gets the last write time for a given asset id
  virtual TimeType GetLastAssetWriteTime(const AssetID& asset_id) const = 0;

  // Loads a texture as a game texture, providing additional checks like confirming
  // each mip level size is correct and that the format is consistent across the data
  LoadInfo LoadGameTexture(const AssetID& asset_id, CustomTextureData* data);
};

// This class implements 'CustomAssetLibrary' and loads any assets
// directly from the filesystem
class DirectFilesystemLibrary final : public CustomAssetLibrary
{
public:
  LoadInfo LoadTexture(const AssetID& asset_id, CustomTextureData* data) override;

  // Gets the latest time from amongst all the files in the asset map
  TimeType GetLastAssetWriteTime(const AssetID& asset_id) const override;

  // Assigns the asset id to a map of files, how this map is read is dependent on the data
  // For instance, a raw texture would expect the map to have a single entry and load that
  // file as the asset.  But a model file data might have its data spread across multiple files
  void SetAssetIDMapData(const AssetID& asset_id,
                         std::map<std::string, std::filesystem::path> asset_path_map);

private:
  // Loads additional mip levels into the texture structure until _mip<N> texture is not found
  bool LoadMips(const std::filesystem::path& asset_path, CustomTextureData* data);
  std::map<AssetID, std::map<std::string, std::filesystem::path>> m_assetid_to_asset_map_path;
};
}  // namespace VideoCommon
