// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/ShaderGeneration.h"

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace GraphicsModEditor
{
EditorAsset* GenerateShader(const std::filesystem::path& output_path,
                            const ShaderGenerationContext& context,
                            GraphicsModEditor::EditorAssetSource* source)
{
  picojson::object obj;
  VideoCommon::RasterShaderData::ToJson(obj, context.shader_template);

  return GenerateShader(output_path, context.pixel_name, context.vertex_name, context.metadata_name,
                        context.shader_template.m_pixel_source,
                        context.shader_template.m_vertex_source, picojson::value{obj}.serialize(),
                        source);
}

EditorAsset* GenerateShader(const std::filesystem::path& output_path, std::string_view pixel_name,
                            std::string_view vertex_name, std::string_view metadata_name,
                            std::string_view pixel_source, std::string_view vertex_source,
                            std::string_view metadata_source,
                            GraphicsModEditor::EditorAssetSource* source)
{
  const auto metadata_path = output_path / metadata_name;
  if (!File::WriteStringToFile(PathToString(metadata_path), metadata_source))
  {
    return nullptr;
  }

  const auto vertex_path = output_path / vertex_name;
  if (!File::WriteStringToFile(PathToString(vertex_path), vertex_source))
  {
    return nullptr;
  }

  const auto pixel_path = output_path / pixel_name;
  if (!File::WriteStringToFile(PathToString(pixel_path), pixel_source))
  {
    return nullptr;
  }

  source->AddAsset(metadata_path);
  return source->GetAssetFromPath(metadata_path);
}
}  // namespace GraphicsModEditor
