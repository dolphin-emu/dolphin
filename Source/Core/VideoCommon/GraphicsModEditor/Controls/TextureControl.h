// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace VideoCommon
{
class AbstractTexture;
struct TextureData;
}  // namespace VideoCommon

namespace GraphicsModEditor::Controls
{
class TextureControl
{
public:
  explicit TextureControl(EditorState& state);
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::TextureData* texture_data, const std::filesystem::path& path,
                 VideoCommon::CustomAssetLibrary::TimeType* last_data_write,
                 AbstractTexture* texture_preview);

private:
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
