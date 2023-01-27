// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <picojson.h>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/ShaderGenCommon.h"

class CustomPixelShaderAction final : public GraphicsModAction
{
public:
  struct TextureAllocationData
  {
    std::string file_name;
    std::string code_name;
    VideoCommon::CustomTextureData raw_data;
    std::optional<TextureConfig> config;
  };

  static std::unique_ptr<CustomPixelShaderAction> Create(const picojson::value& json_data,
                                                         std::string_view path);
  CustomPixelShaderAction(std::string color_shader_data,
                          std::vector<TextureAllocationData> texture_allocations);
  ~CustomPixelShaderAction();
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnTextureLoad(GraphicsModActionData::TextureLoad*) override;

private:
  std::string m_color_shader_data;

  ShaderCode m_last_generated_shader_code;

  bool m_valid = true;

  std::vector<TextureAllocationData> m_texture_data;
  std::vector<std::unique_ptr<AbstractTexture>> m_textures;
};
