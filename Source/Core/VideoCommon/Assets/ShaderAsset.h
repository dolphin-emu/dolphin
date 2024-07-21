// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/TextureSamplerValue.h"
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

  struct Sampler2D
  {
    TextureSamplerValue value;
  };

  struct Sampler2DArray
  {
    TextureSamplerValue value;
  };

  struct SamplerCube
  {
    TextureSamplerValue value;
  };

  using Value = std::variant<s32, std::array<s32, 2>, std::array<s32, 3>, std::array<s32, 4>, float,
                             std::array<float, 2>, std::array<float, 3>, std::array<float, 4>, bool,
                             RGB, RGBA, Sampler2D, Sampler2DArray, SamplerCube>;
  static std::span<const std::string_view> GetValueTypeNames();
  static Value GetDefaultValueFromTypeName(std::string_view name);

  Value m_default;
  std::string m_description;
};

struct ShaderProperty2
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
  static void WriteAsShaderCode(ShaderCode& shader_source, std::string_view name,
                                const ShaderProperty2& property);

  Value m_default;
  std::string m_description;
};

struct PixelShaderData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       PixelShaderData* data);
  static void ToJson(picojson::object& obj, const PixelShaderData& data);

  // These shader properties describe the input that the
  // shader expects to expose.  The key is text
  // expected to be in the shader code and the propery
  // describes various details about the input
  std::map<std::string, ShaderProperty> m_properties;
  std::string m_shader_source;
};

class PixelShaderAsset final : public CustomLoadableAsset<PixelShaderData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};

struct RasterShaderData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       RasterShaderData* data);
  static void ToJson(picojson::object& obj, const RasterShaderData& data);

  // These shader properties describe the input that the
  // shader expects to expose.  The key is text
  // expected to be in the shader code and the propery
  // describes various details about the input
  std::map<std::string, ShaderProperty2> m_vertex_properties;
  std::string m_vertex_source;

  std::map<std::string, ShaderProperty2> m_pixel_properties;
  std::string m_pixel_source;

  struct SamplerData
  {
    AbstractTextureType type;
    std::string name;

    bool operator==(const SamplerData&) const = default;
  };
  std::vector<SamplerData> m_pixel_samplers;
};

class RasterShaderAsset final : public CustomLoadableAsset<RasterShaderData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
