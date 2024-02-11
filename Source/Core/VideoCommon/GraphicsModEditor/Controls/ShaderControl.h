// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

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

private:
  std::string m_add_property_name;
  std::string m_add_property_chosen_type;
  VideoCommon::ShaderProperty::Value m_add_property_data;
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
