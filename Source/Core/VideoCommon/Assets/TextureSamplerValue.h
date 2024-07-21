// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace VideoCommon
{
struct TextureSamplerValue
{
  CustomAssetLibrary::AssetID asset;

  enum class SamplerOrigin
  {
    Asset,
    TextureHash
  };
  static std::string ToString(SamplerOrigin sampler_origin);
  SamplerOrigin sampler_origin = SamplerOrigin::Asset;
  std::string texture_hash;

  bool is_render_target = false;
  GraphicsModSystem::CameraType camera_type;
  GraphicsModSystem::DrawCallID camera_originating_draw_call;

  static bool FromJson(const picojson::object& json, TextureSamplerValue* data);
  static void ToJson(picojson::object* obj, const TextureSamplerValue& data);
};
}  // namespace VideoCommon
