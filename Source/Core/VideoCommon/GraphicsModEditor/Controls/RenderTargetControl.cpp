// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/RenderTargetControl.h"

#include <chrono>

#include <imgui.h>

#include "VideoCommon/Assets/RenderTargetAsset.h"
#include "VideoCommon/GraphicsModEditor/Controls/TextureControl.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
RenderTargetControl::RenderTargetControl(EditorState& state) : m_state(state)
{
}
void RenderTargetControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                    VideoCommon::RenderTargetData* render_target_data,
                                    const std::filesystem::path& path,
                                    VideoCommon::CustomAsset::TimeType* last_data_write,
                                    AbstractTexture* texture_preview)
{
  VideoCommon::TextureAndSamplerData texture_data;
  texture_data.sampler = render_target_data->sampler;
  texture_data.type = render_target_data->type;
  TextureControl control(m_state);
  control.DrawImGui(asset_id, &texture_data, path, last_data_write, nullptr);
  render_target_data->sampler = texture_data.sampler;
  render_target_data->type = texture_data.type;
  if (ImGui::BeginTable("RenderTargetForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Width");
    ImGui::TableNextColumn();
    int width = render_target_data->width;
    if (ImGui::InputInt("##Width", &width))
    {
      render_target_data->width = static_cast<u16>(width);
      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Height");
    ImGui::TableNextColumn();
    int height = render_target_data->height;
    if (ImGui::InputInt("##Height", &height))
    {
      render_target_data->height = static_cast<u16>(width);
      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
      GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(asset_id);
    }
    ImGui::EndTable();

    if (texture_preview)
    {
      const auto ratio = texture_preview->GetWidth() / texture_preview->GetHeight();
      ImGui::Image(*texture_preview, ImVec2{ImGui::GetContentRegionAvail().x,
                                            ImGui::GetContentRegionAvail().x * ratio});
    }
  }
}
}  // namespace GraphicsModEditor::Controls
