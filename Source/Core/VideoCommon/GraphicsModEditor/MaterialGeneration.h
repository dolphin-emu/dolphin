// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "VideoCommon/Assets/MaterialAsset.h"

namespace GraphicsModEditor
{
struct EditorAsset;
class EditorAssetSource;
struct EditorState;
struct ShaderGenerationContext;

struct MaterialGenerationContext
{
  EditorState* state;
  std::string lookup_path;
  std::string input_path;
  std::string output_path;
  VideoCommon::MaterialData material_template_data;
  std::map<std::size_t, std::string> material_property_index_to_texture_filter;
  bool search_input_recursively = true;
};
struct RasterMaterialGenerationContext
{
  EditorState* state;
  std::string lookup_path;
  std::string input_path;
  std::string output_path;
  VideoCommon::RasterMaterialData material_template_data;
  std::map<std::size_t, std::string> material_property_index_to_texture_filter;
  bool search_input_recursively = true;
};
void GenerateMaterials(MaterialGenerationContext* generation_context, std::string* error);
void GenerateMaterials(RasterMaterialGenerationContext* generation_context, std::string* error);
EditorAsset* GenerateMaterial(std::string_view name, const std::filesystem::path& output_path,
                              const VideoCommon::RasterMaterialData& metadata_template,
                              const ShaderGenerationContext& shader_context,
                              GraphicsModEditor::EditorAssetSource* source);
EditorAsset* GenerateMaterial(std::string_view name, const std::filesystem::path& output_path,
                              std::string_view metadata,
                              GraphicsModEditor::EditorAssetSource* source);
}  // namespace GraphicsModEditor
