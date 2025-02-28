// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

#include "VideoCommon/Assets/RenderTargetAsset.h"

namespace GraphicsModEditor
{
struct EditorAsset;
class EditorAssetSource;
EditorAsset* GenerateRenderTarget(const std::filesystem::path& output_path,
                                  const VideoCommon::RenderTargetData& data,
                                  GraphicsModEditor::EditorAssetSource* source);
}  // namespace GraphicsModEditor
