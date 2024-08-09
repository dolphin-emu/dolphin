// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MaterialControl.h"

#include <string>
#include <variant>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MaterialAssetUtils.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
MaterialControl::MaterialControl(EditorState& state) : m_state(state)
{
}

void MaterialControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                VideoCommon::MaterialData* material,
                                VideoCommon::CustomAssetLibrary::TimeType* last_data_write,
                                bool* valid)
{
  if (ImGui::BeginTable("MaterialShaderForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", asset_id.c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Shader");
    ImGui::TableNextColumn();

    if (AssetDisplay("MaterialShaderAsset", &m_state, &material->shader_asset,
                     AssetDataType::PixelShader))
    {
      auto asset = m_state.m_user_data.m_asset_library->GetAssetFromID(material->shader_asset);
      if (!asset)
      {
        ImGui::Text("Please choose a shader for this material");
        material->shader_asset = "";
        *valid = false;
        return;
      }
      else
      {
        auto shader = std::get_if<std::unique_ptr<VideoCommon::PixelShaderData>>(&asset->m_data);
        if (!shader)
        {
          ImGui::Text(
              "%s",
              fmt::format("Asset id '{}' was not type shader!", material->shader_asset).c_str());
          material->shader_asset = "";
          *valid = false;
          return;
        }
        SetMaterialPropertiesFromShader(*shader->get(), material);
        *last_data_write = std::chrono::system_clock::now();
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
        *valid = true;
      }
    }
    ImGui::EndTable();
  }

  if (!*valid)
    return;

  // Look up shader
  auto asset = m_state.m_user_data.m_asset_library->GetAssetFromID(material->shader_asset);
  if (!asset)
  {
    ImGui::Text("Please choose a shader for this material");
  }
  else
  {
    auto shader = std::get_if<std::unique_ptr<VideoCommon::PixelShaderData>>(&asset->m_data);
    if (!shader)
    {
      ImGui::Text(
          "%s", fmt::format("Asset id '{}' was not type shader!", material->shader_asset).c_str());
    }
    else
    {
      VideoCommon::PixelShaderData* shader_data = shader->get();
      if (!asset->m_valid)
      {
        ImGui::Text("%s",
                    fmt::format("The shader '{}' is invalid!", material->shader_asset).c_str());
      }
      else
      {
        DrawControl(asset_id, shader_data, material, last_data_write);
      }
    }
  }
}

void MaterialControl::DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                  VideoCommon::PixelShaderData* shader,
                                  VideoCommon::MaterialData* material,
                                  VideoCommon::CustomAssetLibrary::TimeType* last_data_write)
{
  if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("MaterialPropertiesForm", 2))
    {
      std::size_t material_property_index = 0;
      for (const auto& entry : shader->m_properties)
      {
        // C++20: error with capturing structured bindings for our version of clang
        const auto& name = entry.first;
        const auto& shader_property = entry.second;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", name.c_str());
        ImGui::TableNextColumn();

        auto& material_property = material->properties[material_property_index];

        std::visit(
            overloaded{
                [&](VideoCommon::TextureSamplerValue& value) {
                  if (AssetDisplay(material_property.m_code_name, &m_state, &value.asset,
                                   AssetDataType::Texture))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }

                  const auto sampler_origin_str =
                      VideoCommon::TextureSamplerValue::ToString(value.sampler_origin);
                  if (ImGui::BeginCombo(
                          fmt::format("##{}SamplerOrigin", material_property.m_code_name).c_str(),
                          sampler_origin_str.c_str()))
                  {
                    static std::array<VideoCommon::TextureSamplerValue::SamplerOrigin, 2>
                        all_sampler_types = {
                            VideoCommon::TextureSamplerValue::SamplerOrigin::Asset,
                            VideoCommon::TextureSamplerValue::SamplerOrigin::TextureHash};

                    for (const auto& type : all_sampler_types)
                    {
                      const bool is_selected = type == value.sampler_origin;
                      const auto type_name = VideoCommon::TextureSamplerValue::ToString(type);
                      if (ImGui::Selectable(
                              fmt::format("{}##{}", type_name, material_property.m_code_name)
                                  .c_str(),
                              is_selected))
                      {
                        value.sampler_origin = type;
                        *last_data_write = std::chrono::system_clock::now();
                        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                      }
                    }
                    ImGui::EndCombo();
                  }

                  if (value.sampler_origin ==
                      VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
                  {
                    ImGui::BeginDisabled();
                  }
                  if (ImGui::InputText(
                          fmt::format("##{}TextureHash", material_property.m_code_name).c_str(),
                          &value.texture_hash))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                  if (value.sampler_origin ==
                      VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
                  {
                    ImGui::EndDisabled();
                  }
                },
                [&](s32& value) {
                  if (ImGui::InputInt(fmt::format("##{}", material_property.m_code_name).c_str(),
                                      &value))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 2>& value) {
                  if (ImGui::InputInt2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 3>& value) {
                  if (ImGui::InputInt3(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 4>& value) {
                  if (ImGui::InputInt4(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](float& value) {
                  if (ImGui::InputFloat(fmt::format("##{}", material_property.m_code_name).c_str(),
                                        &value))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<float, 2>& value) {
                  if (ImGui::InputFloat2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                         value.data()))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<float, 3>& value) {
                  if (std::holds_alternative<VideoCommon::ShaderProperty::RGB>(
                          shader_property.m_default))
                  {
                    if (ImGui::ColorEdit3(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = std::chrono::system_clock::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                  else
                  {
                    if (ImGui::InputFloat3(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = std::chrono::system_clock::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                },
                [&](std::array<float, 4>& value) {
                  if (std::holds_alternative<VideoCommon::ShaderProperty::RGBA>(
                          shader_property.m_default))
                  {
                    if (ImGui::ColorEdit4(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = std::chrono::system_clock::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                  else
                  {
                    if (ImGui::InputFloat4(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = std::chrono::system_clock::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                },
                [&](bool& value) {
                  if (ImGui::Checkbox(fmt::format("##{}", material_property.m_code_name).c_str(),
                                      &value))
                  {
                    *last_data_write = std::chrono::system_clock::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                }},
            material_property.m_value);
        material_property_index++;
      }
      ImGui::EndTable();
    }
  }
}
}  // namespace GraphicsModEditor::Controls
