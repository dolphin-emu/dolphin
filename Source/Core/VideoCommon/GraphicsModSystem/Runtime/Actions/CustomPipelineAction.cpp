// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"

#include "Common/Logging/Log.h"

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  return std::make_unique<CustomPipelineAction>(std::move(library));
}

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  std::vector<CustomPipelineAction::PipelinePassPassDescription> pipeline_passes;

  const auto& passes_json = json_data.get("passes");
  if (passes_json.is<picojson::array>())
  {
    for (const auto& passes_json_val : passes_json.get<picojson::array>())
    {
      CustomPipelineAction::PipelinePassPassDescription pipeline_pass;
      if (!passes_json_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load custom pipeline action, 'passes' has an array value that "
                      "is not an object!");
        return nullptr;
      }

      auto pass = passes_json_val.get<picojson::object>();
      if (!pass.contains("pixel_material_asset"))
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load custom pipeline action, 'passes' value missing required "
                      "field 'pixel_material_asset'");
        return nullptr;
      }

      auto pixel_material_asset_json = pass["pixel_material_asset"];
      if (!pixel_material_asset_json.is<std::string>())
      {
        ERROR_LOG_FMT(VIDEO, "Failed to load custom pipeline action, 'passes' field "
                             "'pixel_material_asset' is not a string!");
        return nullptr;
      }
      pipeline_pass.m_pixel_material_asset = pixel_material_asset_json.to_str();
      pipeline_passes.push_back(std::move(pipeline_pass));
    }
  }

  if (pipeline_passes.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load custom pipeline action, must specify at least one pass");
    return nullptr;
  }

  if (pipeline_passes.size() > 1)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load custom pipeline action, multiple passes are not currently supported");
    return nullptr;
  }

  return std::make_unique<CustomPipelineAction>(std::move(library), std::move(pipeline_passes));
}

CustomPipelineAction::CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
    : m_library(std::move(library))
{
}

CustomPipelineAction::CustomPipelineAction(
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
    std::vector<PipelinePassPassDescription> pass_descriptions)
    : m_library(std::move(library)), m_passes_config(std::move(pass_descriptions))
{
  m_pipeline_passes.resize(m_passes_config.size());
}

void CustomPipelineAction::OnDrawStarted(GraphicsModActionData::DrawStarted*)
{
}
