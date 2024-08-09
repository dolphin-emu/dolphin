// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace VideoCommon
{
struct MaterialData;
struct PixelShaderData;
}  // namespace VideoCommon

namespace GraphicsModEditor::Controls
{
class MaterialControl
{
public:
  explicit MaterialControl(EditorState& state);
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::MaterialData* material,
                 VideoCommon::CustomAssetLibrary::TimeType* last_data_write, bool* valid);

private:
  void DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                   VideoCommon::PixelShaderData* shader, VideoCommon::MaterialData* material,
                   VideoCommon::CustomAssetLibrary::TimeType* last_data_write);
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
