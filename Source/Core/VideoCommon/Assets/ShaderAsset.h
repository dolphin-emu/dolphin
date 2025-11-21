// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/TextureConfig.h"

class ShaderCode;

namespace VideoCommon
{
struct ShaderProperty
{
  struct RGB
  {
    std::array<float, 3> value;
  };

  struct RGBA
  {
    std::array<float, 4> value;
  };

  using Value = std::variant<s32, std::array<s32, 2>, std::array<s32, 3>, std::array<s32, 4>, float,
                             std::array<float, 2>, std::array<float, 3>, std::array<float, 4>, bool,
                             RGB, RGBA>;
  static std::span<const std::string_view> GetValueTypeNames();
  static Value GetDefaultValueFromTypeName(std::string_view name);
  static void WriteAsShaderCode(ShaderCode& shader_source, const ShaderProperty& property);

  Value default_value;
  std::string name;
  std::string description;
};

struct RasterSurfaceShaderData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       RasterSurfaceShaderData* data);
  static void ToJson(picojson::object& obj, const RasterSurfaceShaderData& data);

  // These shader properties describe the input that the
  // shader expects to expose.  The key is text
  // expected to be in the shader code and the propery
  // describes various details about the input
  std::vector<ShaderProperty> uniform_properties;
  std::string vertex_source;
  std::string pixel_source;

  struct SamplerData
  {
    AbstractTextureType type;
    std::string name;

    bool operator==(const SamplerData&) const = default;
  };
  std::vector<SamplerData> samplers;
};

class RasterSurfaceShaderAsset final : public CustomLoadableAsset<RasterSurfaceShaderData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
