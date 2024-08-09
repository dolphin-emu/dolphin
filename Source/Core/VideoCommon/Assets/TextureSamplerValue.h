// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAssetLibrary.h"

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
  SamplerOrigin sampler_origin;
  std::string texture_hash;

  static bool FromJson(const picojson::object& json, TextureSamplerValue* data);
  static void ToJson(picojson::object* obj, const TextureSamplerValue& data);
};
}  // namespace VideoCommon
