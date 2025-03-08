// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/RenderTargetGeneration.h"

#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"

#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace GraphicsModEditor
{
EditorAsset* GenerateRenderTarget(const std::filesystem::path& output_path,
                                  const VideoCommon::RenderTargetData& data,
                                  GraphicsModEditor::EditorAssetSource* source)
{
  picojson::object obj;
  VideoCommon::RenderTargetData::ToJson(&obj, data);

  const auto metadata_path = output_path / "metadata.rendertarget";
  if (!JsonToFile(PathToString(metadata_path), picojson::value{obj}, true))
  {
    return nullptr;
  }

  source->AddAsset(metadata_path);
  return source->GetAssetFromPath(metadata_path);
}
}  // namespace GraphicsModEditor
