// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "VideoCommon/Assets/ShaderAsset.h"

namespace GraphicsModEditor
{
struct EditorAsset;
class EditorAssetSource;
struct ShaderGenerationContext
{
  VideoCommon::RasterShaderData shader_template;

  std::string_view pixel_name;
  std::string_view vertex_name;
  std::string_view metadata_name;
};
EditorAsset* GenerateShader(const std::filesystem::path& output_path,
                            const ShaderGenerationContext& context,
                            GraphicsModEditor::EditorAssetSource* source);
EditorAsset* GenerateShader(const std::filesystem::path& output_path, std::string_view pixel_name,
                            std::string_view vertex_name, std::string_view metadata_name,
                            std::string_view pixel_source, std::string_view vertex_source,
                            std::string_view metadata_source,
                            GraphicsModEditor::EditorAssetSource* source);
}  // namespace GraphicsModEditor
