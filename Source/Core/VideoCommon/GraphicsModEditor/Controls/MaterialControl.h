// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
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
                 VideoCommon::MaterialData* material, bool* valid);

  static void DrawSamplers(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                           EditorState* editor_state,
                           std::span<VideoCommon::RasterSurfaceShaderData::SamplerData> samplers,
                           std::span<VideoCommon::TextureSamplerValue> textures);

  static void DrawUniforms(
      const VideoCommon::CustomAssetLibrary::AssetID& asset_id, EditorState* editor_state,
      const decltype(VideoCommon::RasterSurfaceShaderData::uniform_properties)& shader_properties,
      std::vector<VideoCommon::MaterialProperty>* material_properties);

  static void DrawProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                             EditorState* editor_state, VideoCommon::MaterialData* material);

  static void DrawLinkedMaterial(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                 EditorState* editor_state, VideoCommon::MaterialData* material);

private:
  void DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                   VideoCommon::PixelShaderData* shader, VideoCommon::MaterialData* material,
                   VideoCommon::CustomAsset::TimeType* last_data_write);
  void DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                   VideoCommon::RasterSurfaceShaderData* shader,
                   VideoCommon::MaterialData* material);
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
