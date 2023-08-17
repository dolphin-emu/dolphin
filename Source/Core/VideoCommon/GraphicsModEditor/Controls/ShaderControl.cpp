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
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
ShaderControl::ShaderControl(EditorState& state) : m_state(state)
{
}

void ShaderControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                              VideoCommon::PixelShaderData* shader,
                              VideoCommon::CustomAssetLibrary::TimeType* last_data_write)
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

  if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
  {
    for (auto& entry : shader->m_properties)
    {
      // C++20: error with capturing structured bindings for our version of clang
      auto& name = entry.first;
      auto& property = entry.second;

      ImGui::Text("%s", name.c_str());
      ImGui::SameLine();
      // TODO Button for editing description
      ImGui::SameLine();
      std::visit(
          overloaded{
              [&](VideoCommon::ShaderProperty::Sampler2D& default_value) {
                if (AssetDisplay(name, &m_state, &default_value.value.asset,
                                 AssetDataType::Texture))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](VideoCommon::ShaderProperty::Sampler2DArray& default_value) {
                if (AssetDisplay(name, &m_state, &default_value.value.asset,
                                 AssetDataType::Texture))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](VideoCommon::ShaderProperty::SamplerCube& default_value) {
                if (AssetDisplay(name, &m_state, &default_value.value.asset,
                                 AssetDataType::Texture))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](s32& default_value) {
                if (ImGui::InputInt(fmt::format("##{}", name).c_str(), &default_value))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<s32, 2>& default_value) {
                if (ImGui::InputInt2(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<s32, 3>& default_value) {
                if (ImGui::InputInt3(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<s32, 4>& default_value) {
                if (ImGui::InputInt4(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](float& default_value) {
                if (ImGui::InputFloat(fmt::format("##{}", name).c_str(), &default_value))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<float, 2>& default_value) {
                if (ImGui::InputFloat2(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<float, 3>& default_value) {
                if (ImGui::InputFloat3(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](std::array<float, 4>& default_value) {
                if (ImGui::InputFloat4(fmt::format("##{}", name).c_str(), default_value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](VideoCommon::ShaderProperty::RGB& default_value) {
                if (ImGui::ColorEdit3(fmt::format("##{}", name).c_str(),
                                      default_value.value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](VideoCommon::ShaderProperty::RGBA& default_value) {
                if (ImGui::ColorEdit4(fmt::format("##{}", name).c_str(),
                                      default_value.value.data()))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              },
              [&](bool& default_value) {
                if (ImGui::Checkbox(fmt::format("##{}", name).c_str(), &default_value))
                {
                  *last_data_write = std::chrono::system_clock::now();
                  GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                }
              }},
          property.m_default);
      ImGui::SameLine();
      if (ImGui::Button("X"))
      {
        shader->m_properties.erase(name);
      }
    }

    ImGui::Separator();

    if (ImGui::Button("Add"))
    {
      m_add_property_name = "";
      m_add_property_chosen_type = "";
      m_add_property_data = {};
      ImGui::OpenPopup("AddShaderPropPopup");
    }

    if (ImGui::BeginPopupModal("AddShaderPropPopup", nullptr))
    {
      if (ImGui::BeginTable("AddShaderPropForm", 2))
      {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Name");
        ImGui::TableNextColumn();
        ImGui::InputText("##PropName", &m_add_property_name);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Type");
        ImGui::TableNextColumn();

        if (ImGui::BeginCombo("##PropType", m_add_property_chosen_type.c_str()))
        {
          for (const auto& type : VideoCommon::ShaderProperty::GetValueTypeNames())
          {
            const bool is_selected = type == m_add_property_chosen_type;
            if (ImGui::Selectable(type.data(), is_selected))
            {
              m_add_property_chosen_type = type;
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
        shader_property.m_description = "";
        shader_property.m_default = m_add_property_data;
        shader->m_properties.insert_or_assign(m_add_property_name, std::move(shader_property));
        *last_data_write = std::chrono::system_clock::now();
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
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
}

void ShaderControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                              VideoCommon::RasterShaderData* shader)
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
      [&](std::map<std::string, VideoCommon::ShaderProperty2>& properties) {
        std::vector<std::string> properties_to_erase;
        for (auto& entry : properties)
        {
          // C++20: error with capturing structured bindings for our version of clang
          auto& name = entry.first;
          auto& property = entry.second;

          ImGui::Text("%s", name.c_str());
          ImGui::SameLine();
          // TODO Button for editing description
          ImGui::SameLine();
          std::visit(
              overloaded{
                  [&](s32& default_value) {
                    if (ImGui::InputInt(fmt::format("##{}", name).c_str(), &default_value))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 2>& default_value) {
                    if (ImGui::InputInt2(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 3>& default_value) {
                    if (ImGui::InputInt3(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<s32, 4>& default_value) {
                    if (ImGui::InputInt4(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](float& default_value) {
                    if (ImGui::InputFloat(fmt::format("##{}", name).c_str(), &default_value))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 2>& default_value) {
                    if (ImGui::InputFloat2(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 3>& default_value) {
                    if (ImGui::InputFloat3(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](std::array<float, 4>& default_value) {
                    if (ImGui::InputFloat4(fmt::format("##{}", name).c_str(), default_value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](VideoCommon::ShaderProperty2::RGB& default_value) {
                    if (ImGui::ColorEdit3(fmt::format("##{}", name).c_str(),
                                          default_value.value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](VideoCommon::ShaderProperty2::RGBA& default_value) {
                    if (ImGui::ColorEdit4(fmt::format("##{}", name).c_str(),
                                          default_value.value.data()))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  },
                  [&](bool& default_value) {
                    if (ImGui::Checkbox(fmt::format("##{}", name).c_str(), &default_value))
                    {
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }},
              property.m_default);
          ImGui::SameLine();
          if (ImGui::Button("X"))
          {
            properties_to_erase.push_back(name);
          }
        }

        for (const auto& name : properties_to_erase)
        {
          properties.erase(name);
        }
      };

  if (ImGui::CollapsingHeader("Vertex Properties", ImGuiTreeNodeFlags_DefaultOpen))
  {
    parse_properties(shader->m_vertex_properties);
    ImGui::Separator();
    AddShaderProperty(shader, &shader->m_vertex_properties, "vertex");
  }

  if (ImGui::CollapsingHeader("Pixel Properties", ImGuiTreeNodeFlags_DefaultOpen))
  {
    parse_properties(shader->m_pixel_properties);
    ImGui::Separator();
    AddShaderProperty(shader, &shader->m_pixel_properties, "pixel");
  }

  if (ImGui::CollapsingHeader("Pixel Textures", ImGuiTreeNodeFlags_DefaultOpen))
  {
    std::vector<VideoCommon::RasterShaderData::SamplerData*> sampler_to_erase;
    for (auto& sampler : shader->m_pixel_samplers)
    {
      ImGui::Text("%s", sampler.name.c_str());
      ImGui::SameLine();

      if (ImGui::BeginCombo("##SamplerTextureType", fmt::to_string(sampler.type).c_str()))
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
            GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::SameLine();
      if (ImGui::Button("X"))
      {
        sampler_to_erase.push_back(&sampler);
      }
    }

    for (const auto sampler : sampler_to_erase)
    {
      std::erase(shader->m_pixel_samplers, *sampler);
    }

    if (ImGui::Button("Add"))
    {
      VideoCommon::RasterShaderData::SamplerData sampler_data;
      sampler_data.name = "";
      sampler_data.type = AbstractTextureType::Texture_2D;
      shader->m_pixel_samplers.push_back(std::move(sampler_data));
      GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    }
  }
}

void ShaderControl::AddShaderProperty(
    VideoCommon::RasterShaderData* shader,
    std::map<std::string, VideoCommon::ShaderProperty2>* properties, std::string_view type)
{
  if (ImGui::Button(fmt::format("Add {} property", type).c_str()))
  {
    m_add_property_name = "";
    m_add_property_chosen_type = "";
    m_add_property2_data = {};
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
        for (const auto& type_name : VideoCommon::ShaderProperty2::GetValueTypeNames())
        {
          const bool is_selected = type_name == m_add_property_chosen_type;
          if (ImGui::Selectable(type.data(), is_selected))
          {
            m_add_property_chosen_type = type_name;
            m_add_property2_data = VideoCommon::ShaderProperty2::GetDefaultValueFromTypeName(type);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::EndTable();
    }
    if (ImGui::Button("Add"))
    {
      VideoCommon::ShaderProperty2 shader_property;
      shader_property.m_description = "";
      shader_property.m_default = m_add_property2_data;
      shader->m_pixel_properties.insert_or_assign(m_add_property_name, std::move(shader_property));
      GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
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
