// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <span>
#include <vector>

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace VideoCommon
{
struct MaterialData;
struct PixelShaderData;
struct RasterMaterialData;
struct RasterShaderData;
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
  void DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                 VideoCommon::RasterMaterialData* material, bool* valid);

  static void DrawSamplers(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                           EditorState* editor_state,
                           std::span<VideoCommon::RasterShaderData::SamplerData> samplers,
                           std::span<VideoCommon::TextureSamplerValue> textures);

  static void
  DrawUniforms(const VideoCommon::CustomAssetLibrary::AssetID& asset_id, EditorState* editor_state,
               const std::map<std::string, VideoCommon::ShaderProperty2>& shader_properties,
               std::vector<VideoCommon::MaterialProperty2>* material_properties);

  static void DrawProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                             EditorState* editor_state, VideoCommon::RasterMaterialData* material);

  static void DrawLinkedMaterial(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                 EditorState* editor_state,
                                 VideoCommon::RasterMaterialData* material);

private:
  void DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                   VideoCommon::PixelShaderData* shader, VideoCommon::MaterialData* material,
                   VideoCommon::CustomAssetLibrary::TimeType* last_data_write);
  void DrawControl(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                   VideoCommon::RasterShaderData* shader,
                   VideoCommon::RasterMaterialData* material);
  EditorState& m_state;
};
}  // namespace GraphicsModEditor::Controls
