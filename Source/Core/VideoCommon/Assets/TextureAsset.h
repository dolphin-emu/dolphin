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
