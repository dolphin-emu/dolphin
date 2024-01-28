// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class CustomPipelineAction final : public GraphicsModAction
{
public:
  struct PipelinePassPassDescription
  {
    std::string m_pixel_material_asset;
  };

  static std::unique_ptr<CustomPipelineAction>
  Create(const picojson::value& json_data,
         std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  static std::unique_ptr<CustomPipelineAction>
  Create(std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  explicit CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                       std::vector<PipelinePassPassDescription> pass_descriptions);
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

private:
  std::shared_ptr<VideoCommon::CustomAssetLibrary> m_library;
  std::vector<PipelinePassPassDescription> m_passes_config;
  std::vector<CustomPipeline> m_pipeline_passes;
};
