// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Core/System.h"

#include "VideoCommon/Resources/CustomResourceManager.h"

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  return std::make_unique<CustomPipelineAction>(std::move(library));
}

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto material_asset = ReadStringFromJson(json_data.get<picojson::object>(), "material_asset");

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

void CustomPipelineAction::OnDrawStarted(GraphicsModActionData::DrawStarted*)
{
  // TODO
}

void CustomPipelineAction::AfterEFB(GraphicsModActionData::PostEFB* post_efb)
{
  if (!post_efb) [[unlikely]]
    return;

  if (m_material_asset.empty())
    return;

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
  post_efb->material =
      resource_manager.GetPostProcessingMaterialFromAsset(m_material_asset, m_library);
}
