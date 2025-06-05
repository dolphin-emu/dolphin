// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor
{
bool NativeMeshToFile(File::IOFile* file_data, const NativeMeshData& native_mesh_data);
bool NativeMeshFromFile(File::IOFile* file_data, NativeMeshData* data);

struct VertexInfluence
{
  std::array<int, 4> bone_ids;
  std::array<float, 4> weights;
};

class ExporterSkinningRig
{
public:
  VideoCommon::SkinningRig runtime_rig;

  // Each welded vertex has exactly 4 influences (stride of 4)
  std::vector<VertexInfluence> global_weights;

  std::map<GraphicsModSystem::DrawCallID, std::vector<int>> draw_call_to_local_svj;

  static bool FromBinary(std::span<const u8> raw_data, ExporterSkinningRig* data);
  static bool ToBinary(File::IOFile* file_data, const ExporterSkinningRig& data);
};

// native_mesh_data = the data to transfer to the final mesh
// reference_mesh_data = the new replacement mesh that might have its positions altered to better
// match the original mesh
// final_mesh_data = the final mesh with the skinning data applied, passed
// in as the new replacement mesh
void CalculateMeshDataWithSkinning(const NativeMeshData& native_mesh_data,
                                   const VideoCommon::MeshData& reference_mesh_data,
                                   u32 components_to_copy,
                                   const std::optional<ExporterSkinningRig>& cpu_skinning_rig,
                                   VideoCommon::MeshData* final_mesh_data);
}  // namespace GraphicsModEditor
