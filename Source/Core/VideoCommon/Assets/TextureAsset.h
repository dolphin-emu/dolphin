// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <picojson.h>

#include "Common/EnumFormatter.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
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
  CustomTextureData m_texture;
  SamplerState m_sampler;
};

class GameTextureAsset final : public CustomLoadableAsset<TextureData>
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

template <>
struct fmt::formatter<VideoCommon::TextureData::Type>
    : EnumFormatter<VideoCommon::TextureData::Type::Type_Max>
{
  constexpr formatter() : EnumFormatter({"Undefined", "Texture2D", "TextureCube"}) {}
};
