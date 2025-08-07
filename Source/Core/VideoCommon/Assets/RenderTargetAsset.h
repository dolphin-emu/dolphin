// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
struct RenderTargetData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       RenderTargetData* data);
  static void ToJson(picojson::object* obj, const RenderTargetData& data);
  AbstractTextureFormat texture_format;
  SamplerState sampler;
  u16 width;
  u16 height;
  AbstractTextureType type = AbstractTextureType::Texture_2D;
};

class RenderTargetAsset final : public CustomLoadableAsset<RenderTargetData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
