// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MaterialAssetUtils.h"

#include <variant>

#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"

namespace VideoCommon
{
namespace
{
MaterialProperty GetMaterialPropertyDefault(const ShaderProperty& shader_property)
{
  MaterialProperty result;
  std::visit(
      overloaded{[&](s32 default_value) { result.m_value = default_value; },
                 [&](const std::array<s32, 2>& default_value) { result.m_value = default_value; },
                 [&](const std::array<s32, 3>& default_value) { result.m_value = default_value; },
                 [&](const std::array<s32, 4>& default_value) { result.m_value = default_value; },
                 [&](float default_value) { result.m_value = default_value; },
                 [&](const std::array<float, 2>& default_value) { result.m_value = default_value; },
                 [&](const std::array<float, 3>& default_value) { result.m_value = default_value; },
                 [&](const std::array<float, 4>& default_value) { result.m_value = default_value; },
                 [&](const VideoCommon::ShaderProperty::RGB& default_value) {
                   result.m_value = default_value.value;
                 },
                 [&](const VideoCommon::ShaderProperty::RGBA& default_value) {
                   result.m_value = default_value.value;
                 },
                 [&](bool default_value) { result.m_value = default_value; }},
      shader_property.default_value);

  return result;
}

constexpr bool DoesMaterialPropertyTypeMatch(const MaterialProperty& material_property,
                                             const ShaderProperty& shader_property)
{
  return std::visit(
      [](const auto& a, const auto& b) { return std::is_same_v<decltype(a), decltype(b)>; },
      material_property.m_value, shader_property.default_value);
}
}  // namespace

void SetMaterialPropertiesFromShader(const RasterSurfaceShaderData& shader, MaterialData* material)
{
  material->properties.resize(shader.uniform_properties.size());
  auto iter = shader.uniform_properties.begin();
  for (std::size_t i = 0; i < material->properties.size(); i++)
  {
    MaterialProperty& material_property = material->properties[i];
    const ShaderProperty& shader_property = iter->second;

    if (!DoesMaterialPropertyTypeMatch(material_property, shader_property))
    {
      material_property = GetMaterialPropertyDefault(shader_property);
    }

    iter++;
  }
}

void SetMaterialTexturesFromShader(const RasterSurfaceShaderData& shader, MaterialData* material)
{
  material->textures.resize(shader.samplers.size());
}
}  // namespace VideoCommon
