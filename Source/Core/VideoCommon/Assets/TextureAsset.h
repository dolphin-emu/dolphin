// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <nlohmann/json_fwd.hpp>

#include "Common/EnumFormatter.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
struct TextureAndSamplerData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const nlohmann::json& json,
                       TextureAndSamplerData* data);
  static void ToJson(nlohmann::json* obj, const TextureAndSamplerData& data);
  AbstractTextureType type;
  CustomTextureData texture_data;
  SamplerState sampler;
};

class TextureAsset final : public CustomLoadableAsset<CustomTextureData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};

class TextureAndSamplerAsset final : public CustomLoadableAsset<TextureAndSamplerData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
