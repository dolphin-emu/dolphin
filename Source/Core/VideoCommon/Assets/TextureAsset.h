// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomTextureData.h"
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

struct TextureData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       TextureData* data);
  enum class Type
  {
    Type_Undefined,
    Type_Texture2D,
    Type_TextureCube,
    Type_Max = Type_TextureCube
  };
  Type m_type;
  CustomTextureData m_data;
  SamplerState m_sampler;
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
}  // namespace VideoCommon
