// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"

class AbstractTexture;

namespace VideoCommon
{
struct RenderTargetData;
}  // namespace VideoCommon

namespace GraphicsModEditor::Controls
{
class RenderTargetControl
{
public:
  explicit RenderTargetControl(EditorState& state);
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::RenderTargetData* render_target_data,
                 const std::filesystem::path& path,
                 VideoCommon::CustomAsset::TimeType* last_data_write,
                 AbstractTexture* texture_preview);

private:
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
