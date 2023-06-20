// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomTextureData.h"

#include <picojson.h>

#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
class RawTextureAsset final : public CustomLoadableAsset<CustomTextureData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};

class GameTextureAsset final : public CustomLoadableAsset<CustomTextureData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

  // Validates that the game texture matches the native dimensions provided
  // Callees are expected to call this once the data is loaded
  bool Validate(u32 native_width, u32 native_height) const;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};

struct TextureAndSamplerData
{
  enum class Type
  {
    Type_Undefined,
    Type_2D,
    Type_Max = Type_2D
  };
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       TextureAndSamplerData* data);
  Type m_type = Type::Type_Undefined;
  CustomTextureData m_data;
  SamplerState m_sampler;
};

class TextureWithMetadataAsset final : public CustomLoadableAsset<TextureAndSamplerData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
