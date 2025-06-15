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
struct TextureAndSamplerData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       TextureAndSamplerData* data);
  static void ToJson(picojson::object* obj, const TextureAndSamplerData& data);
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

class TextureAsset final : public CustomLoadableAsset<CustomTextureData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon

template <>
struct fmt::formatter<VideoCommon::TextureAndSamplerData::Type>
    : EnumFormatter<VideoCommon::TextureAndSamplerData::Type::Type_Max>
{
  constexpr formatter() : EnumFormatter({"Undefined", "Texture2D", "TextureCube"}) {}
};
