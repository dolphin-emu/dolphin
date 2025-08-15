// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataDumper.h"

#include "Common/JsonUtil.h"

namespace GraphicsModEditor
{
void GpuSkinningDataDumper::SetOutputPath(std::string path)
{
  m_path = std::move(path);
}

void GpuSkinningDataDumper::WriteToFile()
{
  if (m_gpu_skinning_data.m_draw_call_to_gpu_skinning.empty())
    return;

  JsonToFile(m_path, picojson::value{ToJsonObject(m_gpu_skinning_data)}, true);
}

void GpuSkinningDataDumper::AddData(GraphicsModSystem::DrawCallID draw_call_id,
                                    const GraphicsModSystem::DrawDataView& draw_data,
                                    bool apply_gpu_skinning, std::span<const u16> index_data)
{
  const auto& declaration = draw_data.vertex_format->GetVertexDeclaration();
  if (!declaration.position.enable || !declaration.posmtx.enable)
  {
    return;
  }

  auto& gpu_skinning_data = m_gpu_skinning_data.m_draw_call_to_gpu_skinning[draw_call_id];
  gpu_skinning_data.positions.clear();
  gpu_skinning_data.view_to_object_transforms.clear();

  // TODO: remove, this is debugging
  gpu_skinning_data.indices.clear();
  gpu_skinning_data.indices.reserve(index_data.size());
  gpu_skinning_data.indices.assign(index_data.begin(), index_data.end());

  const auto byte_offset = declaration.position.offset;
  auto vert_ptr = reinterpret_cast<const u8*>(draw_data.vertex_data.data());
  for (std::size_t vert_index = 0; vert_index < draw_data.vertex_data.size(); vert_index++)
  {
    u32 gpu_skin_index;
    std::memcpy(&gpu_skin_index, vert_ptr + declaration.posmtx.offset, sizeof(u32));

    Common::Matrix44 position_transform = Common::Matrix44::Identity();
    if (apply_gpu_skinning)
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
    }

    Common::Vec3 vertex_position;
    std::memcpy(&vertex_position.x, vert_ptr + byte_offset, sizeof(float));
    std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + byte_offset, sizeof(float));
    std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + byte_offset, sizeof(float));
    gpu_skinning_data.positions.push_back(position_transform.Transform(vertex_position, 1));

    if (apply_gpu_skinning)
    {
      position_transform = position_transform.Inverted();
    }

    gpu_skinning_data.view_to_object_transforms.push_back(std::move(position_transform));
    gpu_skinning_data.skinning_index_per_vertex.push_back(gpu_skin_index);

    vert_ptr += draw_data.vertex_format->GetVertexStride();
  }
}
}  // namespace GraphicsModEditor
