// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MaterialControl.h"

#include <string>
#include <variant>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/EnumUtils.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MaterialAssetUtils.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/RenderState.h"

namespace GraphicsModEditor::Controls
{
MaterialControl::MaterialControl(EditorState& state) : m_state(state)
{
}

void MaterialControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                VideoCommon::MaterialData* material,
                                VideoCommon::CustomAsset::TimeType* last_data_write, bool* valid)
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
        *last_data_write = std::chrono::steady_clock::now();
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

void MaterialControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                VideoCommon::RasterMaterialData* material, bool* valid)
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
                     AssetDataType::Shader))
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
        auto shader = std::get_if<std::unique_ptr<VideoCommon::RasterShaderData>>(&asset->m_data);
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
        SetMaterialTexturesFromShader(*shader->get(), material);
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
    auto shader = std::get_if<std::unique_ptr<VideoCommon::RasterShaderData>>(&asset->m_data);
    if (!shader)
    {
      ImGui::Text(
          "%s", fmt::format("Asset id '{}' was not type shader!", material->shader_asset).c_str());
    }
    else
    {
      VideoCommon::RasterShaderData* shader_data = shader->get();
      if (!asset->m_valid)
      {
        ImGui::Text("%s",
                    fmt::format("The shader '{}' is invalid!", material->shader_asset).c_str());
      }
      else
      {
        DrawControl(asset_id, shader_data, material);
      }
    }
  }
}

void MaterialControl::DrawSamplers(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                   EditorState* editor_state,
                                   std::span<VideoCommon::RasterShaderData::SamplerData> samplers,
                                   std::span<VideoCommon::TextureSamplerValue> textures)
{
  std::size_t texture_sampler_index = 0;
  for (const auto& texture_sampler : samplers)
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", texture_sampler.name.c_str());
    ImGui::TableNextColumn();

    auto& texture = textures[texture_sampler_index];

    constexpr std::array<GraphicsModEditor::AssetDataType, 2> asset_types_allowed{
        AssetDataType::Texture, AssetDataType::RenderTarget};
    AssetDataType asset_type_chosen =
        texture.is_render_target ? AssetDataType::RenderTarget : AssetDataType::Texture;
    if (AssetDisplay(texture_sampler.name, editor_state, &texture.asset, &asset_type_chosen,
                     asset_types_allowed))
    {
      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
    }
    texture.is_render_target = asset_type_chosen == AssetDataType::RenderTarget;

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
          GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    if (texture.sampler_origin == VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
    {
      ImGui::BeginDisabled();
    }
    if (ImGui::InputText(fmt::format("##{}TextureHash", texture_sampler.name).c_str(),
                         &texture.texture_hash))
    {
      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
    }
    if (texture.sampler_origin == VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
    {
      ImGui::EndDisabled();
    }

    texture_sampler_index++;
  }
}

void MaterialControl::DrawRenderTargets(
    const VideoCommon::CustomAssetLibrary::AssetID& asset_id, EditorState* editor_state,
    std::span<VideoCommon::RasterShaderData::OutputTargetData> output_targets,
    std::vector<VideoCommon::CustomAssetLibrary::AssetID>* render_targets)
{
  if (!render_targets) [[unlikely]]
    return;

  constexpr std::array<GraphicsModEditor::AssetDataType, 1> asset_types_allowed{
      AssetDataType::RenderTarget};
  AssetDataType asset_type_chosen = AssetDataType::RenderTarget;
  for (std::size_t i = 0; i < output_targets.size(); i++)
  {
    auto& output_target = output_targets[i];
    auto& render_target = (*render_targets)[i];

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", output_target.name.c_str());
    ImGui::TableNextColumn();

    if (AssetDisplay("RenderTargetWindow", editor_state, &render_target, &asset_type_chosen,
                     asset_types_allowed))
    {
      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
    }
  }
}

void MaterialControl::DrawUniforms(
    const VideoCommon::CustomAssetLibrary::AssetID& asset_id, EditorState*,
    const std::map<std::string, VideoCommon::ShaderProperty2>& shader_properties,
    std::vector<VideoCommon::MaterialProperty2>* material_properties)
{
  std::size_t material_property_index = 0;
  for (const auto& entry : shader_properties)
  {
    // C++20: error with capturing structured bindings for our version of clang
    const auto& name = entry.first;
    const auto& shader_property = entry.second;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", name.c_str());
    ImGui::TableNextColumn();

    auto& material_property = (*material_properties)[material_property_index];

    std::visit(
        overloaded{[&](s32& value) {
                     if (ImGui::InputInt(fmt::format("##{}", name).c_str(), &value))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](std::array<s32, 2>& value) {
                     if (ImGui::InputInt2(fmt::format("##{}", name).c_str(), value.data()))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](std::array<s32, 3>& value) {
                     if (ImGui::InputInt3(fmt::format("##{}", name).c_str(), value.data()))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](std::array<s32, 4>& value) {
                     if (ImGui::InputInt4(fmt::format("##{}", name).c_str(), value.data()))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](float& value) {
                     if (ImGui::InputFloat(fmt::format("##{}", name).c_str(), &value))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](std::array<float, 2>& value) {
                     if (ImGui::InputFloat2(fmt::format("##{}", name).c_str(), value.data()))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   },
                   [&](std::array<float, 3>& value) {
                     if (std::holds_alternative<VideoCommon::ShaderProperty2::RGB>(
                             shader_property.m_default))
                     {
                       if (ImGui::ColorEdit3(fmt::format("##{}", name).c_str(), value.data()))
                       {
                         GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                       }
                     }
                     else
                     {
                       if (ImGui::InputFloat3(fmt::format("##{}", name).c_str(), value.data()))
                       {
                         GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                       }
                     }
                   },
                   [&](std::array<float, 4>& value) {
                     if (std::holds_alternative<VideoCommon::ShaderProperty2::RGBA>(
                             shader_property.m_default))
                     {
                       if (ImGui::ColorEdit4(fmt::format("##{}", name).c_str(), value.data()))
                       {
                         GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                       }
                     }
                     else
                     {
                       if (ImGui::InputFloat4(fmt::format("##{}", name).c_str(), value.data()))
                       {
                         GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                       }
                     }
                   },
                   [&](bool& value) {
                     if (ImGui::Checkbox(fmt::format("##{}", name).c_str(), &value))
                     {
                       GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                     }
                   }},
        material_property.m_value);
    material_property_index++;
  }
}

void MaterialControl::DrawProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                     EditorState*, VideoCommon::RasterMaterialData* material)
{
  if (ImGui::CollapsingHeader("Blending", ImGuiTreeNodeFlags_DefaultOpen))
  {
    bool custom_blending = material->blending_state.has_value();
    if (ImGui::Checkbox("Use Custom Data", &custom_blending))
    {
      if (custom_blending && !material->blending_state)
      {
        material->blending_state = BlendingState{};
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
      else if (!custom_blending && material->blending_state)
      {
        material->blending_state.reset();
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
    }

    if (material->blending_state && ImGui::BeginTable("BlendingForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Blend Enable");
      ImGui::TableNextColumn();

      bool blend_enable = material->blending_state->blendenable == 0;
      if (ImGui::Checkbox("##BlendEnable", &blend_enable))
      {
        material->blending_state->blendenable = static_cast<u32>(blend_enable);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Logicop Enable");
      ImGui::TableNextColumn();

      bool logicop_enable = material->blending_state->logicopenable == 0;
      if (ImGui::Checkbox("##LogicOpEnable", &logicop_enable))
      {
        material->blending_state->logicopenable = static_cast<u32>(logicop_enable);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Color Update");
      ImGui::TableNextColumn();

      bool colorupdate = material->blending_state->colorupdate == 0;
      if (ImGui::Checkbox("##ColorUpdate", &colorupdate))
      {
        material->blending_state->colorupdate = static_cast<u32>(colorupdate);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Alpha Update");
      ImGui::TableNextColumn();

      bool alphaupdate = material->blending_state->alphaupdate == 0;
      if (ImGui::Checkbox("##AlphaUpdate", &alphaupdate))
      {
        material->blending_state->alphaupdate = static_cast<u32>(alphaupdate);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Culling", ImGuiTreeNodeFlags_DefaultOpen))
  {
    bool custom_cull_mode = material->cull_mode.has_value();
    if (ImGui::Checkbox("Use Custom Data##2", &custom_cull_mode))
    {
      if (custom_cull_mode && !material->cull_mode)
      {
        material->cull_mode = CullMode{};
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
      else if (!custom_cull_mode && material->cull_mode)
      {
        material->cull_mode.reset();
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
    }

    if (material->cull_mode && ImGui::BeginTable("CullingForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Cull Mode");
      ImGui::TableNextColumn();

      if (ImGui::BeginCombo("##CullMode", fmt::to_string(*material->cull_mode).c_str()))
      {
        static constexpr std::array<CullMode, 4> all_cull_modes = {CullMode::None, CullMode::Back,
                                                                   CullMode::Front, CullMode::All};

        for (const auto& cull_mode : all_cull_modes)
        {
          const bool is_selected = cull_mode == *material->cull_mode;
          const auto cull_mode_str = fmt::to_string(cull_mode);
          if (ImGui::Selectable(fmt::format("{}##", cull_mode_str).c_str(), is_selected))
          {
            material->cull_mode = cull_mode;
            GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Depth Test", ImGuiTreeNodeFlags_DefaultOpen))
  {
    bool custom_depth = material->depth_state.has_value();
    if (ImGui::Checkbox("Use Custom Data##3", &custom_depth))
    {
      if (custom_depth && !material->depth_state)
      {
        material->depth_state = DepthState{};
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
      else if (!custom_depth && material->depth_state)
      {
        material->depth_state.reset();
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }
    }

    if (material->depth_state && ImGui::BeginTable("DepthForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Test Enable");
      ImGui::TableNextColumn();

      bool test_enable = static_cast<bool>(material->depth_state->testenable);
      if (ImGui::Checkbox("##TestEnable", &test_enable))
      {
        material->depth_state->testenable = static_cast<u32>(test_enable);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Update Enable");
      ImGui::TableNextColumn();

      bool update_enable = static_cast<bool>(material->depth_state->updateenable);
      if (ImGui::Checkbox("##UpdateEnable", &update_enable))
      {
        material->depth_state->updateenable = static_cast<u32>(update_enable);
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Comparison");
      ImGui::TableNextColumn();

      if (ImGui::BeginCombo("##CompareMode", fmt::to_string(material->depth_state->func).c_str()))
      {
        static constexpr std::array<CompareMode, 8> all_compare_modes = {
            CompareMode::Never,   CompareMode::Less,   CompareMode::Equal,  CompareMode::LEqual,
            CompareMode::Greater, CompareMode::NEqual, CompareMode::GEqual, CompareMode::Always};

        for (const auto& compare_mode : all_compare_modes)
        {
          const bool is_selected = compare_mode == material->depth_state->func;
          const auto compare_mode_str = fmt::to_string(compare_mode);
          if (ImGui::Selectable(fmt::format("{}##", compare_mode_str).c_str(), is_selected))
          {
            material->depth_state->func = compare_mode;
            GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
          }
        }
        ImGui::EndCombo();
      }

      ImGui::EndTable();
    }
  }
}

void MaterialControl::DrawLinkedMaterial(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                         EditorState* editor_state,
                                         VideoCommon::RasterMaterialData* material)
{
  if (AssetDisplay("Next Material", editor_state, &material->next_material_asset,
                   AssetDataType::RasterMaterial))
  {
    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
  }
}

void MaterialControl::DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                  VideoCommon::PixelShaderData* shader,
                                  VideoCommon::MaterialData* material,
                                  VideoCommon::CustomAsset::TimeType* last_data_write)
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
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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
                        *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 2>& value) {
                  if (ImGui::InputInt2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 3>& value) {
                  if (ImGui::InputInt3(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<s32, 4>& value) {
                  if (ImGui::InputInt4(fmt::format("##{}", material_property.m_code_name).c_str(),
                                       value.data()))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](float& value) {
                  if (ImGui::InputFloat(fmt::format("##{}", material_property.m_code_name).c_str(),
                                        &value))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                    GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                  }
                },
                [&](std::array<float, 2>& value) {
                  if (ImGui::InputFloat2(fmt::format("##{}", material_property.m_code_name).c_str(),
                                         value.data()))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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
                      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                  else
                  {
                    if (ImGui::InputFloat3(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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
                      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                  else
                  {
                    if (ImGui::InputFloat4(
                            fmt::format("##{}", material_property.m_code_name).c_str(),
                            value.data()))
                    {
                      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
                      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
                    }
                  }
                },
                [&](bool& value) {
                  if (ImGui::Checkbox(fmt::format("##{}", material_property.m_code_name).c_str(),
                                      &value))
                  {
                    *last_data_write = VideoCommon::CustomAsset::ClockType::now();
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

void MaterialControl::DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                  VideoCommon::RasterShaderData* shader,
                                  VideoCommon::RasterMaterialData* material)
{
  if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("MaterialVertexPropertiesForm", 2))
    {
      DrawUniforms(asset_id, &m_state, shader->m_vertex_properties, &material->vertex_properties);

      ImGui::EndTable();
    }

    if (ImGui::BeginTable("MaterialPixelPropertiesForm", 2))
    {
      DrawUniforms(asset_id, &m_state, shader->m_pixel_properties, &material->pixel_properties);
      DrawSamplers(asset_id, &m_state, shader->m_pixel_samplers, material->pixel_textures);

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("MaterialRenderTargetsForm", 2))
    {
      DrawRenderTargets(asset_id, &m_state, shader->m_output_targets, &material->render_targets);
      ImGui::EndTable();
    }
  }

  DrawProperties(asset_id, &m_state, material);
  DrawLinkedMaterial(asset_id, &m_state, material);
}
}  // namespace GraphicsModEditor::Controls
