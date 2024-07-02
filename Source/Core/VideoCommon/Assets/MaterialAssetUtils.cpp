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
}  // namespace VideoCommon
