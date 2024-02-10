// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <string>

namespace VideoCommon
{
struct MaterialData;
struct PixelShaderData;
struct TextureData;

// This class provides functionality to load
// specific data (like textures).  Where this data
// is loaded from is implementation defined
class CustomAssetLibrary
{
public:
  using TimeType = std::chrono::system_clock::time_point;

  // The AssetID is a unique identifier for a particular asset
  using AssetID = std::string;

  struct LoadInfo
  {
    std::size_t m_bytes_loaded = 0;
    TimeType m_load_time = {};
  };

  virtual ~CustomAssetLibrary() = default;

  // Loads a texture, if there are no levels, bytes loaded will be empty
  virtual LoadInfo LoadTexture(const AssetID& asset_id, TextureData* data) = 0;

  // Gets the last write time for a given asset id
  virtual TimeType GetLastAssetWriteTime(const AssetID& asset_id) const = 0;

  // Loads a texture as a game texture, providing additional checks like confirming
  // each mip level size is correct and that the format is consistent across the data
  LoadInfo LoadGameTexture(const AssetID& asset_id, TextureData* data);

  // Loads a pixel shader
  virtual LoadInfo LoadPixelShader(const AssetID& asset_id, PixelShaderData* data) = 0;

  // Loads a material
  virtual LoadInfo LoadMaterial(const AssetID& asset_id, MaterialData* data) = 0;
};
}  // namespace VideoCommon
