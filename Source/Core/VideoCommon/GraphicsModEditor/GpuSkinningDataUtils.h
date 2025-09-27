// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor
{
picojson::object ToJsonObject(const GpuSkinningData& gpu_skinning_data);
void FromJson(const picojson::object& obj, GpuSkinningData& gpu_skinning_data);

void CalculateMeshDataWithGpuSkinning(const GpuSkinningData& gpu_skinning_data,
                                      const VideoCommon::MeshData& reference_mesh_data,
                                      VideoCommon::MeshData& calculated_mesh_data);
}  // namespace GraphicsModEditor
