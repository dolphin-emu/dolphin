// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <picojson.h>

#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor
{
bool NativeMeshToFile(File::IOFile* file_data, const NativeMeshData& native_mesh_data);
bool NativeMeshFromFile(File::IOFile* file_data, NativeMeshData* data);

using VertexGroupPerDrawCall =
    std::map<GraphicsModSystem::DrawCallID, std::vector<VideoCommon::VertexGroup>>;

// native_mesh_data = the data to transfer to the final mesh
// reference_mesh_data = the new replacement mesh that might have its positions altered to better
// match the original mesh
// final_mesh_data = the final mesh with the skinning data applied, passed
// in as the new replacement mesh
void CalculateMeshDataWithSkinning(const NativeMeshData& native_mesh_data,
                                   const VideoCommon::MeshData& reference_mesh_data,
                                   u32 components_to_copy,
                                   const VertexGroupPerDrawCall& cpu_vertex_groups,
                                   VideoCommon::MeshData* final_mesh_data);
}  // namespace GraphicsModEditor
