// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"

#include <map>
#include <optional>
#include <set>

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

class VecToIDConverter
{
public:
  u32 Get(const Common::Vec3& v)
  {
    const auto iter = m_pos_to_id.find(v);
    if (iter != m_pos_to_id.end())
      return iter->second;
    const u32 id = m_id++;
    m_pos_to_id[v] = id;
    return id;
  }

private:
  u32 m_id = 0;
  std::map<Common::Vec3, uint32_t, CompareVec3> m_pos_to_id;
};

template <typename Func>
void OnEachNativeTriangle(const NativeMeshData::NativeMeshChunkForDrawCall& draw_data_chunk,
                          Func&& f)
{
  if (draw_data_chunk.primitive_type == PrimitiveType::TriangleStrip)
  {
    u32 strip_count = 0;
    for (std::size_t i = 0; i < draw_data_chunk.index_data.size(); i++)
    {
      // Primitive restart
      if (draw_data_chunk.index_data[i] == UINT16_MAX)
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
        i0 = draw_data_chunk.index_data[i - 2];
        i1 = draw_data_chunk.index_data[i - 1];
        i2 = draw_data_chunk.index_data[i];
      }
      else
      {
        // Swap for odd-numbered triangles to maintain consistent CCW winding
        i0 = draw_data_chunk.index_data[i - 2];
        i1 = draw_data_chunk.index_data[i];
        i2 = draw_data_chunk.index_data[i - 1];
      }

      if (i0 == i1 || i1 == i2 || i0 == i2)
      {
        continue;
      }

      f(i0, i1, i2);
    }
  }
}

std::vector<float> GetClosestDistancePerPosition(
    const Common::Vec3& reference_position, const Common::Vec3& reference_normal,
    std::span<const Common::Vec3> positions, std::span<const Common::Vec3> normals,
    const std::set<Common::Vec3, CompareVec3>& vertex_positions_duplicated)
{
  std::vector<float> result;
  result.reserve(positions.size());
  for (std::size_t i = 0; i < positions.size(); i++)
  {
    float cos_theta = reference_normal.Dot(normals[i]);

    // If the normals are pointing more than 90 degrees apart,
    // it's likely the "inside" of a mesh or a different limb.
    if (cos_theta < 0.0f)
    {
      result.push_back(std::numeric_limits<float>::max());
      continue;
    }
    float normal_penalty = 1.0f + (1.0f - cos_theta) * 10.0f;

    const auto& position = positions[i];
    if (vertex_positions_duplicated.find(position) != vertex_positions_duplicated.end())
    {
      result.push_back(std::numeric_limits<float>::max());
    }
    else
    {
      const float distance = (reference_position - position).LengthSquared();

      // Optional: Slightly penalize based on angle (closer alignment = smaller distance)
      // weight = distance * (2.0 - cos_theta)
      result.push_back(distance * normal_penalty);
    }
  }
  return result;
}

using NativeMeshDrawToNormals = std::map<GraphicsModSystem::DrawCallID, std::vector<Common::Vec3>>;

GraphicsModSystem::DrawCallID
GetClosestDrawCall(const Common::Vec3& v0, const Common::Vec3& v1, const Common::Vec3& v2,
                   const Common::Vec3& n0, const Common::Vec3& n1, const Common::Vec3& n2,
                   const NativeMeshData& native_mesh_data,
                   const NativeMeshDrawToNormals& native_mesh_normals_per_draw)
{
  // 1. Calculate the Replacement Face Center and Average Normal
  Common::Vec3 replacement_center = (v0 + v1 + v2) * (1.0f / 3.0f);
  Common::Vec3 replacement_normal = (n0 + n1 + n2).Normalized();

  GraphicsModSystem::DrawCallID best_draw_call = GraphicsModSystem::DrawCallID::INVALID;
  float best_score = std::numeric_limits<float>::max();

  for (const auto& [draw_call, draw_data] : native_mesh_data.m_draw_call_to_chunk)
  {
    const auto& native_positions = draw_data.positions_transformed;
    const auto& native_normals = native_mesh_normals_per_draw.at(draw_call);

    OnEachNativeTriangle(draw_data, [&](u16 i0, u16 i1, u16 i2) {
      // 2. Calculate Native Face Center
      Common::Vec3 native_center =
          (native_positions[i0] + native_positions[i1] + native_positions[i2]) * (1.0f / 3.0f);

      // 3. Use Averaged Vertex Normals (More stable than Cross Product for matching)
      Common::Vec3 native_avg_normal =
          (native_normals[i0] + native_normals[i1] + native_normals[i2]);
      float normal_len = native_avg_normal.Length();
      if (normal_len < 1e-6f)
        return;  // Skip degenerate/corrupt normals
      native_avg_normal = native_avg_normal * (1.0f / normal_len);

      // 4. Scoring Logic
      float dist_sq = (replacement_center - native_center).LengthSquared();
      float dot = replacement_normal.Dot(native_avg_normal);

      // Penalty: Harsh if facing away (>90 deg), otherwise smooth
      float normal_penalty = (dot > 0.0f) ? (1.0f - dot) : 20.0f;

      // Balance: Distance is primary, normal is the "Tie-Breaker"
      // We use a small weight for normal_penalty to prevent it from
      // picking a far-away triangle just because the normal is a 100% match.
      float current_score = dist_sq + (normal_penalty * 0.001f);

      if (current_score < best_score)
      {
        best_score = current_score;
        best_draw_call = draw_call;
      }
    });
  }
  return best_draw_call;
}

struct OriginalVertexData
{
  u32 buffer_index;
  const NativeMeshData::NativeMeshChunkForDrawCall* draw_call_data;
};

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

Common::Vec3 ReadVertexNormal(const NativeMeshData::NativeMeshChunkForDrawCall& mesh_chunk,
                              u16 index)
{
  const auto offset = mesh_chunk.vertex_declaration.normals[0].offset;
  const auto vert_ptr = reinterpret_cast<const u8*>(mesh_chunk.vertex_data.data()) +
                        index * mesh_chunk.vertex_declaration.stride;
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

void WriteIndiceIndex(VideoCommon::MeshDataChunk& mesh_chunk, u32 buffer_position,
                      u16 gpu_vertex_index)
{
  u16* base_ptr = reinterpret_cast<u16*>(mesh_chunk.indices.get());
  base_ptr[buffer_position] = gpu_vertex_index;
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

  if (component_type == VB_HAS_WEIGHTS)
  {
    auto& attribute = final_mesh_chunk->vertex_declaration.weights;

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

NativeMeshDrawToNormals
GetOrGenerateNativeNormalsPerDrawCall(const NativeMeshData& native_mesh_data)
{
  NativeMeshDrawToNormals result;
  for (const auto& [draw_call, draw_data] : native_mesh_data.m_draw_call_to_chunk)
  {
    result[draw_call].resize(draw_data.positions_transformed.size());

    OnEachNativeTriangle(draw_data, [&](u16 i0, u16 i1, u16 i2) {
      Common::Vec3 n0;
      Common::Vec3 n1;
      Common::Vec3 n2;

      // If we have normals, use them, otherwise we calculate them off the position
      // this is so that we can compare face direction so being accurate isn't necessary
      if (draw_data.vertex_declaration.normals.size() > 0 &&
          draw_data.vertex_declaration.normals[0].enable)
      {
        n0 = ReadVertexNormal(draw_data, i0);
        n1 = ReadVertexNormal(draw_data, i1);
        n2 = ReadVertexNormal(draw_data, i2);
      }
      else
      {
        const auto& v0 = draw_data.positions_transformed[i0];
        const auto& v1 = draw_data.positions_transformed[i1];
        const auto& v2 = draw_data.positions_transformed[i2];

        const auto a = (v1 - v0);
        const auto b = (v2 - v0);
        const auto norm = a.Cross(b).Normalized();
        n0 = norm;
        n1 = norm;
        n2 = norm;
      }

      result[draw_call][i0] = n0;
      result[draw_call][i1] = n1;
      result[draw_call][i2] = n2;
    });
  }
  return result;
}

void NormalizeWeights(VertexInfluence& inf)
{
  float total = 0.0f;
  for (float w : inf.weights)
    total += w;
  if (total > 1e-6f)
  {
    float inv = 1.0f / total;
    for (float& w : inf.weights)
      w *= inv;
  }
}

void BlendInfluence(VertexInfluence& target, const VertexInfluence& source, float factor)
{
  std::map<int, float> accumulator;

  // 1. Load current target bones into the map
  for (int i = 0; i < 4; ++i)
  {
    if (target.weights[i] > 0.0f)
      accumulator[target.bone_ids[i]] = target.weights[i];
  }

  // 2. Add source bones (scaled by the blend factor)
  for (int i = 0; i < 4; ++i)
  {
    if (source.weights[i] > 0.0f)
    {
      accumulator[source.bone_ids[i]] += source.weights[i] * factor;
    }
  }

  // 3. Pick Top 4 and Re-Normalize (Essential for GPU buffers)
  std::vector<std::pair<int, float>> sorted(accumulator.begin(), accumulator.end());
  std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });

  target = {};  // Reset target
  float total = 0.0f;
  for (int i = 0; i < std::min<int>(4, static_cast<int>(sorted.size())); ++i)
  {
    target.bone_ids[i] = sorted[i].first;
    target.weights[i] = sorted[i].second;
    total += target.weights[i];
  }

  if (total > 0.0f)
  {
    for (int i = 0; i < 4; ++i)
      target.weights[i] /= total;
  }
}

// Represents a triangle before it's sorted into rendering buckets
struct DraftTriangle
{
  GraphicsModSystem::DrawCallID current_draw_call;
  bool draw_call_needs_resync = false;
  std::size_t replacement_chunk_index;
  std::array<u16, 3> replacement_indices;
  std::array<u16, 3> native_indices;
  std::array<u32, 3> weight_ids;
};

struct WeldedEdge
{
  u32 weight_index0;
  u32 weight_index1;

  auto operator<=>(const WeldedEdge&) const = default;
};

std::array<WeldedEdge, 3> GetEdges(const DraftTriangle& tri)
{
  auto a = tri.weight_ids[0];
  auto b = tri.weight_ids[1];
  auto c = tri.weight_ids[2];
  return {WeldedEdge{std::min(a, b), std::max(a, b)}, WeldedEdge{std::min(b, c), std::max(b, c)},
          WeldedEdge{std::min(c, a), std::max(c, a)}};
}

void CalculateMeshData(const NativeMeshData& native_mesh_data,
                       const VideoCommon::MeshData& reference_mesh_data, u32 components_to_copy,
                       const std::optional<ExporterSkinningRig>& cpu_skinning_rig,
                       VideoCommon::MeshData* final_mesh_data)
{
  VecToIDConverter vec_converter;

  struct IntermediateMeshChunkData
  {
    struct Triangle
    {
      struct VertexData
      {
        u16 replacement_index;
        u16 native_index;
        u32 weight_id;
      };
      std::array<VertexData, 3> vertices;
    };
    std::vector<Triangle> triangles;
  };
  using MeshVertexData = std::pair<u16, OriginalVertexData>;
  std::map<GraphicsModSystem::DrawCallID, std::map<std::size_t, IntermediateMeshChunkData>>
      draw_call_to_chunk_to_mesh_data;

  // Add a global flat list of all assigned triangles to make smoothing easy
  struct GlobalTriangleRef
  {
    GraphicsModSystem::DrawCallID* assigned_id;
    std::array<u32, 3> indices;
  };
  std::vector<GlobalTriangleRef> replacement_triangles;

  u32 num_replacement_vertices = 0;
  for (const auto& chunk : reference_mesh_data.m_mesh_chunks)
  {
    num_replacement_vertices += chunk.num_vertices;
  }
  std::vector<VertexInfluence> replacement_weights(num_replacement_vertices);

  std::vector<DraftTriangle> draft_triangles;

  const auto native_mesh_normals_per_draw = GetOrGenerateNativeNormalsPerDrawCall(native_mesh_data);

  for (std::size_t chunk_index = 0; chunk_index < reference_mesh_data.m_mesh_chunks.size();
       chunk_index++)
  {
    const auto& reference_chunk = reference_mesh_data.m_mesh_chunks[chunk_index];

    // TODO: handle other primitive types
    if (reference_chunk.primitive_type != PrimitiveType::Triangles)
      return;

    // Now update our data and calculate the indices per draw call
    u32 indice_index = 0;
    while (indice_index < reference_chunk.num_indices)
    {
      std::vector<std::pair<u16, OriginalVertexData>> index_original_mesh_data_pairs;

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

      Common::Vec3 n0;
      Common::Vec3 n1;
      Common::Vec3 n2;

      // If we have normals, use them, otherwise we calculate them off the position
      // this is so that we can compare face direction to improve our draw selection
      if (reference_chunk.vertex_declaration.normals.size() > 0 &&
          reference_chunk.vertex_declaration.normals[0].enable)
      {
        n0 = ReadVertexNormal(reference_chunk, i0);
        n1 = ReadVertexNormal(reference_chunk, i1);
        n2 = ReadVertexNormal(reference_chunk, i2);
      }
      else
      {
        const auto a = (v1 - v0);
        const auto b = (v2 - v0);
        const auto norm = a.Cross(b).Normalized();
        n0 = norm;
        n1 = norm;
        n2 = norm;
      }

      indice_index += 3;

      const auto draw_call =
          GetClosestDrawCall(v0_transformed, v1_transformed, v2_transformed, n0, n1, n2,
                             native_mesh_data, native_mesh_normals_per_draw);
      if (draw_call == GraphicsModSystem::DrawCallID::INVALID)
      {
        ERROR_LOG_FMT(VIDEO, "Draw call invalid?");
        continue;
      }

      auto& triangle = draft_triangles.emplace_back();
      triangle.current_draw_call = draw_call;
      triangle.replacement_chunk_index = chunk_index;

      const u32 id0 = vec_converter.Get(v0);
      const u32 id1 = vec_converter.Get(v1);
      const u32 id2 = vec_converter.Get(v2);

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v0_transformed, draw_call, native_mesh_data))
      {
        triangle.replacement_indices[0] = i0;
        triangle.native_indices[0] = *original_vertex_index;
        triangle.weight_ids[0] = id0;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 0 not found?");
      }

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v1_transformed, draw_call, native_mesh_data))
      {
        triangle.replacement_indices[1] = i1;
        triangle.native_indices[1] = *original_vertex_index;
        triangle.weight_ids[1] = id1;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 1 not found?");
      }

      if (const auto original_vertex_index =
              GetNearestOriginalVertexIndex(v2_transformed, draw_call, native_mesh_data))
      {
        triangle.replacement_indices[2] = i2;
        triangle.native_indices[2] = *original_vertex_index;
        triangle.weight_ids[2] = id2;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Closest index 2 not found?");
      }

      if (cpu_skinning_rig)
      {
        const auto svj_iter = cpu_skinning_rig->draw_call_to_local_svj.find(draw_call);
        if (svj_iter == cpu_skinning_rig->draw_call_to_local_svj.end())
        {
          ERROR_LOG_FMT(VIDEO, "CPU skinning local_svj not found for draw call");
          continue;
        }
        const u16 welded_i0 = svj_iter->second[triangle.native_indices[0]];
        replacement_weights[id0] = cpu_skinning_rig->global_weights[welded_i0];

        const u16 welded_i1 = svj_iter->second[triangle.native_indices[1]];
        replacement_weights[id1] = cpu_skinning_rig->global_weights[welded_i1];

        const u16 welded_i2 = svj_iter->second[triangle.native_indices[2]];
        replacement_weights[id2] = cpu_skinning_rig->global_weights[welded_i2];
      }
    }
  }

  // If we are CPU skinning, we need to copy
  // the skinning data from the capture step
  // over to the final mesh data.  The weights/bones
  // for the replacement mesh will be handled separately
  if (cpu_skinning_rig)
  {
    final_mesh_data->m_cpu_skinning_rig = cpu_skinning_rig->runtime_rig;

    // The CPU skinning logic also needs the original positional data
    // that represents the rest pose, this is so that we can compare
    // the moving original mesh to the rest pose to determine movement
    for (const auto& [draw_call, data] : native_mesh_data.m_draw_call_to_chunk)
    {
      for (std::size_t i = 0; i < data.positions_transformed.size(); i++)
      {
        const auto pos_transformed = data.positions_transformed[i];
        const auto inverse_transform = data.position_inverse_transforms[i];

        const auto vertex_position_transformed = inverse_transform.Transform(pos_transformed, 1);

        final_mesh_data->m_original_positions[draw_call].push_back(
            Eigen::Vector3f(vertex_position_transformed.x, vertex_position_transformed.y,
                            vertex_position_transformed.z));
      }
    }

    // Build an edge map for blending
    std::map<WeldedEdge, std::vector<size_t>> edge_to_tri_indices;
    for (size_t i = 0; i < draft_triangles.size(); ++i)
    {
      for (const auto& edge : GetEdges(draft_triangles[i]))
      {
        edge_to_tri_indices[edge].push_back(i);
      }
    }

    // Blend the weights, this is so we can avoid any seams that
    // are caused by weights being pulled from the native mesh
    // and not behaving properly across draw-call/chunk boundaries
    // (also reduces "stair stepping"
    for (int p = 0; p < 2; ++p)
    {
      std::vector<VertexInfluence> next_weights = replacement_weights;
      for (auto const& [edge, tri_indices] : edge_to_tri_indices)
      {
        // Blend weighted influences
        BlendInfluence(next_weights[edge.weight_index0], replacement_weights[edge.weight_index1],
                       0.2f);
        BlendInfluence(next_weights[edge.weight_index1], replacement_weights[edge.weight_index0],
                       0.2f);
      }
      // Normalize every entry in next_weights after the blur
      for (auto& inf : next_weights)
        NormalizeWeights(inf);
      replacement_weights = next_weights;  // Update and Normalize
    }
  }

  // Copy draft triangles to real triangles...
  for (const auto& tri : draft_triangles)
  {
    auto& bucket =
        draw_call_to_chunk_to_mesh_data[tri.current_draw_call][tri.replacement_chunk_index];
    IntermediateMeshChunkData::Triangle final_tri;
    for (int i = 0; i < 3; ++i)
    {
      final_tri.vertices[i] = {tri.replacement_indices[i], tri.native_indices[i],
                               tri.weight_ids[i]};
    }
    bucket.triangles.push_back(final_tri);
  }

  // Copy our replacement mesh data into our skinning chunks
  for (const auto& [draw_call, chunk_to_mesh_data] : draw_call_to_chunk_to_mesh_data)
  {
    auto& per_draw_replacement_chunk_list = final_mesh_data->m_skinning_chunks[draw_call];

    per_draw_replacement_chunk_list.reserve(chunk_to_mesh_data.size());

    const auto draw_call_data_iter = native_mesh_data.m_draw_call_to_chunk.find(draw_call);
    if (draw_call_data_iter == native_mesh_data.m_draw_call_to_chunk.end())
      continue;

    auto& draw_call_data = draw_call_data_iter->second;
    auto& native_vertex_declaration = draw_call_data.vertex_declaration;

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

      {
        const u32 component_type = VB_HAS_POSMTXIDX;
        if (native_vertex_declaration.posmtx.enable && components_to_copy & component_type)
        {
          UpdateDeclaration(&final_replacement_chunk, component_type,
                            native_vertex_declaration.posmtx);
        }
        else if (cpu_skinning_rig)
        {
          // Update our declaration to account for adding an index to the vertices
          // this is to avoid having to have a separate buffer...
          AttributeFormat custom_index;
          custom_index.type = ComponentFormat::Float;
          custom_index.components = 4;
          custom_index.integer = false;
          UpdateDeclaration(&final_replacement_chunk, component_type, custom_index);

          // Disable the chunk to avoid using it in the final rendering
          final_replacement_chunk.vertex_declaration.posmtx.enable = false;
          final_replacement_chunk.components_available &= ~component_type;
        }
      }

      if (cpu_skinning_rig)
      {
        const u32 component_type = VB_HAS_WEIGHTS;

        // Update our declaration to account for adding the skinning weights to the vertices
        AttributeFormat custom_index;
        custom_index.type = ComponentFormat::Float;
        custom_index.components = 4;
        custom_index.integer = false;
        UpdateDeclaration(&final_replacement_chunk, component_type, custom_index);

        // Disable the chunk to avoid using it in the final rendering
        final_replacement_chunk.vertex_declaration.weights.enable = false;
        final_replacement_chunk.components_available &= ~component_type;
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

      std::map<u16, u16> replacement_index_to_unique_index;
      std::map<u16, const IntermediateMeshChunkData::Triangle::VertexData*>
          replacement_index_to_vertex_data;
      u16 next_available_final_idx = 0;

      for (const auto& triangle : intermediate_data.triangles)
      {
        for (const auto& vert_data : triangle.vertices)
        {
          if (replacement_index_to_unique_index.find(vert_data.replacement_index) ==
              replacement_index_to_unique_index.end())
          {
            replacement_index_to_unique_index[vert_data.replacement_index] =
                next_available_final_idx++;
            replacement_index_to_vertex_data[vert_data.replacement_index] = &vert_data;
          }
        }
      }

      // Create index data
      const auto total_indices = intermediate_data.triangles.size() * 3;
      final_replacement_chunk.indices = std::make_unique<u16[]>(total_indices);
      final_replacement_chunk.num_indices = static_cast<u32>(total_indices);

      // Create vertex data
      final_replacement_chunk.num_vertices = next_available_final_idx;
      final_replacement_chunk.vertex_data =
          std::make_unique<u8[]>(next_available_final_idx * final_replacement_chunk.vertex_stride);

      // Now fill out all vertex data...
      for (auto const& [replacement_index, final_idx] : replacement_index_to_unique_index)
      {
        const IntermediateMeshChunkData::Triangle::VertexData* vert_data =
            replacement_index_to_vertex_data[replacement_index];
        const u16 native_index = vert_data->native_index;

        CopyReplacementData(final_replacement_chunk, final_idx, replacement_chunk,
                            replacement_index);

        {
          const u32 component_type = VB_HAS_POSMTXIDX;
          if (native_vertex_declaration.posmtx.enable && components_to_copy & component_type)
          {
            CopyNativeMeshData(final_replacement_chunk, final_idx,
                               final_replacement_chunk.vertex_declaration.posmtx, draw_call_data,
                               native_index, native_vertex_declaration.posmtx);
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
              CopyNativeMeshData(final_replacement_chunk, final_idx, replacement_color_declaration,
                                 draw_call_data, native_index, native_color_declaration);
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
              CopyNativeMeshData(final_replacement_chunk, final_idx, replacement_normal_declaration,
                                 draw_call_data, native_index, native_normal_declaration);
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
              CopyNativeMeshData(final_replacement_chunk, final_idx,
                                 replacement_texcoord_declaration, draw_call_data, native_index,
                                 native_texcoord_declaration);
            }
          }
        }

        if (cpu_skinning_rig)
        {
          // If CPU skinned, we need to write our bone indexes to the posmtx spot
          // Similarly, we need to write our weight values to the weight spot

          const auto bone_offset = final_replacement_chunk.vertex_declaration.posmtx.offset;
          const auto weight_offset = final_replacement_chunk.vertex_declaration.weights.offset;
          u8* vert_ptr = final_replacement_chunk.vertex_data.get() +
                         final_idx * final_replacement_chunk.vertex_stride;

          const auto& smoothed_influence = replacement_weights[vert_data->weight_id];

          std::memcpy(vert_ptr + bone_offset, smoothed_influence.bone_ids.data(), sizeof(int) * 4);
          std::memcpy(vert_ptr + weight_offset, smoothed_influence.weights.data(),
                      sizeof(float) * 4);
        }

        auto& inverse_position_transform = draw_call_data.position_inverse_transforms[native_index];
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
          WriteVertexPosition(final_replacement_chunk, new_vertex_position_transformed, final_idx);
        }

        if (replacement_chunk.vertex_declaration.normals[0].enable &&
            final_replacement_chunk.vertex_declaration.normals[0].enable)
        {
          auto& inverse_normal_transform = draw_call_data.normal_inverse_transforms[native_index];
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
                              final_idx);
          }
        }
      }

      // Write our indices
      u32 buffer_pos = 0;
      for (const auto& triangle : intermediate_data.triangles)
      {
        for (const auto& vert_data : triangle.vertices)
        {
          const u16 unique_index = replacement_index_to_unique_index[vert_data.replacement_index];
          WriteIndiceIndex(final_replacement_chunk, buffer_pos++, unique_index);
        }
      }
    }
  }

  // We've copied everything out of the original mesh, we can remove it as it isn't
  // used during the gpu skinning process
  final_mesh_data->m_mesh_chunks.clear();
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

  for (std::size_t i = 0; i < chunk_size; ++i)
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

bool ExporterSkinningRig::FromBinary(std::span<const u8> raw_data, ExporterSkinningRig* data)
{
  std::size_t offset = 0;

  // 1. Load the core runtime rig (Welded Pos, Bone Centers, SVD Groups)
  if (!VideoCommon::SkinningRig::FromBinary(raw_data.data(), &offset, &data->runtime_rig))
    return false;

  // Load the data only needed during import

  // The Global Welded Weights (The "DNA" for the cage)
  const size_t weight_count = data->runtime_rig.welded_positions.size();
  data->global_weights.resize(weight_count);

  for (std::size_t i = 0; i < weight_count; ++i)
  {
    GraphicsModEditor::VertexInfluence influence;

    std::memcpy(influence.bone_ids.data(), raw_data.data() + offset, sizeof(int) * 4);
    offset += sizeof(int) * 4;

    std::memcpy(influence.weights.data(), raw_data.data() + offset, sizeof(float) * 4);
    offset += sizeof(float) * 4;

    data->global_weights[i] = std::move(influence);
  }

  // The Per-Chunk SVJ (Native Index -> Welded Index Mapping)
  u32 num_chunks = 0;
  std::memcpy(&num_chunks, raw_data.data() + offset, sizeof(u32));
  offset += sizeof(u32);

  for (u32 i = 0; i < num_chunks; ++i)
  {
    u64 id = 0;
    std::memcpy(&id, raw_data.data() + offset, sizeof(u64));
    offset += sizeof(u64);
    const GraphicsModSystem::DrawCallID draw_call = static_cast<GraphicsModSystem::DrawCallID>(id);

    u32 vertex_count = 0;
    std::memcpy(&vertex_count, raw_data.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    auto& local_svj = data->draw_call_to_local_svj[draw_call];
    local_svj.resize(vertex_count);

    std::memcpy(local_svj.data(), raw_data.data() + offset, vertex_count * sizeof(int));
    offset += vertex_count * sizeof(int);
  }

  return true;
}

bool ExporterSkinningRig::ToBinary(File::IOFile* file_data, const ExporterSkinningRig& data)
{
  // 1. Reuse the Core Runtime Logic first
  if (!VideoCommon::SkinningRig::ToBinary(file_data, data.runtime_rig))
    return false;

  // 2. Write Global Welded Weights (4 per vertex)
  for (const auto& inf : data.global_weights)
  {
    file_data->WriteBytes(inf.bone_ids.data(), sizeof(int) * 4);
    file_data->WriteBytes(inf.weights.data(), sizeof(float) * 4);
  }

  // 3. Write Per-Chunk SVJ Mappings
  u32 num_chunks = static_cast<u32>(data.draw_call_to_local_svj.size());
  file_data->WriteBytes(&num_chunks, sizeof(u32));

  for (auto const& [draw_call, svj_vec] : data.draw_call_to_local_svj)
  {
    u64 id = static_cast<u64>(draw_call);
    file_data->WriteBytes(&id, sizeof(u64));

    u32 vertex_count = static_cast<u32>(svj_vec.size());
    file_data->WriteBytes(&vertex_count, sizeof(u32));

    // Bulk write the SVJ indices (Native Index -> Welded Index)
    file_data->WriteBytes(svj_vec.data(), vertex_count * sizeof(int));
  }

  return true;
}

void CalculateMeshDataWithSkinning(const NativeMeshData& native_mesh_data,
                                   const VideoCommon::MeshData& reference_mesh_data,
                                   u32 components_to_copy,
                                   const std::optional<ExporterSkinningRig>& cpu_skinning_rig,
                                   VideoCommon::MeshData* final_mesh_data)
{
  CalculateMeshData(native_mesh_data, reference_mesh_data, components_to_copy, cpu_skinning_rig,
                    final_mesh_data);
}
}  // namespace GraphicsModEditor
