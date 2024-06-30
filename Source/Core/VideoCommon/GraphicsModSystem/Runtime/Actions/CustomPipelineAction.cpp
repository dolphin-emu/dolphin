// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"

#include <algorithm>
#include <array>
#include <optional>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomResourceManager.h"

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  return std::make_unique<CustomPipelineAction>(std::move(library));
}

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto material_asset =
      ReadStringFromJson(json_data.get<picojson::object>(), "material_asset");

  if (!material_asset)
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load custom pipeline action, 'material_asset' does not have a value");
    return nullptr;
  }

  return std::make_unique<CustomPipelineAction>(std::move(library), std::move(*material_asset));
}

CustomPipelineAction::CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
    : m_library(std::move(library))
{
}

CustomPipelineAction::CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                           std::string material_asset)
    : m_library(std::move(library)), m_material_asset(std::move(material_asset))
{
}

void CustomPipelineAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->material) [[unlikely]]
    return;

  if (m_material_asset.empty())
    return;

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
  *draw_started->material = resource_manager.GetMaterialFromAsset(m_material_asset, m_library,
                                                                  draw_started->draw_data_view);
}

void CustomPipelineAction::DrawImGui()
{
  auto& editor = Core::System::GetInstance().GetGraphicsModEditor();
  if (ImGui::CollapsingHeader("Custom pipeline", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomPipelineForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Material");
      ImGui::TableNextColumn();
      if (GraphicsModEditor::Controls::AssetDisplay("CustomPipelineActionMaterial",
                                                    editor.GetEditorState(), &m_material_asset,
                                                    GraphicsModEditor::RasterMaterial))
      {
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(m_material_asset);
      }
      ImGui::EndTable();
    }
  }
}

void CustomPipelineAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj["material_asset"] = picojson::value{m_material_asset};
}

std::string CustomPipelineAction::GetFactoryName() const
{
  return "custom_pipeline";
}
