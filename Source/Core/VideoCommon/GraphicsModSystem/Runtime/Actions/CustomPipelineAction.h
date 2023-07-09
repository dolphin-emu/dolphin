// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <picojson.h>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/ShaderGenCommon.h"

class CustomPipelineAction final : public GraphicsModAction
{
public:
  struct PipelinePassPassDescription
  {
    std::string m_pixel_material_asset;
  };

  static std::unique_ptr<CustomPipelineAction> Create(const picojson::value& json_data,
                                                      std::string_view path);
  CustomPipelineAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                       std::vector<PipelinePassPassDescription> pass_descriptions);
  ~CustomPipelineAction();
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnTextureCreate(GraphicsModActionData::TextureCreate*) override;

private:
  std::shared_ptr<VideoCommon::CustomAssetLibrary> m_library;
  std::vector<PipelinePassPassDescription> m_passes_config;
  struct PipelinePass
  {
    VideoCommon::CachedAsset<VideoCommon::MaterialAsset> m_pixel_material;
    VideoCommon::CachedAsset<VideoCommon::PixelShaderAsset> m_pixel_shader;
    std::vector<VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>> m_game_textures;
  };
  std::vector<PipelinePass> m_passes;

  ShaderCode m_last_generated_shader_code;

  bool m_valid = true;

  std::vector<std::string> m_texture_code_names;
};
