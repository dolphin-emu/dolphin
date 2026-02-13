// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
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

  enum class RenderType
  {
    ScreenColor,
    ScreenDepth
  };
  RenderType render_type = RenderType::ScreenColor;

  using Color = std::array<float, 3>;
  Color clear_color;
  AbstractTextureFormat texture_format = AbstractTextureFormat::RGBA8;
  AbstractTextureType texture_type = AbstractTextureType::Texture_2D;
  SamplerState sampler;
  u16 width = 0;
  u16 height = 0;
};

class RenderTargetAsset final : public CustomLoadableAsset<RenderTargetData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon

template <>
struct fmt::formatter<VideoCommon::RenderTargetData::RenderType>
    : EnumFormatter<VideoCommon::RenderTargetData::RenderType::ScreenDepth>
{
  constexpr formatter() : EnumFormatter({"Screen Color", "Screen Depth"}) {}
};
