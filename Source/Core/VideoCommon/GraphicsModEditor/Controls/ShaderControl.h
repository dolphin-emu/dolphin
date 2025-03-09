// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace GraphicsModEditor::Controls
{
class ShaderControl
{
public:
  explicit ShaderControl(EditorState& state);
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::PixelShaderData* shader,
                 VideoCommon::CustomAssetLibrary::TimeType* last_data_write);
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::RasterShaderData* shader);

private:
  void AddShaderProperty(VideoCommon::RasterShaderData* shader,
                         std::map<std::string, VideoCommon::ShaderProperty2>* properties,
                         std::string_view type);
  std::string m_add_property_name;
  std::string m_add_property_chosen_type;
  VideoCommon::ShaderProperty::Value m_add_property_data;
  VideoCommon::ShaderProperty2::Value m_add_property2_data;
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
