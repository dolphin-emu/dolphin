// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataDumper.h"

#include "Common/IOFile.h"

namespace GraphicsModEditor
{
void GpuSkinningDataDumper::SetOutputPath(std::string path)
{
  m_path = std::move(path);
}

void GpuSkinningDataDumper::WriteToFile()
{
  if (m_native_mesh_data.m_draw_call_to_chunk.empty())
    return;

  File::IOFile outbound_file(m_path, "wb");
  if (!GraphicsModEditor::NativeMeshToFile(&outbound_file, m_native_mesh_data))
  {
    // TODO: move this to the UI
    ERROR_LOG_FMT(VIDEO, "Failed to write Dolphin native mesh '{}'", m_path);
  }
}

void GpuSkinningDataDumper::AddData(GraphicsModSystem::DrawCallID draw_call_id,
                                    const GraphicsModSystem::DrawDataView& draw_data,
                                    TransformType apply_transform_type,
                                    std::span<const u16> index_data)
{
  const auto& declaration = draw_data.vertex_format->GetVertexDeclaration();
  if (!declaration.position.enable ||
      (!declaration.posmtx.enable && apply_transform_type == TransformType::GPU))
  {
    return;
  }

  auto& chunk_data = m_native_mesh_data.m_draw_call_to_chunk[draw_call_id];
  chunk_data.primitive_type = draw_data.uid->rasterization_state.primitive;
  chunk_data.vertex_declaration = declaration;
  chunk_data.vertex_data = std::vector<u8>{
      draw_data.vertex_data, draw_data.vertex_data + declaration.stride * draw_data.vertex_count};

  chunk_data.positions_transformed.clear();
  chunk_data.position_inverse_transforms.clear();

  chunk_data.index_data.clear();
  chunk_data.index_data.reserve(index_data.size());
  chunk_data.index_data.assign(index_data.begin(), index_data.end());

  const auto byte_offset = declaration.position.offset;
  auto vert_ptr = draw_data.vertex_data;
  for (u32 vert_index = 0; vert_index < draw_data.vertex_count; vert_index++)
  {
    u32 gpu_skin_index;
    std::memcpy(&gpu_skin_index, vert_ptr + declaration.posmtx.offset, sizeof(u32));

    Common::Matrix44 position_transform = Common::Matrix44::Identity();
    Common::Matrix33 normal_transform = Common::Matrix33::Identity();
    if (apply_transform_type == TransformType::GPU)
    {
      for (std::size_t i = 0; i < 3; i++)
      {
        position_transform.data[i * 4 + 0] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][0];
        position_transform.data[i * 4 + 1] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][1];
        position_transform.data[i * 4 + 2] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][2];
        position_transform.data[i * 4 + 3] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][3];
      }
      position_transform.data[12] = 0;
      position_transform.data[13] = 0;
      position_transform.data[14] = 0;
      position_transform.data[15] = 1;

      const u32 gpu_skin_normal_index = gpu_skin_index & 31;
      for (std::size_t i = 0; i < 3; i++)
      {
        const auto row = draw_data.gpu_skinning_normal_transform[gpu_skin_normal_index + i];
        normal_transform.data[i * 3 + 0] = row[0];
        normal_transform.data[i * 3 + 1] = row[1];
        normal_transform.data[i * 3 + 2] = row[2];
      }
    }
    else if (apply_transform_type == TransformType::Object)
    {
      for (std::size_t i = 0; i < 3; i++)
      {
        position_transform.data[i * 4 + 0] = draw_data.object_transform[i][0];
        position_transform.data[i * 4 + 1] = draw_data.object_transform[i][1];
        position_transform.data[i * 4 + 2] = draw_data.object_transform[i][2];
        position_transform.data[i * 4 + 3] = draw_data.object_transform[i][3];
      }
      position_transform.data[12] = 0;
      position_transform.data[13] = 0;
      position_transform.data[14] = 0;
      position_transform.data[15] = 1;

      const u32 normal_index_offset = 3;
      for (std::size_t i = 0; i < 3; i++)
      {
        const auto row = draw_data.object_transform[normal_index_offset + i];
        normal_transform.data[i * 3 + 0] = row[0];
        normal_transform.data[i * 3 + 1] = row[1];
        normal_transform.data[i * 3 + 2] = row[2];
      }
    }

    // Update position and track position transform if applicable
    Common::Vec3 vertex_position;
    std::memcpy(&vertex_position.x, vert_ptr + byte_offset, sizeof(float));
    std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + byte_offset, sizeof(float));
    std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + byte_offset, sizeof(float));
    chunk_data.positions_transformed.push_back(position_transform.Transform(vertex_position, 1));

    if (apply_transform_type != TransformType::None)
    {
      position_transform = position_transform.Inverted();
    }

    chunk_data.position_inverse_transforms.push_back(std::move(position_transform));

    // Track normal transform if applicable
    if (apply_transform_type != TransformType::None)
    {
      normal_transform = normal_transform.Inverted();
    }

    chunk_data.normal_inverse_transforms.push_back(std::move(normal_transform));

    vert_ptr += draw_data.vertex_format->GetVertexStride();
  }
}
}  // namespace GraphicsModEditor
