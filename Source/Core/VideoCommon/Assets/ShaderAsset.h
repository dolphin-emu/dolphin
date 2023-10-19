// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>

#include <picojson.h>

#include "Common/EnumFormatter.h"
#include "VideoCommon/Assets/CustomAsset.h"

namespace VideoCommon
{
struct ShaderProperty
{
  // "SamplerShared" denotes that the sampler
  // already exists outside of the shader source
  // (ex: in the Dolphin defined pixel shader)
  // "Main" is the first entry in a shared sampler array
  // and "Additional" denotes a subsequent entry
  // in the array
  enum class Type
  {
    Type_Undefined,
    Type_SamplerArrayShared_Main,
    Type_SamplerArrayShared_Additional,
    Type_Sampler2D,
    Type_SamplerCube,
    Type_Int,
    Type_Int2,
    Type_Int3,
    Type_Int4,
    Type_Float,
    Type_Float2,
    Type_Float3,
    Type_Float4,
    Type_RGB,
    Type_RGBA,
    Type_Bool,
    Type_Max = Type_Bool
  };
  Type m_type;
  std::string m_description;
};
struct PixelShaderData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       PixelShaderData* data);

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
}  // namespace VideoCommon

template <>
struct fmt::formatter<VideoCommon::ShaderProperty::Type>
    : EnumFormatter<VideoCommon::ShaderProperty::Type::Type_Max>
{
  constexpr formatter()
      : EnumFormatter({"Undefined", "DolphinSamplerArray_Main", "DolphinSamplerArray_Additional",
                       "2DSampler", "CubeSampler", "Int", "Int2", "Int3", "Int4", "Float", "Float2",
                       "Float3", "Float4", "RGB", "RGBA", "Bool"})
  {
  }
};
