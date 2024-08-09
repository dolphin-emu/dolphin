// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MaterialGenerateWindow.h"

#include <array>
#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/MaterialAssetUtils.h"
#include "VideoCommon/Assets/TextureSamplerValue.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
namespace
{
void DrawMaterialGenerationContext(MaterialGenerationContext* context,
                                   VideoCommon::PixelShaderData* shader_data)
{
  if (ImGui::BeginTable("MaterialPropertiesForm", 2))
  {
    std::size_t material_property_index = 0;
    for (const auto& entry : shader_data->m_properties)
    {
      // C++20: error with capturing structured bindings for our version of clang
      const auto& name = entry.first;
      const auto& shader_property = entry.second;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", name.c_str());
      ImGui::TableNextColumn();

      auto& material_property = context->material_template_data.properties[material_property_index];

      std::visit(
          overloaded{
              [&](VideoCommon::TextureSamplerValue& value) {
                auto [iter, inserted] =
                    context->material_property_index_to_texture_filter.try_emplace(
                        material_property_index, "{IMAGE_1}");
                ImGui::InputText(fmt::format("##{}", material_property.m_code_name).c_str(),
                                 &iter->second);

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
                            fmt::format("{}##{}", type_name, material_property.m_code_name).c_str(),
                            is_selected))
                    {
                      value.sampler_origin = type;
                    }
                  }
                  ImGui::EndCombo();
                }
              },
              [&](s32& value) {
                ImGui::InputInt(fmt::format("##{}", material_property.m_code_name).c_str(), &value);
              },
              [&](std::array<s32, 2>& value) {
                ImGui::InputInt2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                 value.data());
              },
              [&](std::array<s32, 3>& value) {
                ImGui::InputInt3(fmt::format("##{}", material_property.m_code_name).c_str(),
                                 value.data());
              },
              [&](std::array<s32, 4>& value) {
                ImGui::InputInt4(fmt::format("##{}", material_property.m_code_name).c_str(),
                                 value.data());
              },
              [&](float& value) {
                ImGui::InputFloat(fmt::format("##{}", material_property.m_code_name).c_str(),
                                  &value);
              },
              [&](std::array<float, 2>& value) {
                ImGui::InputFloat2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                   value.data());
              },
              [&](std::array<float, 3>& value) {
                if (std::holds_alternative<VideoCommon::ShaderProperty::RGB>(
                        shader_property.m_default))
                {
                  ImGui::ColorEdit3(fmt::format("##{}", material_property.m_code_name).c_str(),
                                    value.data());
                }
                else
                {
                  ImGui::InputFloat3(fmt::format("##{}", material_property.m_code_name).c_str(),
                                     value.data());
                }
              },
              [&](std::array<float, 4>& value) {
                if (std::holds_alternative<VideoCommon::ShaderProperty::RGBA>(
                        shader_property.m_default))
                {
                  ImGui::ColorEdit4(fmt::format("##{}", material_property.m_code_name).c_str(),
                                    value.data());
                }
                else
                {
                  ImGui::InputFloat4(fmt::format("##{}", material_property.m_code_name).c_str(),
                                     value.data());
                }
              },
              [&](bool& value) {
                ImGui::Checkbox(fmt::format("##{}", material_property.m_code_name).c_str(), &value);
              }},
          material_property.m_value);
      material_property_index++;
    }
    ImGui::EndTable();
  }
}
}  // namespace
bool ShowMaterialGenerateWindow(MaterialGenerationContext* context)
{
  bool valid = true;
  bool result = false;
  const std::string_view material_generate_popup = "Material Generate";
  if (!ImGui::IsPopupOpen(material_generate_popup.data()))
  {
    ImGui::OpenPopup(material_generate_popup.data());
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(material_generate_popup.data(), nullptr))
  {
    bool shader_changed = false;
    if (ImGui::BeginTable("MaterialForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Shader");
      ImGui::TableNextColumn();

      if (AssetDisplay("MaterialShaderAsset", context->state,
                       &context->material_template_data.shader_asset, AssetDataType::PixelShader))
      {
        shader_changed = true;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Texture Folder");
      ImGui::TableNextColumn();

      ImGui::InputText("##TextureFolderPath", &context->input_path);
      if (!File::Exists(context->input_path))
      {
        valid = false;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Output Folder");
      ImGui::TableNextColumn();

      ImGui::InputText("##OutputFolderPath", &context->output_path);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Lookup?");
      ImGui::TableNextColumn();

      ImGui::InputText("##LookupPath", &context->lookup_path);
      if (!File::Exists(context->lookup_path))
      {
        valid = false;
      }

      ImGui::EndTable();
    }

    auto asset = context->state->m_user_data.m_asset_library->GetAssetFromID(
        context->material_template_data.shader_asset);
    if (asset)
    {
      auto shader = std::get_if<std::unique_ptr<VideoCommon::PixelShaderData>>(&asset->m_data);
      if (!shader)
      {
        ImGui::Text("Asset id '%s' was not type shader!",
                    context->material_template_data.shader_asset.c_str());
        context->material_template_data.shader_asset = "";
        valid = false;
      }
      else
      {
        if (shader_changed)
        {
          SetMaterialPropertiesFromShader(*shader->get(), &context->material_template_data);
        }
        VideoCommon::PixelShaderData* shader_data = shader->get();
        if (!asset->m_valid)
        {
          valid = false;
          ImGui::Text("The shader '%s' is invalid!",
                      context->material_template_data.shader_asset.c_str());
        }
        else
        {
          if (valid)
          {
            DrawMaterialGenerationContext(context, shader_data);
          }
        }
      }
    }
    else
    {
      ImGui::Text("Please choose a shader for this material");
      valid = false;
    }

    if (!valid)
    {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Import", ImVec2(120, 0)))
    {
      std::string error;
      GenerateMaterials(context, &error);
      if (error.empty())
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Failed to generate materials, error was '{}'", error);
      }
      ImGui::CloseCurrentPopup();
      result = true;
    }
    if (!valid)
    {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
      result = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return result;
}
}  // namespace GraphicsModEditor::Controls
