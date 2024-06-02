// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MaterialAssetUtils.h"

#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"

namespace VideoCommon
{
void SetMaterialPropertiesFromShader(const VideoCommon::PixelShaderData& shader,
                                     VideoCommon::MaterialData* material)
{
  material->properties.clear();
  for (const auto& entry : shader.m_properties)
  {
    // C++20: error with capturing structured bindings for our version of clang
    const auto& name = entry.first;
    const auto& shader_property = entry.second;
    std::visit(overloaded{[&](const VideoCommon::ShaderProperty::Sampler2D& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value.value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const VideoCommon::ShaderProperty::Sampler2DArray& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value.value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const VideoCommon::ShaderProperty::SamplerCube& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value.value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](s32 default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<s32, 2>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<s32, 3>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<s32, 4>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](float default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<float, 2>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<float, 3>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const std::array<float, 4>& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const VideoCommon::ShaderProperty::RGB& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value.value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](const VideoCommon::ShaderProperty::RGBA& default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value.value;
                            material->properties.push_back(std::move(property));
                          },
                          [&](bool default_value) {
                            VideoCommon::MaterialProperty property;
                            property.m_code_name = name;
                            property.m_value = default_value;
                            material->properties.push_back(std::move(property));
                          }},
               shader_property.m_default);
  }
}

void SetMaterialPropertiesFromShader(const RasterShaderData& shader, RasterMaterialData* material)
{
  material->vertex_properties.clear();
  material->pixel_properties.clear();

  const auto set_properties = [&](const std::map<std::string, ShaderProperty2>& shader_properties,
                                  std::vector<MaterialProperty2>* material_properties) {
    for (const auto& entry : shader_properties)
    {
      // C++20: error with capturing structured bindings for our version of clang
      // const auto& name = entry.first;
      const auto& shader_property = entry.second;
      std::visit(overloaded{[&](s32 default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<s32, 2>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<s32, 3>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<s32, 4>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](float default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<float, 2>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<float, 3>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const std::array<float, 4>& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const VideoCommon::ShaderProperty2::RGB& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value.value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](const VideoCommon::ShaderProperty2::RGBA& default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value.value;
                              material_properties->push_back(std::move(property));
                            },
                            [&](bool default_value) {
                              VideoCommon::MaterialProperty2 property;
                              property.m_value = default_value;
                              material_properties->push_back(std::move(property));
                            }},
                 shader_property.m_default);
    }
  };

  set_properties(shader.m_vertex_properties, &material->vertex_properties);
  set_properties(shader.m_pixel_properties, &material->pixel_properties);
}

void SetMaterialTexturesFromShader(const RasterShaderData& shader, RasterMaterialData* material)
{
  material->pixel_textures.clear();

  const auto set_textures = [&](const std::vector<RasterShaderData::SamplerData>& shader_samplers,
                                std::vector<TextureSamplerValue>* material_textures) {
    for (std::size_t i = 0; i < shader_samplers.size(); i++)
    {
      TextureSamplerValue sampler_value;
      material_textures->push_back(std::move(sampler_value));
    }
  };

  set_textures(shader.m_pixel_samplers, &material->pixel_textures);
}
}  // namespace VideoCommon
