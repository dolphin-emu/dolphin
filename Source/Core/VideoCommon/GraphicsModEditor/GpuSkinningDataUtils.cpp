// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"

#include <optional>
#include <vector>

#include "Common/EnumUtils.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Matrix.h"

namespace GraphicsModEditor
{
namespace
{
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

std::vector<float> GetClosestDistancePerPosition(const Common::Vec3& reference_position,
                                                 std::span<const Common::Vec3> positions)
{
  std::vector<float> result;
  result.reserve(positions.size());
  for (std::size_t i = 0; i < positions.size(); i++)
  {
    const auto& position = positions[i];
    const float distance = (reference_position - position).LengthSquared();
    result.push_back(distance);
  }
  return result;
}

GraphicsModSystem::DrawCallID GetClosestDrawCall(const Common::Vec3& v0, const Common::Vec3& v1,
                                                 const Common::Vec3& v2,
                                                 const GpuSkinningData& gpu_skinning_data)
{
  std::map<GraphicsModSystem::DrawCallID, float> draw_call_to_distance;
  for (const auto& [draw_call, draw_data] : gpu_skinning_data.m_draw_call_to_gpu_skinning)
  {
    const auto [iter, inserted] = draw_call_to_distance.try_emplace(draw_call, 0.0f);

    const auto v0_lookup = GetClosestDistancePerPosition(v0, draw_data.positions);
    const auto v1_lookup = GetClosestDistancePerPosition(v1, draw_data.positions);
    const auto v2_lookup = GetClosestDistancePerPosition(v2, draw_data.positions);

    float smallest = std::numeric_limits<float>::max();
    /*float smallest_v0 = std::numeric_limits<float>::max();
    float smallest_v1 = std::numeric_limits<float>::max();
    float smallest_v2 = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < draw_data.positions.size(); index++)
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

    /*for (std::size_t index = 0; index < draw_data.positions.size(); index++)
    {
      const auto distance = v0_lookup[index] + v1_lookup[index] + v2_lookup[index];
      if (distance < smallest)
        smallest = distance;
    }*/

    for (std::size_t indice_index = 0; indice_index < draw_data.indices.size(); indice_index += 3)
    {
      const auto i0 = draw_data.indices[indice_index + 0];
      const auto i1 = draw_data.indices[indice_index + 1];
      const auto i2 = draw_data.indices[indice_index + 2];

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
  u32 skinning_index;
  const Common::Matrix44* transform;
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

std::optional<OriginalVertexData>
GetNearestOriginalVertexData(const Common::Vec3& reference_position,
                             GraphicsModSystem::DrawCallID draw_call_chosen,
                             const GpuSkinningData& gpu_skinning_data)
{
  OriginalVertexData result;
  if (const auto draw_data = gpu_skinning_data.m_draw_call_to_gpu_skinning.find(draw_call_chosen);
      draw_data != gpu_skinning_data.m_draw_call_to_gpu_skinning.end())
  {
    std::optional<float> nearest;
    for (std::size_t i = 0; i < draw_data->second.positions.size(); i++)
    {
      const auto& position = draw_data->second.positions[i];
      const float distance = (reference_position - position).LengthSquared();
      if (!nearest || distance < nearest)
      {
        result.skinning_index = draw_data->second.skinning_index_per_vertex[i];
        result.transform = &draw_data->second.view_to_object_transforms[i];
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

void WriteGpuSkinningIndex(VideoCommon::MeshDataChunk& mesh_chunk, u32 skinning_index, u16 index)
{
  const auto posmtx_offset = mesh_chunk.vertex_declaration.posmtx.offset;
  const auto vert_ptr =
      reinterpret_cast<u8*>(mesh_chunk.vertex_data.get()) + index * mesh_chunk.vertex_stride;

  std::memcpy(vert_ptr + posmtx_offset, &skinning_index, sizeof(u32));
}

void WriteIndiceIndex(VideoCommon::MeshDataChunk& mesh_chunk, u16 indice_index)
{
  const auto index_ptr = reinterpret_cast<u16*>(mesh_chunk.indices.get()) + indice_index;

  std::memcpy(index_ptr, &indice_index, sizeof(u16));
}

void CopyReferenceData(VideoCommon::MeshDataChunk& skinning_chunk, std::size_t skinning_index_size,
                       u16 skinning_index, const VideoCommon::MeshDataChunk& reference_chunk,
                       u16 reference_index)
{
  const auto dest_ptr = reinterpret_cast<u8*>(skinning_chunk.vertex_data.get()) +
                        skinning_index * skinning_chunk.vertex_stride;
  const auto src_ptr = reinterpret_cast<u8*>(reference_chunk.vertex_data.get()) +
                       reference_index * reference_chunk.vertex_stride;

  std::memcpy(dest_ptr + skinning_index_size, src_ptr, reference_chunk.vertex_stride);
}

void CalculateMeshData(const GpuSkinningData& gpu_skinning_data,
                       const VideoCommon::MeshData& reference_mesh_data,
                       VideoCommon::MeshData& calculated_mesh_data)
{
  using MeshVertexData = std::pair<u16, OriginalVertexData>;
  std::map<GraphicsModSystem::DrawCallID, std::map<std::size_t, std::vector<MeshVertexData>>>
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

      const auto draw_call =
          GetClosestDrawCall(v0_transformed, v1_transformed, v2_transformed, gpu_skinning_data);
      if (draw_call == GraphicsModSystem::DrawCallID::INVALID)
        continue;

      auto& skinning_chunk_data = draw_call_to_chunk_to_mesh_data[draw_call];

      if (const auto original_vertex_data =
              GetNearestOriginalVertexData(v0_transformed, draw_call, gpu_skinning_data))
      {
        skinning_chunk_data[chunk_index].emplace_back(i0, *original_vertex_data);
      }

      if (const auto original_vertex_data =
              GetNearestOriginalVertexData(v1_transformed, draw_call, gpu_skinning_data))
      {
        skinning_chunk_data[chunk_index].emplace_back(i1, *original_vertex_data);
      }

      if (const auto original_vertex_data =
              GetNearestOriginalVertexData(v2_transformed, draw_call, gpu_skinning_data))
      {
        skinning_chunk_data[chunk_index].emplace_back(i2, *original_vertex_data);
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

  // Copy our reference mesh data into our skinning chunks
  for (const auto& [draw_call, chunk_to_mesh_data] : draw_call_to_chunk_to_mesh_data)
  {
    auto& skinning_chunk_list = calculated_mesh_data.m_gpu_skinning_chunks[draw_call];

    skinning_chunk_list.reserve(chunk_to_mesh_data.size());
    for (const auto& [chunk_index, mesh_data] : chunk_to_mesh_data)
    {
      const auto& reference_chunk = reference_mesh_data.m_mesh_chunks[chunk_index];
      const auto& replacement_chunk = calculated_mesh_data.m_mesh_chunks[chunk_index];
      auto& skinning_chunk = skinning_chunk_list.emplace_back();
      skinning_chunk.components_available = reference_chunk.components_available;
      skinning_chunk.material_name = reference_chunk.material_name;
      skinning_chunk.primitive_type = reference_chunk.primitive_type;
      skinning_chunk.transform = reference_chunk.transform;
      skinning_chunk.vertex_declaration = reference_chunk.vertex_declaration;

      // Add to our stride for the skinning index
      const std::size_t skinning_index_size = 4;
      skinning_chunk.vertex_stride = reference_chunk.vertex_stride + skinning_index_size;
      skinning_chunk.vertex_declaration.stride += skinning_index_size;

      // Update our declaration to include the skinning index
      skinning_chunk.vertex_declaration.posmtx.components = 4;
      skinning_chunk.vertex_declaration.posmtx.enable = true;
      skinning_chunk.vertex_declaration.posmtx.integer = true;
      skinning_chunk.vertex_declaration.posmtx.offset = 0;
      skinning_chunk.vertex_declaration.posmtx.type = ComponentFormat::UByte;
      skinning_chunk.components_available |= VB_HAS_POSMTXIDX;

      // Update our previous entries to offset by the skinning index
      if (skinning_chunk.vertex_declaration.position.enable)
        skinning_chunk.vertex_declaration.position.offset += skinning_index_size;

      for (auto& color : skinning_chunk.vertex_declaration.colors)
      {
        if (color.enable)
          color.offset += skinning_index_size;
      }

      for (auto& normal : skinning_chunk.vertex_declaration.normals)
      {
        if (normal.enable)
          normal.offset += skinning_index_size;
      }

      for (auto& texcoord : skinning_chunk.vertex_declaration.texcoords)
      {
        if (texcoord.enable)
          texcoord.offset += skinning_index_size;
      }

      // Create index data
      skinning_chunk.indices = std::make_unique<u16[]>(mesh_data.size());
      skinning_chunk.num_indices = static_cast<u32>(mesh_data.size());

      // Create vertex data
      skinning_chunk.vertex_data =
          std::make_unique<u8[]>(mesh_data.size() * skinning_chunk.vertex_stride);
      skinning_chunk.num_vertices = static_cast<u32>(mesh_data.size());

      u16 next_indice = 0;
      // Now fill out all data...
      for (const auto& [reference_index, original_vertex_data] : mesh_data)
      {
        CopyReferenceData(skinning_chunk, skinning_index_size, next_indice, replacement_chunk,
                          reference_index);
        WriteGpuSkinningIndex(skinning_chunk, original_vertex_data.skinning_index, next_indice);
        if (original_vertex_data.transform->data != Common::Matrix44::Identity().data)
        {
          // Get existing vertex position
          const auto vertex_position = ReadVertexPosition(replacement_chunk, reference_index);

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
              original_vertex_data.transform->Transform(vertex_position_transformed, 1);

          // Now write that back to the data
          WriteVertexPosition(skinning_chunk, new_vertex_position_transformed, next_indice);
        }

        WriteIndiceIndex(skinning_chunk, next_indice);
        next_indice++;
      }
    }
  }

  // We've copied everything out of the original mesh, we can remove it as it isn't
  // used during the gpu skinning process
  calculated_mesh_data.m_mesh_chunks.clear();

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
picojson::object ToJsonObject(const GpuSkinningData& gpu_skinning_data)
{
  picojson::array arr;
  for (const auto& [draw_call, skinning_data] : gpu_skinning_data.m_draw_call_to_gpu_skinning)
  {
    picojson::object data_json;
    data_json.emplace("draw_call_id", std::to_string(Common::ToUnderlying(draw_call)));

    picojson::array positions_arr_json;
    for (const auto& position : skinning_data.positions)
    {
      positions_arr_json.push_back(picojson::value{::ToJsonObject(position)});
    }
    data_json.emplace("positions", positions_arr_json);

    picojson::array transforms_arr_json;
    for (const auto& transform : skinning_data.view_to_object_transforms)
    {
      transforms_arr_json.push_back(picojson::value{::ToJsonObject(transform)});
    }
    data_json.emplace("view_to_object_transforms", transforms_arr_json);

    picojson::array gpu_skinning_indices_arr_json;
    for (const auto& index : skinning_data.skinning_index_per_vertex)
    {
      gpu_skinning_indices_arr_json.push_back(picojson::value{static_cast<double>(index)});
    }
    data_json.emplace("posmtx_indices", gpu_skinning_indices_arr_json);

    // TODO: debug data...
    picojson::array indices_arr_json;
    for (const auto& indice : skinning_data.indices)
    {
      indices_arr_json.push_back(picojson::value{static_cast<double>(indice)});
    }
    data_json.emplace("indices", indices_arr_json);

    arr.push_back(picojson::value{data_json});
  }
  picojson::object obj;
  obj.emplace("data", arr);
  return obj;
}

void FromJson(const picojson::object& obj, GpuSkinningData& gpu_skinning_data)
{
  const auto data_json_iter = obj.find("data");
  if (data_json_iter == obj.end()) [[unlikely]]
    return;

  if (!data_json_iter->second.is<picojson::array>())
    return;

  const auto arr_json = data_json_iter->second.get<picojson::array>();
  for (const auto arr_json_val : arr_json)
  {
    if (!arr_json_val.is<picojson::object>())
      continue;

    const auto data_value_json = arr_json_val.get<picojson::object>();
    const auto draw_call_id_str = ReadStringFromJson(data_value_json, "draw_call_id").value_or("0");
    u64 draw_call_value = 0;
    if (!TryParse(draw_call_id_str, &draw_call_value))
    {
      ERROR_LOG_FMT(VIDEO, "'draw_call_id' is not a number!");
      return;
    }
    const auto draw_call_id = static_cast<GraphicsModSystem::DrawCallID>(draw_call_value);

    GpuSkinningData::GpuSkinningForDrawCall drawcall_data;
    if (const auto positions_iter = data_value_json.find("positions");
        positions_iter != data_value_json.end())
    {
      if (positions_iter->second.is<picojson::array>())
      {
        const auto positions_arr = positions_iter->second.get<picojson::array>();
        for (const auto& position_arr_value : positions_arr)
        {
          if (position_arr_value.is<picojson::object>())
          {
            const auto positions_arr_obj = position_arr_value.get<picojson::object>();
            Common::Vec3 vec;
            ::FromJson(positions_arr_obj, vec);
            drawcall_data.positions.push_back(std::move(vec));
          }
        }
      }
    }

    if (const auto transforms_iter = data_value_json.find("view_to_object_transforms");
        transforms_iter != data_value_json.end())
    {
      if (transforms_iter->second.is<picojson::array>())
      {
        const auto transforms_arr = transforms_iter->second.get<picojson::array>();
        for (const auto& transform_arr_value : transforms_arr)
        {
          if (transform_arr_value.is<picojson::object>())
          {
            const auto transforms_arr_obj = transform_arr_value.get<picojson::object>();
            Common::Matrix44 mat;
            ::FromJson(transforms_arr_obj, mat);
            drawcall_data.view_to_object_transforms.push_back(std::move(mat));
          }
        }
      }
    }

    if (const auto posmtx_indices_iter = data_value_json.find("posmtx_indices");
        posmtx_indices_iter != data_value_json.end())
    {
      if (posmtx_indices_iter->second.is<picojson::array>())
      {
        const auto posmtx_indices_arr = posmtx_indices_iter->second.get<picojson::array>();
        for (const auto& posmtx_index_value : posmtx_indices_arr)
        {
          if (posmtx_index_value.is<double>())
          {
            drawcall_data.skinning_index_per_vertex.push_back(
                static_cast<u32>(posmtx_index_value.get<double>()));
          }
        }
      }
    }

    if (const auto indices_iter = data_value_json.find("indices");
        indices_iter != data_value_json.end())
    {
      if (indices_iter->second.is<picojson::array>())
      {
        const auto indices_arr = indices_iter->second.get<picojson::array>();
        for (const auto& indice_value : indices_arr)
        {
          if (indice_value.is<double>())
          {
            drawcall_data.indices.push_back(static_cast<u32>(indice_value.get<double>()));
          }
        }
      }
    }

    gpu_skinning_data.m_draw_call_to_gpu_skinning[draw_call_id] = std::move(drawcall_data);
  }
}

void CalculateMeshDataWithGpuSkinning(const GpuSkinningData& gpu_skinning_data,
                                      const VideoCommon::MeshData& reference_mesh_data,
                                      VideoCommon::MeshData& calculated_mesh_data)
{
  CalculateMeshData(gpu_skinning_data, reference_mesh_data, calculated_mesh_data);
}
}  // namespace GraphicsModEditor
