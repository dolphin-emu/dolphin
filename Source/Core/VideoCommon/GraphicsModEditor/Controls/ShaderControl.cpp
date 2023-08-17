// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/ShaderControl.h"

#include <chrono>
#include <map>
#include <string>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
ShaderControl::ShaderControl(EditorState& state) : m_state(state)
{
}

void ShaderControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                              VideoCommon::RasterSurfaceShaderData* shader)
{
  if (ImGui::BeginTable("ShaderForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", asset_id.c_str());

    ImGui::EndTable();
  }

  const auto parse_properties =
      [&](decltype(VideoCommon::RasterSurfaceShaderData::uniform_properties)* shader_properties,
          decltype(VideoCommon::RasterSurfaceShaderData::name_to_property)* name_to_property) {
        std::vector<std::pair<int, std::string>> property_name_and_index_to_erase;
        for (int i = 0; i < shader_properties->size(); i++)
        {
          auto& property = (*shader_properties)[i];
          ImGui::Text("%s", property.name.c_str());
          ImGui::SameLine();
          // TODO Button for editing description
          ImGui::SameLine();
          std::visit(
              overloaded{
                  [&](s32& default_value) {
                    if (ImGui::InputInt(fmt::format("##{}", property.name).c_str(), &default_value))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 2>& default_value) {
                    if (ImGui::InputInt2(fmt::format("##{}", property.name).c_str(),
                                         default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 3>& default_value) {
                    if (ImGui::InputInt3(fmt::format("##{}", property.name).c_str(),
                                         default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 4>& default_value) {
                    if (ImGui::InputInt4(fmt::format("##{}", property.name).c_str(),
                                         default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](float& default_value) {
                    if (ImGui::InputFloat(fmt::format("##{}", property.name).c_str(),
                                          &default_value))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 2>& default_value) {
                    if (ImGui::InputFloat2(fmt::format("##{}", property.name).c_str(),
                                           default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 3>& default_value) {
                    if (ImGui::InputFloat3(fmt::format("##{}", property.name).c_str(),
                                           default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 4>& default_value) {
                    if (ImGui::InputFloat4(fmt::format("##{}", property.name).c_str(),
                                           default_value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](VideoCommon::ShaderProperty::RGB& default_value) {
                    if (ImGui::ColorEdit3(fmt::format("##{}", property.name).c_str(),
                                          default_value.value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](VideoCommon::ShaderProperty::RGBA& default_value) {
                    if (ImGui::ColorEdit4(fmt::format("##{}", property.name).c_str(),
                                          default_value.value.data()))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  },
                  [&](bool& default_value) {
                    if (ImGui::Checkbox(fmt::format("##{}", property.name).c_str(), &default_value))
                    {
                      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
                    }
                  }},
              property.default_value);
          ImGui::SameLine();
          if (ImGui::Button(fmt::format("X##{}", property.name).c_str()))
          {
            property_name_and_index_to_erase.emplace_back(i, property.name);
          }
        }

        for (const auto& [index, name] : property_name_and_index_to_erase)
        {
          shader_properties->erase(shader_properties->begin() + index);
          name_to_property->erase(name);
        }
      };

  if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
  {
    parse_properties(&shader->uniform_properties, &shader->name_to_property);
    ImGui::Separator();
    AddShaderProperty(shader, "pixel");
  }

  if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::PushID("Sampler");
    std::vector<VideoCommon::RasterSurfaceShaderData::SamplerData*> sampler_to_erase;
    for (std::size_t i = 0; i < shader->samplers.size(); i++)
    {
      ImGui::PushID(static_cast<int>(i));
      auto& sampler = shader->samplers[i];
      ImGui::InputText("Name", &sampler.name, ImGuiInputTextFlags_CharsNoBlank);
      ImGui::SameLine();

      if (ImGui::BeginCombo("##TextureType", fmt::to_string(sampler.type).c_str()))
      {
        static constexpr std::array<AbstractTextureType, 3> all_texture_types = {
            AbstractTextureType::Texture_2D, AbstractTextureType::Texture_2DArray,
            AbstractTextureType::Texture_CubeMap};

        for (const auto& texture_type : all_texture_types)
        {
          const bool is_selected = texture_type == sampler.type;
          const auto texture_type_str = fmt::to_string(texture_type);
          if (ImGui::Selectable(fmt::format("{}##", texture_type_str).c_str(), is_selected))
          {
            sampler.type = texture_type;
            GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::SameLine();
      if (ImGui::Button("X"))
      {
        sampler_to_erase.push_back(&sampler);
      }
      ImGui::PopID();
    }

    for (const auto sampler : sampler_to_erase)
    {
      std::erase(shader->samplers, *sampler);
    }

    if (ImGui::Button("Add"))
    {
      VideoCommon::RasterSurfaceShaderData::SamplerData sampler_data;
      sampler_data.name = "";
      sampler_data.type = AbstractTextureType::Texture_2D;
      shader->samplers.push_back(std::move(sampler_data));
      GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
    }

    ImGui::PopID();
  }
}

void ShaderControl::AddShaderProperty(VideoCommon::RasterSurfaceShaderData* shader,
                                      std::string_view type)
{
  if (ImGui::Button(fmt::format("Add {} property", type).c_str()))
  {
    m_add_property_name = "";
    m_add_property_chosen_type = "";
    ImGui::OpenPopup(fmt::format("AddShader{}PropPopup", type).c_str());
  }

  if (ImGui::BeginPopupModal(fmt::format("AddShader{}PropPopup", type).c_str(), nullptr))
  {
    if (ImGui::BeginTable(fmt::format("AddShader{}PropForm", type).c_str(), 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Name");
      ImGui::TableNextColumn();
      ImGui::InputText(fmt::format("##PropName{}", type).c_str(), &m_add_property_name);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Type");
      ImGui::TableNextColumn();

      if (ImGui::BeginCombo(fmt::format("##PropType{}", type).c_str(),
                            m_add_property_chosen_type.c_str()))
      {
        for (const auto& type_name : VideoCommon::ShaderProperty::GetValueTypeNames())
        {
          const bool is_selected = type_name == m_add_property_chosen_type;
          if (ImGui::Selectable(type.data(), is_selected))
          {
            m_add_property_chosen_type = type_name;
            m_add_property_data = VideoCommon::ShaderProperty::GetDefaultValueFromTypeName(type);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::EndTable();
    }
    if (ImGui::Button("Add"))
    {
      VideoCommon::ShaderProperty shader_property;
      shader_property.description = "";
      shader_property.default_value = m_add_property_data;
      shader_property.name = m_add_property_name;
      shader->uniform_properties.push_back(std::move(shader_property));
      shader->name_to_property.insert_or_assign(shader_property.name,
                                                &shader->uniform_properties.back());
      GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
}  // namespace GraphicsModEditor::Controls
