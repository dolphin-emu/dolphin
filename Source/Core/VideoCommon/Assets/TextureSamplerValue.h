// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
// A structure that provides metadata about a texture to a material
struct TextureSamplerValue
{
  CustomAssetLibrary::AssetID asset;

  // Where does the sampler originate from
  // If 'Asset' is used, the sampler is pulled
  // directly from the asset properties
  // If 'TextureHash' is chosen, the sampler is pulled
  // from the game with the cooresponding texture hash
  enum class SamplerOrigin
  {
    Asset,
    TextureHash
  };
  static std::string ToString(SamplerOrigin sampler_origin);
  SamplerOrigin sampler_origin = SamplerOrigin::Asset;
  std::string texture_hash;

  static bool FromJson(const picojson::object& json, TextureSamplerValue* data);
  static void ToJson(picojson::object* obj, const TextureSamplerValue& data);
};
}  // namespace VideoCommon
