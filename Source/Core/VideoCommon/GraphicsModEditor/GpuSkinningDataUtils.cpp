// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"

#include <optional>
#include <set>
#include <vector>

#include "imgui/../../eigen/eigen/Eigen/Dense"

#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Matrix.h"

namespace GraphicsModEditor
{
namespace
{
struct CompareVec3
{
  bool operator()(const Common::Vec3& lhs, const Common::Vec3& rhs) const
  {
    if (lhs.x != rhs.x)
      return lhs.x < rhs.x;
    if (lhs.y != rhs.y)
      return lhs.y < rhs.y;
    return lhs.z < rhs.z;
  }
};

Common::Vec3 closestPointTriangle(const Common::Vec3& p, const Common::Vec3& a,
                                  const Common::Vec3& b, const Common::Vec3& c)
{
  const Common::Vec3 ab = b - a;
  const Common::Vec3 ac = c - a;
  const Common::Vec3 ap = p - a;

  const float d1 = ab.Dot(ap);
  const float d2 = ac.Dot(ap);
  if (d1 <= 0.f && d2 <= 0.f)
    return a;

  const Common::Vec3 bp = p - b;
  const float d3 = ab.Dot(bp);
  const float d4 = ac.Dot(bp);
  if (d3 >= 0.f && d4 <= d3)
    return b;

  const Common::Vec3 cp = p - c;
  const float d5 = ab.Dot(cp);
  const float d6 = ac.Dot(cp);
  if (d6 >= 0.f && d5 <= d6)
    return c;

  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
  {
    const float v = d1 / (d1 - d3);
    return a + ab * v;
  }

  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
  {
    const float v = d2 / (d2 - d6);
    return a + ac * v;
  }

  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
  {
    const float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return b + (c - b) * v;
  }

  const float denom = 1.f / (va + vb + vc);
  const float v = vb * denom;
  const float w = vc * denom;
  return a + ab * v + ac * w;
}

std::vector<float> GetClosestDistancePerPosition(
    const Common::Vec3& reference_position, std::span<const Common::Vec3> positions,
    const std::set<Common::Vec3, CompareVec3>& vertex_positions_duplicated)
{
  std::vector<float> result;
  result.reserve(positions.size());
  for (std::size_t i = 0; i < positions.size(); i++)
  {
    const auto& position = positions[i];
    if (vertex_positions_duplicated.find(position) != vertex_positions_duplicated.end())
    {
      result.push_back(std::numeric_limits<float>::max());
    }
    else
    {
      const float distance = (reference_position - position).LengthSquared();
      result.push_back(distance);
    }
  }
  return result;
}

GraphicsModSystem::DrawCallID
GetClosestDrawCall(const Common::Vec3& v0, const Common::Vec3& v1, const Common::Vec3& v2,
                   const NativeMeshData& native_mesh_data,
                   const std::set<Common::Vec3, CompareVec3>& vertex_positions_duplicated)
{
  std::map<GraphicsModSystem::DrawCallID, float> draw_call_to_distance;
  for (const auto& [draw_call, draw_data] : native_mesh_data.m_draw_call_to_chunk)
  {
    const auto [iter, inserted] = draw_call_to_distance.try_emplace(draw_call, 0.0f);

    const auto v0_lookup = GetClosestDistancePerPosition(v0, draw_data.positions_transformed,
                                                         vertex_positions_duplicated);
    const auto v1_lookup = GetClosestDistancePerPosition(v1, draw_data.positions_transformed,
                                                         vertex_positions_duplicated);
    const auto v2_lookup = GetClosestDistancePerPosition(v2, draw_data.positions_transformed,
                                                         vertex_positions_duplicated);

    float smallest = std::numeric_limits<float>::max();
    /*float smallest_v0 = std::numeric_limits<float>::max();
    float smallest_v1 = std::numeric_limits<float>::max();
    float smallest_v2 = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < draw_data.positions_transformed.size(); index++)
    {
      const auto distance_v0 = v0_lookup[index];
      if (distance_v0 < smallest_v0)
        smallest_v0 = distance_v0;

      const auto distance_v1 = v1_lookup[index];
      if (distance_v1 < smallest_v1)
        smallest_v1 = distance_v1;

      const auto distance_v2 = v2_lookup[index];
      if (distance_v2 < smallest_v2)
        smallest_v2 = distance_v2;
    }
    smallest = smallest_v0 + smallest_v1 + smallest_v2;*/

    /*for (std::size_t index = 0; index < draw_data.positions_transformed.size(); index++)
    {
      const auto distance = v0_lookup[index] + v1_lookup[index] + v2_lookup[index];
      if (distance < smallest)
        smallest = distance;
    }*/

    if (draw_data.primitive_type == PrimitiveType::TriangleStrip)
    {
      u32 strip_count = 0;
      for (std::size_t i = 0; i < draw_data.index_data.size(); i++)
      {
        // Primitive restart
        if (draw_data.index_data[i] == UINT16_MAX)
        {
          strip_count = 0;  // Reset strip state
          continue;
        }

        // A triangle is at least 3 verts
        strip_count++;
        if (strip_count < 3)
          continue;

        u16 i0, i1, i2;

        // Triangle index 'n' in the current strip is at (i-2, i-1, i)
        // Its local index is (triangleCountInStrip - 3)
        if ((strip_count - 3) % 2 == 0)
        {
          i0 = draw_data.index_data[i - 2];
          i1 = draw_data.index_data[i - 1];
          i2 = draw_data.index_data[i];
        }
        else
        {
          // Swap for odd-numbered triangles to maintain consistent CCW winding
          i0 = draw_data.index_data[i - 2];
          i1 = draw_data.index_data[i];
          i2 = draw_data.index_data[i - 1];
        }

        if (i0 == i1 || i1 == i2 || i0 == i2)
        {
          continue;
        }

        /*const auto distance = v0_lookup[i0] + v1_lookup[i1] + v2_lookup[i2];
        if (distance < smallest)
          smallest = distance;*/

        float weight = v0_lookup[i0] + v1_lookup[i1] + v2_lookup[i2];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i0] + v1_lookup[i2] + v2_lookup[i1];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i1] + v1_lookup[i0] + v2_lookup[i2];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i1] + v1_lookup[i2] + v2_lookup[i0];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i2] + v1_lookup[i0] + v2_lookup[i1];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i2] + v1_lookup[i1] + v2_lookup[i0];
        if (weight < smallest)
        {
          smallest = weight;
        }
      }
    }
    else
    {
      for (std::size_t indice_index = 0; indice_index < draw_data.index_data.size();
           indice_index += 3)
      {
        const auto i0 = draw_data.index_data[indice_index + 0];
        const auto i1 = draw_data.index_data[indice_index + 1];
        const auto i2 = draw_data.index_data[indice_index + 2];

        float weight = v0_lookup[i0] + v1_lookup[i1] + v2_lookup[i2];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i0] + v1_lookup[i2] + v2_lookup[i1];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i1] + v1_lookup[i0] + v2_lookup[i2];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i1] + v1_lookup[i2] + v2_lookup[i0];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i2] + v1_lookup[i0] + v2_lookup[i1];
        if (weight < smallest)
        {
          smallest = weight;
        }

        weight = v0_lookup[i2] + v1_lookup[i1] + v2_lookup[i0];
        if (weight < smallest)
        {
          smallest = weight;
        }
      }
    }
    iter->second = smallest;
  }

  const auto chosen =
      std::min_element(draw_call_to_distance.begin(), draw_call_to_distance.end(),
                       [](const auto& l, const auto& r) { return l.second < r.second; });
  return chosen == draw_call_to_distance.end() ? GraphicsModSystem::DrawCallID::INVALID :
                                                 chosen->first;
}

struct OriginalVertexData
{
  u32 buffer_index;
  const NativeMeshData::NativeMeshChunkForDrawCall* draw_call_data;
};
/*std::optional<OriginalPointData>
GetNearestOriginalMeshData(const Common::Vec3& reference_position, GraphicsModSystem::DrawCallID
draw_call_chosen, const GpuSkinningData& gpu_skinning_data)
{
  struct ClosestPointData
  {
    GraphicsModSystem::DrawCallID draw_call_id;
    std::size_t triangle_start_index;
    std::size_t point_index;
    const GpuSkinningData::GpuSkinningForDrawCall* draw_data;
  };
  std::vector<ClosestPointData> closest_points_data;
  std::optional<float> nearest;
  for (const auto& [draw_call, draw_data] : gpu_skinning_data.m_draw_call_to_gpu_skinning)
  {
    for (std::size_t starting_index = 0; starting_index < draw_data.indices.size();
         starting_index += 3)
    {
      const auto orig_mesh_start_index = draw_data.indices[starting_index];
      for (std::size_t i = 0; i < 3; i++)
      {
        const auto indice_index = starting_index + i;
        const auto orig_mesh_index = draw_data.indices[indice_index];
        const auto& original_position = draw_data.positions[orig_mesh_index];
        const float distance = (new_position - original_position).LengthSquared();
        if (!nearest || distance < nearest)
        {
          closest_points_data.clear();
          nearest = distance;
          ClosestPointData closest_point_data;
          closest_point_data.draw_call_id = draw_call;
          closest_point_data.triangle_start_index = orig_mesh_start_index;
          closest_point_data.point_index = orig_mesh_index;
          closest_point_data.draw_data = &draw_data;
          closest_points_data.push_back(std::move(closest_point_data));
        }
        else if (distance == nearest)
        {
          ClosestPointData closest_point_data;
          closest_point_data.draw_call_id = draw_call;
          closest_point_data.triangle_start_index = orig_mesh_start_index;
          closest_point_data.point_index = orig_mesh_index;
          closest_point_data.draw_data = &draw_data;
          closest_points_data.push_back(std::move(closest_point_data));
        }
      }
    }
  }

  if (!nearest)
    return std::nullopt;

  // We only have one point's data, return it
  if (closest_points_data.size() == 1)
  {
    auto& draw_data = *closest_points_data[0].draw_data;
    auto index = closest_points_data[0].point_index;

    OriginalPointData result;
    result.draw_call_id = closest_points_data[0].draw_call_id;
    result.transform = &draw_data.view_to_object_transforms[index];
    result.skinning_index = draw_data.skinning_index_per_vertex[index];
    return result;
  }

  // In the case of a tie, filter the triangles by looking at the other points in the
  // reference mesh triangle
  const auto get_closest_point = [](const Common::Vec3& new_position,
                                    const std::vector<ClosestPointData>& closest_points_data) {
    std::vector<ClosestPointData> closest_points_data_result;
    std::optional<float> nearest;
    for (const auto& closest_point : closest_points_data)
    {
      for (std::size_t i = 0; i < 3; i++)
      {
        const std::size_t triangle_index = i + closest_point.triangle_start_index;
        if (triangle_index == closest_point.point_index)
          continue;
        const auto& original_position = closest_point.draw_data->positions[triangle_index];
        const float distance = (new_position - original_position).LengthSquared();
        if (!nearest || distance < nearest)
        {
          nearest = distance;
          closest_points_data_result.clear();
          closest_points_data_result.push_back(closest_point);
        }
        else if (distance == nearest)
        {
          closest_points_data_result.push_back(closest_point);
        }
      }
    }
    return closest_points_data_result;
  };

  auto filtered_0 = get_closest_point(weight_0, closest_points_data);
  auto filtered_1 = get_closest_point(weight_1, filtered_0);

  if (filtered_1.empty())
    return std::nullopt;

  auto& draw_data = *filtered_1[0].draw_data;
  auto index = filtered_1[0].point_index;

  OriginalPointData result;
  result.draw_call_id = filtered_1[0].draw_call_id;
  result.transform = &draw_data.view_to_object_transforms[index];
  result.skinning_index = draw_data.skinning_index_per_vertex[index];
  return result;

  // In the case of a tie between points...
  // pick the point closest to the others on the triangle
  nearest = std::nullopt;
  std::size_t chosen_index = 0;
  const GpuSkinningData::GpuSkinningForDrawCall* chosen_draw_data = nullptr;
  GraphicsModSystem::DrawCallID chosen_draw_call = GraphicsModSystem::DrawCallID::INVALID;
  for (const auto& closest_point_data : closest_points_data)
  {
    const auto draw_data = closest_point_data.draw_data;
    const auto triangle_start_index = closest_point_data.triangle_start_index;
    const auto closest_pt = closestPointTriangle(new_position,
  draw_data->positions[triangle_start_index], draw_data->positions[triangle_start_index + 1],
                         draw_data->positions[triangle_start_index + 2]);
    const float distance = (-closest_pt).LengthSquared();
    if (!nearest || distance < nearest)
    {
      nearest = distance;
      chosen_index = closest_point_data.point_index;
      chosen_draw_data = closest_point_data.draw_data;
      chosen_draw_call = closest_point_data.draw_call_id;
    }
  }

  OriginalPointData result;
  result.draw_call_id = chosen_draw_call;
  result.transform = &chosen_draw_data->view_to_object_transforms[chosen_index];
  result.skinning_index = chosen_draw_data->skinning_index_per_vertex[chosen_index];
  return result;
  return std::nullopt;
}*/

std::optional<u16> GetNearestOriginalVertexIndex(const Common::Vec3& reference_position,
                                                 GraphicsModSystem::DrawCallID draw_call_chosen,
                                                 const NativeMeshData& native_mesh_data)
{
  u16 result;
  if (const auto draw_data = native_mesh_data.m_draw_call_to_chunk.find(draw_call_chosen);
      draw_data != native_mesh_data.m_draw_call_to_chunk.end())
  {
    std::optional<float> nearest;
    for (std::size_t i = 0; i < draw_data->second.positions_transformed.size(); i++)
    {
      const auto& position = draw_data->second.positions_transformed[i];
      const float distance = (reference_position - position).LengthSquared();
      if (!nearest || distance < nearest)
      {
        result = static_cast<u16>(i);
        nearest = distance;
      }
    }
    return result;
  }
  return std::nullopt;
}

Common::Vec3 ReadVertexPosition(const VideoCommon::MeshDataChunk& mesh_chunk, u16 index)
{
  const auto position_offset = mesh_chunk.vertex_declaration.position.offset;
  const auto vert_ptr =
      reinterpret_cast<const u8*>(mesh_chunk.vertex_data.get()) + index * mesh_chunk.vertex_stride;
  Common::Vec3 vertex_position;
  std::memcpy(&vertex_position.x, vert_ptr + position_offset, sizeof(float));
  std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + position_offset, sizeof(float));
  std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + position_offset, sizeof(float));
  return vertex_position;
}

void WriteVertexPosition(VideoCommon::MeshDataChunk& mesh_chunk,
                         const Common::Vec3& vertex_position, u16 index)
{
  const auto position_offset = mesh_chunk.vertex_declaration.position.offset;
  const auto vert_ptr =
      reinterpret_cast<u8*>(mesh_chunk.vertex_data.get()) + index * mesh_chunk.vertex_stride;

  std::memcpy(vert_ptr + position_offset, &vertex_position.x, sizeof(float));
  std::memcpy(vert_ptr + position_offset + sizeof(float), &vertex_position.y, sizeof(float));
  std::memcpy(vert_ptr + position_offset + sizeof(float) * 2, &vertex_position.z, sizeof(float));
}

Common::Vec3 ReadVertexNormal(const VideoCommon::MeshDataChunk& mesh_chunk, u16 index)
{
  const auto offset = mesh_chunk.vertex_declaration.normals[0].offset;
  const auto vert_ptr =
      reinterpret_cast<const u8*>(mesh_chunk.vertex_data.get()) + index * mesh_chunk.vertex_stride;
  Common::Vec3 vertex_position;
  std::memcpy(&vertex_position.x, vert_ptr + offset, sizeof(float));
  std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + offset, sizeof(float));
  std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + offset, sizeof(float));
  return vertex_position;
}

void WriteVertexNormal(VideoCommon::MeshDataChunk& mesh_chunk, const Common::Vec3& vertex_normal,
                       u16 index)
{
  const auto offset = mesh_chunk.vertex_declaration.normals[0].offset;
  const auto vert_ptr =
      reinterpret_cast<u8*>(mesh_chunk.vertex_data.get()) + index * mesh_chunk.vertex_stride;

  std::memcpy(vert_ptr + offset, &vertex_normal.x, sizeof(float));
  std::memcpy(vert_ptr + offset + sizeof(float), &vertex_normal.y, sizeof(float));
  std::memcpy(vert_ptr + offset + sizeof(float) * 2, &vertex_normal.z, sizeof(float));
}

void WriteIndiceIndex(VideoCommon::MeshDataChunk& mesh_chunk, u16 indice_index)
{
  const auto index_ptr = reinterpret_cast<u16*>(mesh_chunk.indices.get()) + indice_index;

  std::memcpy(index_ptr, &indice_index, sizeof(u16));
}

u32 GetSizeOfAttribute(const AttributeFormat& attribute_format)
{
  switch (attribute_format.type)
  {
  case ComponentFormat::UByte:
  case ComponentFormat::Byte:
    return static_cast<u32>(attribute_format.components);
  case ComponentFormat::UShort:
  case ComponentFormat::Short:
    return static_cast<u32>(attribute_format.components * sizeof(short));
  case ComponentFormat::Float:
    return static_cast<u32>(attribute_format.components * sizeof(float));
  case ComponentFormat::InvalidFloat5:
  case ComponentFormat::InvalidFloat6:
  case ComponentFormat::InvalidFloat7:
  default:
    return 0;
  };
}

void CopyNativeMeshData(VideoCommon::MeshDataChunk& replacement_mesh_chunk, u32 replacement_index,
                        const AttributeFormat& replacement_attribute_format,
                        const NativeMeshData::NativeMeshChunkForDrawCall& native_mesh_chunk,
                        u32 native_index, const AttributeFormat& native_attribute_format)
{
  const u32 size = GetSizeOfAttribute(native_attribute_format);

  const auto dest_ptr = reinterpret_cast<u8*>(replacement_mesh_chunk.vertex_data.get()) +
                        replacement_index * replacement_mesh_chunk.vertex_stride;
  const auto src_ptr = native_mesh_chunk.vertex_data.data() +
                       native_index * native_mesh_chunk.vertex_declaration.stride;

  const u32 native_offset = native_attribute_format.offset;
  const u32 replacement_offset = replacement_attribute_format.offset;
  std::memcpy(dest_ptr + replacement_offset, src_ptr + native_offset, size);
}

void CopyReplacementData(VideoCommon::MeshDataChunk& final_mesh_chunk, u16 final_mesh_index,
                         const VideoCommon::MeshDataChunk& replacement_mesh_chunk,
                         u16 replacement_index)
{
  const auto dest_ptr = reinterpret_cast<u8*>(final_mesh_chunk.vertex_data.get()) +
                        final_mesh_index * final_mesh_chunk.vertex_stride;
  const auto src_ptr = reinterpret_cast<u8*>(replacement_mesh_chunk.vertex_data.get()) +
                       replacement_index * replacement_mesh_chunk.vertex_stride;

  std::memcpy(dest_ptr, src_ptr, replacement_mesh_chunk.vertex_stride);
}

void UpdateDeclaration(VideoCommon::MeshDataChunk* final_mesh_chunk, u32 component_type,
                       const AttributeFormat& attribute_format)
{
  const auto update_stride = [&] {
    const u32 size = GetSizeOfAttribute(attribute_format);

    const u32 old_stride = final_mesh_chunk->vertex_stride;
    final_mesh_chunk->vertex_stride = old_stride + size;
    final_mesh_chunk->vertex_declaration.stride = old_stride + size;
  };

  const auto update_declaration = [&](AttributeFormat* format_to_update, u32 offset) {
    format_to_update->components = attribute_format.components;
    format_to_update->enable = true;
    format_to_update->integer = attribute_format.integer;
    format_to_update->offset = offset;
    format_to_update->type = attribute_format.type;
  };

  if (component_type == VB_HAS_POSMTXIDX)
  {
    auto& attribute = final_mesh_chunk->vertex_declaration.posmtx;

    // If the attribute is already enabled, we don't need to update it
    if (attribute.enable)
      return;

    const u32 old_stride = final_mesh_chunk->vertex_stride;
    update_stride();
    update_declaration(&attribute, old_stride);
  }

  for (std::size_t i = 0; i < final_mesh_chunk->vertex_declaration.colors.size(); i++)
  {
    const auto component_to_compare = VB_HAS_COL0 << i;
    if (component_to_compare == component_type)
    {
      auto& attribute = final_mesh_chunk->vertex_declaration.colors[i];

      // If the attribute is already enabled, we don't need to update it
      if (attribute.enable)
        return;

      const u32 old_stride = final_mesh_chunk->vertex_stride;
      update_stride();
      update_declaration(&attribute, old_stride);
    }
  }

  for (std::size_t i = 0; i < final_mesh_chunk->vertex_declaration.normals.size(); i++)
  {
    const auto component_to_compare = VB_HAS_NORMAL << i;
    if (component_to_compare == component_type)
    {
      auto& attribute = final_mesh_chunk->vertex_declaration.normals[i];

      // If the attribute is already enabled, we don't need to update it
      if (attribute.enable)
        return;

      const u32 old_stride = final_mesh_chunk->vertex_stride;
      update_stride();
      update_declaration(&attribute, old_stride);
    }
  }

  for (std::size_t i = 0; i < final_mesh_chunk->vertex_declaration.texcoords.size(); i++)
  {
    const auto component_to_compare = VB_HAS_UV0 << i;
    if (component_to_compare == component_type)
    {
      auto& attribute = final_mesh_chunk->vertex_declaration.texcoords[i];

      // If the attribute is already enabled, we don't need to update it
      if (attribute.enable)
        return;

      const u32 old_stride = final_mesh_chunk->vertex_stride;
      update_stride();
      update_declaration(&attribute, old_stride);
    }
  }

  final_mesh_chunk->components_available |= component_type;
}

void CalculateMeshData(const NativeMeshData& native_mesh_data,
                       const VideoCommon::MeshData& reference_mesh_data, u32 components_to_copy,
                       const VertexGroupPerDrawCall& cpu_vertex_groups,
                       VideoCommon::MeshData* final_mesh_data)
{
  std::set<Common::Vec3, CompareVec3> vertex_positions_duplicated;

  struct IntermediateMeshChunkData
  {
    struct IndexPair
    {
      u16 replacement_index;
      u16 replacement_origin_index;
      u16 native_index;
    };
    std::vector<IndexPair> indexes;
    const NativeMeshData::NativeMeshChunkForDrawCall* draw_call_data;
  };
  using MeshVertexData = std::pair<u16, OriginalVertexData>;
  std::map<GraphicsModSystem::DrawCallID, std::map<std::size_t, IntermediateMeshChunkData>>
      draw_call_to_chunk_to_mesh_data;

  for (std::size_t chunk_index = 0; chunk_index < reference_mesh_data.m_mesh_chunks.size();
       chunk_index++)
  {
    const auto& reference_chunk = reference_mesh_data.m_mesh_chunks[chunk_index];

    // TODO: handle other primitive types
    if (reference_chunk.primitive_type != PrimitiveType::Triangles)
      return;

    /*auto& new_mesh_chunk = calculated_mesh_data.m_mesh_chunks[chunk_index];

    // Tell our vertex declaration we have skinning data
    new_mesh_chunk.vertex_declaration.posmtx.components = 4;
    new_mesh_chunk.vertex_declaration.posmtx.enable = true;
    new_mesh_chunk.vertex_declaration.posmtx.integer = true;
    new_mesh_chunk.vertex_declaration.posmtx.offset = 0;
    new_mesh_chunk.vertex_declaration.posmtx.type = ComponentFormat::UByte;
    new_mesh_chunk.components_available |= VB_HAS_POSMTXIDX;*/

    // Now update our data and calculate the indices per draw call
    u32 indice_index = 0;
    while (indice_index < reference_chunk.num_indices)
    {
      std::vector<std::pair<u16, OriginalVertexData>> index_original_mesh_data_pairs;
      // std::map<GraphicsModSystem::DrawCallID, int> draw_call_to_count;

      // Read the position from our mesh
      const auto i0 = reference_chunk.indices[indice_index + 0];
      const auto v0 = ReadVertexPosition(reference_chunk, i0);

      const auto i1 = reference_chunk.indices[indice_index + 1];
      const auto v1 = ReadVertexPosition(reference_chunk, i1);

      const auto i2 = reference_chunk.indices[indice_index + 2];
      const auto v2 = ReadVertexPosition(reference_chunk, i2);

      // Transform the vertex position so that we can compare it against
      // our original mesh
      const auto v0_transformed = reference_chunk.transform.Transform(v0, 1);
      const auto v1_transformed = reference_chunk.transform.Transform(v1, 1);
      const auto v2_transformed = reference_chunk.transform.Transform(v2, 1);

      indice_index += 3;

      const auto draw_call = GetClosestDrawCall(v0_transformed, v1_transformed, v2_transformed,
                                                native_mesh_data, vertex_positions_duplicated);
      if (draw_call == GraphicsModSystem::DrawCallID::INVALID)
      {
        ERROR_LOG_FMT(VIDEO, "Draw call invalid?");
        continue;
      }

      auto& chunk_index_to_intermediate_data = draw_call_to_chunk_to_mesh_data[draw_call];
      auto& intermediate_chunk_data = chunk_index_to_intermediate_data[chunk_index];

      if (const auto native_chunk_iter = native_mesh_data.m_draw_call_to_chunk.find(draw_call);
          native_chunk_iter != native_mesh_data.m_draw_call_to_chunk.end())
      {
        intermediate_chunk_data.draw_call_data = &native_chunk_iter->second;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Draw call not found?");
        continue;
      }

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v0_transformed, draw_call, native_mesh_data))
      {
        intermediate_chunk_data.indexes.emplace_back(i0, i0, *original_vertex_index);
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 0 not found?");
      }

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v1_transformed, draw_call, native_mesh_data))
      {
        intermediate_chunk_data.indexes.emplace_back(i1, i0, *original_vertex_index);
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 1 not found?");
      }

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v2_transformed, draw_call, native_mesh_data))
      {
        intermediate_chunk_data.indexes.emplace_back(i2, i0, *original_vertex_index);
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 2 not found?");
      }

      /*if (const auto original_mesh_data = GetNearestOriginalMeshData(
              vertex_position_0, vertex_position_1, vertex_position_2, gpu_skinning_data))
      {
        draw_call_to_count[original_mesh_data->draw_call_id]++;
        index_original_mesh_data_pairs.emplace_back(reference_index_0, *original_mesh_data);
      }

      if (const auto original_mesh_data = GetNearestOriginalMeshData(
              vertex_position_1, vertex_position_0, vertex_position_2, gpu_skinning_data))
      {
        draw_call_to_count[original_mesh_data->draw_call_id]++;
        index_original_mesh_data_pairs.emplace_back(reference_index_1, *original_mesh_data);
      }

      if (const auto original_mesh_data = GetNearestOriginalMeshData(
              vertex_position_2, vertex_position_0, vertex_position_1, gpu_skinning_data))
      {
        draw_call_to_count[original_mesh_data->draw_call_id]++;
        index_original_mesh_data_pairs.emplace_back(reference_index_2, *original_mesh_data);
      }*/

      /*const auto max_draw_call_pair = std::max_element(
          draw_call_to_count.begin(), draw_call_to_count.end(),
          [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });*/

      /*auto& skinning_chunks_per_draw_call =
          calculated_mesh_data.m_gpu_skinning_chunks[max_draw_call_pair->first];
      if (skinning_chunks_per_draw_call.size() < reference_mesh_data.m_mesh_chunks.size())
        skinning_chunks_per_draw_call.resize(reference_mesh_data.m_mesh_chunks.size());
      auto& skinning_chunk = skinning_chunks_per_draw_call[chunk_index];*/

      /*auto& skinning_chunks_per_draw_call =
          calculated_mesh_data.m_gpu_skinning_chunks[draw_call];
      if (skinning_chunks_per_draw_call.size() < reference_mesh_data.m_mesh_chunks.size())
        skinning_chunks_per_draw_call.resize(reference_mesh_data.m_mesh_chunks.size());
      auto& skinning_chunk = skinning_chunks_per_draw_call[chunk_index];

      auto& skinning_chunk_data = draw_call_to_chunk_to_mesh_data[draw_call];

      for (const auto& [index, original_mesh_data] : index_original_mesh_data_pairs)
      {
        skinning_chunk_data skinning_chunk.components_available =
            reference_chunk.components_available;
        skinning_chunk.indices =
            std::make_unique<u16[]>(reference_chunk.) skinning_chunk.indices.push_back(index);

        if (indices_seen.insert(index).second)
        {
          WriteGpuSkinningIndex(new_mesh_chunk, original_mesh_data.skinning_index, index);
          if (original_mesh_data.transform->data != Common::Matrix44::Identity().data)
          {
            // Get existing vertex position
            const auto new_vertex_position = ReadVertexPosition(new_mesh_chunk, index);

            // Apply the transform to it
            const auto new_vertex_position_transformed =
                original_mesh_data.transform->Transform(new_vertex_position, 1);

            // Now write that back to the data
            WriteVertexPosition(new_mesh_chunk, new_vertex_position_transformed, index);
          }
          indices_to_point_data[index] = original_mesh_data;
        }
        else
        {
          auto& previous_point_data = indices_to_point_data[index];
          if (previous_point_data.draw_call_id != original_mesh_data.draw_call_id ||
              previous_point_data.skinning_index != original_mesh_data.skinning_index)
          {
            ERROR_LOG_FMT(VIDEO,
                          "Saw duplicate indice '{}' with different data, previous indice: "
                          "[draw={}, skin_idx={}], new "
                          "indice: [draw={}, skin_idx={}]",
                          index, Common::ToUnderlying(previous_point_data.draw_call_id),
                          previous_point_data.skinning_index,
                          Common::ToUnderlying(original_mesh_data.draw_call_id),
                          original_mesh_data.skinning_index);
          }
        }
      }*/
    }
  }

  // If we are CPU skinning, we need to compute (per draw call)
  // some data
  if (!cpu_vertex_groups.empty())
  {
    for (const auto& [draw_call, chunk] : native_mesh_data.m_draw_call_to_chunk)
    {
      const auto cpu_vertex_group_iter = cpu_vertex_groups.find(draw_call);
      if (cpu_vertex_group_iter == cpu_vertex_groups.end())
        continue;

      auto& cpu_skinning_data = final_mesh_data->m_cpu_skinning_data[draw_call];
      cpu_skinning_data.native_mesh_vertex_groups = cpu_vertex_group_iter->second;

      for (std::size_t group_index = 0;
           group_index < cpu_skinning_data.native_mesh_vertex_groups.size(); group_index++)
      {
        const auto& group = cpu_skinning_data.native_mesh_vertex_groups[group_index];
        Eigen::MatrixXf native_points(3, group.size());

        for (std::size_t i = 0; i < group.size(); i++)
        {
          const int native_index = group[i];
          const auto& inverse_position_transform = chunk.position_inverse_transforms[native_index];
          const auto native_position_object_space =
              inverse_position_transform.Transform(chunk.positions_transformed[native_index], 1);
          native_points.col(i) =
              Eigen::Vector3f(native_position_object_space.x, native_position_object_space.y,
                              native_position_object_space.z);
        }

        VideoCommon::CPUSkinningData::DataPerGroup group_data;
        Eigen::Vector3f native_centroid = native_points.rowwise().mean();
        Eigen::MatrixXf native_points_centered = native_points.colwise() - native_centroid;
        group_data.centroid =
            Common::Vec3(native_centroid.x(), native_centroid.y(), native_centroid.z());
        for (std::size_t i = 0; i < group.size(); i++)
        {
          const Eigen::Vector3f centered_pt = native_points_centered.col(i);
          group_data.delta_positions_from_centroid.emplace_back(centered_pt.x(), centered_pt.y(),
                                                                centered_pt.z());
        }
        cpu_skinning_data.native_mesh_group_data.push_back(std::move(group_data));
      }
    }
  }

  // Copy our replacement mesh data into our skinning chunks
  for (const auto& [draw_call, chunk_to_mesh_data] : draw_call_to_chunk_to_mesh_data)
  {
    auto& per_draw_replacement_chunk_list = final_mesh_data->m_skinning_chunks[draw_call];

    per_draw_replacement_chunk_list.reserve(chunk_to_mesh_data.size());
    for (const auto& [chunk_index, intermediate_data] : chunk_to_mesh_data)
    {
      const auto& replacement_chunk = final_mesh_data->m_mesh_chunks[chunk_index];
      auto& final_replacement_chunk = per_draw_replacement_chunk_list.emplace_back();
      final_replacement_chunk.components_available = replacement_chunk.components_available;
      final_replacement_chunk.material_name = replacement_chunk.material_name;
      final_replacement_chunk.primitive_type = replacement_chunk.primitive_type;
      final_replacement_chunk.transform = replacement_chunk.transform;
      final_replacement_chunk.vertex_declaration = replacement_chunk.vertex_declaration;
      final_replacement_chunk.vertex_stride = replacement_chunk.vertex_declaration.stride;

      auto& native_vertex_declaration = intermediate_data.draw_call_data->vertex_declaration;

      {
        const u32 component_type = VB_HAS_POSMTXIDX;
        if (native_vertex_declaration.posmtx.enable && components_to_copy & component_type)
        {
          UpdateDeclaration(&final_replacement_chunk, component_type,
                            native_vertex_declaration.posmtx);
        }
        else if (!cpu_vertex_groups.empty())
        {
          // Update our declaration to account for adding an index to the vertices
          // this is to avoid having to have a separate buffer...
          AttributeFormat custom_index;
          custom_index.type = ComponentFormat::UByte;
          custom_index.components = 4;
          custom_index.integer = false;
          UpdateDeclaration(&final_replacement_chunk, component_type, custom_index);

          // Disable the chunk to avoid using it in the final rendering
          final_replacement_chunk.vertex_declaration.posmtx.enable = false;
          final_replacement_chunk.components_available &= ~component_type;
        }
      }

      for (std::size_t i = 0; i < native_vertex_declaration.colors.size(); i++)
      {
        auto& native_color_declaration = native_vertex_declaration.colors[i];
        auto& replacement_color_declaration = final_replacement_chunk.vertex_declaration.colors[i];
        const u32 component_type = VB_HAS_COL0 << i;
        if (native_color_declaration.enable && !replacement_color_declaration.enable &&
            components_to_copy & component_type)
        {
          if (native_color_declaration.type != replacement_color_declaration.type)
          {
            ERROR_LOG_FMT(VIDEO,
                          "Can't update declaration for component Color{}, type of src does not "
                          "match destination",
                          i);
            continue;
          }
          else
          {
            UpdateDeclaration(&final_replacement_chunk, component_type, native_color_declaration);
          }
        }
      }

      for (std::size_t i = 0; i < native_vertex_declaration.normals.size(); i++)
      {
        auto& native_normal_declaration = native_vertex_declaration.normals[i];
        auto& replacement_normal_declaration =
            final_replacement_chunk.vertex_declaration.normals[i];
        const u32 component_type = VB_HAS_NORMAL << i;
        if (native_normal_declaration.enable && !replacement_normal_declaration.enable &&
            components_to_copy & component_type)
        {
          if (native_normal_declaration.type != replacement_normal_declaration.type)
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Can't update declaration for Normal{}, type of src does not match destination", i);
            continue;
          }
          else
          {
            UpdateDeclaration(&final_replacement_chunk, component_type, native_normal_declaration);
          }
        }
      }

      for (std::size_t i = 0; i < native_vertex_declaration.texcoords.size(); i++)
      {
        auto& native_texcoord_declaration = native_vertex_declaration.texcoords[i];
        auto& replacement_texcoord_declaration =
            final_replacement_chunk.vertex_declaration.texcoords[i];
        const u32 component_type = VB_HAS_UV0 << i;
        if (native_texcoord_declaration.enable && !replacement_texcoord_declaration.enable &&
            components_to_copy & component_type)
        {
          if (native_texcoord_declaration.type != replacement_texcoord_declaration.type)
          {
            ERROR_LOG_FMT(
                VIDEO, "Can't update declaration for UV{}, type of src does not match destination",
                i);
            continue;
          }
          else
          {
            UpdateDeclaration(&final_replacement_chunk, component_type,
                              native_texcoord_declaration);
          }
        }
      }

      // Create index data
      final_replacement_chunk.indices = std::make_unique<u16[]>(intermediate_data.indexes.size());
      final_replacement_chunk.num_indices = static_cast<u32>(intermediate_data.indexes.size());

      // Create vertex data
      final_replacement_chunk.vertex_data = std::make_unique<u8[]>(
          intermediate_data.indexes.size() * final_replacement_chunk.vertex_stride);
      final_replacement_chunk.num_vertices = static_cast<u32>(intermediate_data.indexes.size());

      u16 next_indice = 0;
      // Now fill out all data...
      for (const auto& [replacement_index, replacement_origin_index, native_index] :
           intermediate_data.indexes)
      {
        CopyReplacementData(final_replacement_chunk, next_indice, replacement_chunk,
                            replacement_index);

        {
          const u32 component_type = VB_HAS_POSMTXIDX;
          if (native_vertex_declaration.posmtx.enable && components_to_copy & component_type)
          {
            CopyNativeMeshData(final_replacement_chunk, next_indice,
                               final_replacement_chunk.vertex_declaration.posmtx,
                               *intermediate_data.draw_call_data, native_index,
                               native_vertex_declaration.posmtx);
          }
        }

        for (std::size_t i = 0; i < native_vertex_declaration.colors.size(); i++)
        {
          auto& native_color_declaration = native_vertex_declaration.colors[i];
          auto& replacement_color_declaration =
              final_replacement_chunk.vertex_declaration.colors[i];
          const u32 component_type = VB_HAS_COL0 << i;
          if (native_color_declaration.enable && !replacement_color_declaration.enable &&
              components_to_copy & component_type)
          {
            if (native_color_declaration.type != replacement_color_declaration.type)
            {
              ERROR_LOG_FMT(
                  VIDEO, "Can't copy component Color{}, type of src does not match destination", i);
              continue;
            }
            else
            {
              CopyNativeMeshData(final_replacement_chunk, next_indice,
                                 replacement_color_declaration, *intermediate_data.draw_call_data,
                                 native_index, native_color_declaration);
            }
          }
        }

        for (std::size_t i = 0; i < native_vertex_declaration.normals.size(); i++)
        {
          auto& native_normal_declaration = native_vertex_declaration.normals[i];
          auto& replacement_normal_declaration =
              final_replacement_chunk.vertex_declaration.normals[i];
          const u32 component_type = VB_HAS_NORMAL << i;
          if (native_normal_declaration.enable && !replacement_normal_declaration.enable &&
              components_to_copy & component_type)
          {
            if (native_normal_declaration.type != replacement_normal_declaration.type)
            {
              ERROR_LOG_FMT(VIDEO,
                            "Can't copy component Normal{}, type of src does not match destination",
                            i);
              continue;
            }
            else
            {
              CopyNativeMeshData(final_replacement_chunk, next_indice,
                                 replacement_normal_declaration, *intermediate_data.draw_call_data,
                                 native_index, native_normal_declaration);
            }
          }
        }

        for (std::size_t i = 0; i < native_vertex_declaration.texcoords.size(); i++)
        {
          auto& native_texcoord_declaration = native_vertex_declaration.texcoords[i];
          auto& replacement_texcoord_declaration =
              final_replacement_chunk.vertex_declaration.texcoords[i];
          const u32 component_type = VB_HAS_UV0 << i;
          if (native_texcoord_declaration.enable && !replacement_texcoord_declaration.enable &&
              components_to_copy & component_type)
          {
            if (native_texcoord_declaration.type != replacement_texcoord_declaration.type)
            {
              ERROR_LOG_FMT(VIDEO,
                            "Can't copy component UV{}, type of src does not match destination", i);
              continue;
            }
            else
            {
              CopyNativeMeshData(
                  final_replacement_chunk, next_indice, replacement_texcoord_declaration,
                  *intermediate_data.draw_call_data, native_index, native_texcoord_declaration);
            }
          }
        }

        if (!cpu_vertex_groups.empty())
        {
          // If CPU skinned, we need to write our native mesh index into the posmatrix index spot
          // This is an optimization to avoid needing to have a separate buffer to do the conversion
          const auto index_offset = final_replacement_chunk.vertex_declaration.posmtx.offset;
          u8* vert_ptr = final_replacement_chunk.vertex_data.get() +
                         next_indice * final_replacement_chunk.vertex_stride;
          const u32 native_index_copied = native_index;
          std::memcpy(vert_ptr + index_offset, &native_index_copied, sizeof(u32));
        }

        auto& inverse_position_transform =
            intermediate_data.draw_call_data->position_inverse_transforms[native_index];
        if (inverse_position_transform.data != Common::Matrix44::Identity().data)
        {
          // Get existing vertex position
          const auto vertex_position = ReadVertexPosition(replacement_chunk, replacement_index);

          // Apply the reference transform to it
          const auto vertex_position_transformed =
              replacement_chunk.transform.Transform(vertex_position, 1);

          // Apply the transform from the original data to the vertex position
          // Remember this is the inverse of the original skinning
          // at the time when the user captured the mesh
          // This should take our new vertex position back into object
          // space (allowing future animation transforms from the game to move the vertex in the
          // right spot!)
          const auto new_vertex_position_transformed =
              inverse_position_transform.Transform(vertex_position_transformed, 1);

          // Now write that back to the data
          WriteVertexPosition(final_replacement_chunk, new_vertex_position_transformed,
                              next_indice);
        }

        if (replacement_chunk.vertex_declaration.normals[0].enable &&
            final_replacement_chunk.vertex_declaration.normals[0].enable)
        {
          auto& inverse_normal_transform =
              intermediate_data.draw_call_data->normal_inverse_transforms[native_index];
          if (inverse_normal_transform.data != Common::Matrix33::Identity().data)
          {
            // Get existing vertex normal
            const auto vertex_normal = ReadVertexNormal(replacement_chunk, replacement_index);

            // Apply the reference transform to it
            const auto vertex_normal_transformed =
                replacement_chunk.transform.Transform(vertex_normal, 0);

            // Apply the transform from the original data to the vertex normal
            // Remember this is the inverse of the original skinning
            // at the time when the user captured the mesh
            // This should take our new vertex normal back into object
            // space (allowing future animation transforms from the game to move the normal in the
            // right direction!)
            const auto new_vertex_normal_transformed =
                inverse_normal_transform * vertex_normal_transformed;

            // Now write that back to the data
            WriteVertexNormal(final_replacement_chunk, new_vertex_normal_transformed.Normalized(),
                              next_indice);
          }
        }

        WriteIndiceIndex(final_replacement_chunk, next_indice);
        next_indice++;
      }
    }
  }

  // We've copied everything out of the original mesh, we can remove it as it isn't
  // used during the gpu skinning process
  final_mesh_data->m_mesh_chunks.clear();

  // Calculate bounding box for draw call
  /*std::optional<Common::Vec3> min_pos;
  std::optional<Common::Vec3> max_pos;
  for (std::size_t i = 0; i < draw_data.positions.size(); i++)
  {
    const auto& position = draw_data.positions[i];
    if (!min_pos)
    {
      min_pos = position;
    }
    else
    {
      if (position.x < min_pos->x)
        min_pos->x = position.x;
      if (position.y < min_pos->y)
        min_pos->y = position.y;
      if (position.z < min_pos->z)
        min_pos->z = position.z;
    }

    if (!max_pos)
    {
      max_pos = position;
    }
    else
    {
      if (position.x > max_pos->x)
        max_pos->x = position.x;
      if (position.y > max_pos->y)
        max_pos->y = position.y;
      if (position.z > max_pos->z)
        max_pos->z = position.z;
    }
  }
  if (!min_pos || !max_pos)
  {
    // TODO: error
    return;
  }
  const auto within_bounds = [&](Common::Vec3 position) -> bool {
    return position.x >= min_pos->x && position.y >= min_pos->y && position.z >= min_pos->z &&
           position.x <= max_pos->x && position.y <= max_pos->y && position.z <= max_pos->z;
  };*/
}
}  // namespace

bool NativeMeshToFile(File::IOFile* file_data, const NativeMeshData& native_mesh_data)
{
  const auto write_chunk = [](File::IOFile* file_data,
                              const NativeMeshData::NativeMeshChunkForDrawCall& chunk) {
    if (!file_data->WriteBytes(&chunk.vertex_declaration, sizeof(PortableVertexDeclaration)))
      return false;

    if (!file_data->WriteBytes(&chunk.primitive_type, sizeof(PrimitiveType)))
      return false;

    const std::size_t num_indices = chunk.index_data.size();
    if (!file_data->WriteBytes(&num_indices, sizeof(std::size_t)))
      return false;
    if (!file_data->WriteBytes(chunk.index_data.data(), num_indices * sizeof(u16)))
      return false;

    const std::size_t num_vertices = chunk.vertex_data.size() / chunk.vertex_declaration.stride;
    if (!file_data->WriteBytes(&num_vertices, sizeof(std::size_t)))
      return false;

    if (!file_data->WriteBytes(chunk.vertex_data.data(), chunk.vertex_data.size()))
    {
      return false;
    }

    if (!file_data->WriteBytes(chunk.positions_transformed.data(),
                               num_vertices * sizeof(Common::Vec3)))
    {
      return false;
    }

    if (!file_data->WriteBytes(chunk.position_inverse_transforms.data(),
                               num_vertices * sizeof(Common::Matrix44)))
    {
      return false;
    }

    if (!file_data->WriteBytes(chunk.normal_inverse_transforms.data(),
                               num_vertices * sizeof(Common::Matrix33)))
    {
      return false;
    }

    return true;
  };

  const std::size_t draw_call_size = native_mesh_data.m_draw_call_to_chunk.size();
  file_data->WriteBytes(&draw_call_size, sizeof(std::size_t));

  for (const auto& [draw_call, chunk] : native_mesh_data.m_draw_call_to_chunk)
  {
    if (!file_data->WriteBytes(&draw_call, sizeof(GraphicsModSystem::DrawCallID)))
      return false;

    if (!write_chunk(file_data, chunk))
      return false;
  }

  return true;
}

bool NativeMeshFromFile(File::IOFile* file_data, NativeMeshData* data)
{
  const auto read_chunk = [](File::IOFile* file_data,
                             NativeMeshData::NativeMeshChunkForDrawCall* mesh_data_chunk) {
    auto& chunk = *mesh_data_chunk;

    if (!file_data->ReadBytes(&chunk.vertex_declaration, sizeof(PortableVertexDeclaration)))
      return false;

    if (!file_data->ReadBytes(&chunk.primitive_type, sizeof(PrimitiveType)))
      return false;

    std::size_t index_size = 0;
    if (!file_data->ReadBytes(&index_size, sizeof(std::size_t)))
      return false;

    mesh_data_chunk->index_data.resize(index_size);
    if (!file_data->ReadBytes(mesh_data_chunk->index_data.data(), index_size * sizeof(u16)))
      return false;

    std::size_t vertex_size = 0;
    if (!file_data->ReadBytes(&vertex_size, sizeof(std::size_t)))
      return false;

    mesh_data_chunk->vertex_data.resize(vertex_size * chunk.vertex_declaration.stride);
    if (!file_data->ReadBytes(mesh_data_chunk->vertex_data.data(),
                              vertex_size * chunk.vertex_declaration.stride))
    {
      return false;
    }

    mesh_data_chunk->positions_transformed.resize(vertex_size);
    if (!file_data->ReadBytes(mesh_data_chunk->positions_transformed.data(),
                              vertex_size * sizeof(Common::Vec3)))
    {
      return false;
    }

    mesh_data_chunk->position_inverse_transforms.resize(vertex_size);
    if (!file_data->ReadBytes(mesh_data_chunk->position_inverse_transforms.data(),
                              vertex_size * sizeof(Common::Matrix44)))
    {
      return false;
    }

    mesh_data_chunk->normal_inverse_transforms.resize(vertex_size);
    if (!file_data->ReadBytes(mesh_data_chunk->normal_inverse_transforms.data(),
                              vertex_size * sizeof(Common::Matrix33)))
    {
      return false;
    }

    return true;
  };
  std::size_t chunk_size = 0;
  file_data->ReadBytes(&chunk_size, sizeof(std::size_t));

  for (std::size_t i = 0; i < chunk_size; i++)
  {
    GraphicsModSystem::DrawCallID draw_call_id;
    if (!file_data->ReadBytes(&draw_call_id, sizeof(GraphicsModSystem::DrawCallID)))
      return false;

    NativeMeshData::NativeMeshChunkForDrawCall chunk;
    if (!read_chunk(file_data, &chunk))
      return false;
    data->m_draw_call_to_chunk.try_emplace(draw_call_id, std::move(chunk));
  }

  return true;
}

void CalculateMeshDataWithSkinning(const NativeMeshData& native_mesh_data,
                                   const VideoCommon::MeshData& reference_mesh_data,
                                   u32 components_to_copy,
                                   const VertexGroupPerDrawCall& cpu_vertex_groups,
                                   VideoCommon::MeshData* final_mesh_data)
{
  CalculateMeshData(native_mesh_data, reference_mesh_data, components_to_copy, cpu_vertex_groups,
                    final_mesh_data);
}
}  // namespace GraphicsModEditor
