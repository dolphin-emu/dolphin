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
}  // namespace VideoCommon
