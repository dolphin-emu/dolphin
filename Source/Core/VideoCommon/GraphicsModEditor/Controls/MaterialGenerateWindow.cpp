// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MaterialGenerateWindow.h"

#include <algorithm>
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
#include "VideoCommon/GraphicsModEditor/Controls/MaterialControl.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
namespace
{
void DrawSamplers(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                  EditorState* editor_state,
                  std::span<VideoCommon::RasterSurfaceShaderData::SamplerData> samplers,
                  std::span<VideoCommon::TextureSamplerValue> textures,
                  std::map<std::size_t, std::string>* texture_filter, bool* valid)
{
  std::size_t texture_sampler_index = 0;
  for (const auto& texture_sampler : samplers)
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", texture_sampler.name.c_str());
    ImGui::TableNextColumn();

    auto& texture = textures[texture_sampler_index];

    auto [iter, inserted] = texture_filter->try_emplace(texture_sampler_index, "{IMAGE_1}");
    ImGui::InputText(fmt::format("##{}", texture_sampler.name).c_str(), &iter->second);
    if (iter->second.find('{') == std::string::npos || iter->second.find('}') == std::string::npos)
    {
      *valid = false;
    }
    if (std::ranges::count(iter->second, '{') > 1 || std::ranges::count(iter->second, '}') > 1)
    {
      *valid = false;
    }

    const auto sampler_origin_str =
        VideoCommon::TextureSamplerValue::ToString(texture.sampler_origin);
    if (ImGui::BeginCombo(fmt::format("##{}SamplerOrigin", texture_sampler.name).c_str(),
                          sampler_origin_str.c_str()))
    {
      static constexpr std::array<VideoCommon::TextureSamplerValue::SamplerOrigin, 2>
          all_sampler_types = {VideoCommon::TextureSamplerValue::SamplerOrigin::Asset,
                               VideoCommon::TextureSamplerValue::SamplerOrigin::TextureHash};

      for (const auto& type : all_sampler_types)
      {
        const bool is_selected = type == texture.sampler_origin;
        const auto type_name = VideoCommon::TextureSamplerValue::ToString(type);
        if (ImGui::Selectable(fmt::format("{}##{}", type_name, texture_sampler.name).c_str(),
                              is_selected))
        {
          texture.sampler_origin = type;
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    texture_sampler_index++;
  }
}

void DrawMaterialGenerationContext(MaterialGenerationContext* context,
                                   VideoCommon::RasterSurfaceShaderData* shader_data, bool* valid)
{
  SetMaterialPropertiesFromShader(*shader_data, &context->material_template_data);
  SetMaterialTexturesFromShader(*shader_data, &context->material_template_data);

  if (ImGui::BeginTable("MaterialPropertiesForm", 2))
  {
    MaterialControl::DrawUniforms("", context->state, shader_data->uniform_properties,
                                  &context->material_template_data.properties);
    DrawSamplers("", context->state, shader_data->samplers,
                 context->material_template_data.textures,
                 &context->material_property_index_to_texture_filter, valid);

    ImGui::EndTable();
  }

  MaterialControl::DrawProperties("", context->state, &context->material_template_data);
  MaterialControl::DrawLinkedMaterial("", context->state, &context->material_template_data);
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
                       &context->material_template_data.shader_asset, AssetDataType::Shader))
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

      ImGui::EndTable();
    }

    auto asset = context->state->m_user_data.m_asset_library->GetAssetFromID(
        context->material_template_data.shader_asset);
    if (asset)
    {
      auto shader =
          std::get_if<std::unique_ptr<VideoCommon::RasterSurfaceShaderData>>(&asset->m_data);
      if (!shader)
      {
        ImGui::Text("Asset id '%s' was not type raster shader!",
                    context->material_template_data.shader_asset.c_str());
        context->material_template_data.shader_asset = "";
        valid = false;
      }
      else
      {
        if (shader_changed)
        {
          context->material_property_index_to_texture_filter.clear();
        }
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
            DrawMaterialGenerationContext(context, shader->get(), &valid);
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
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
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
