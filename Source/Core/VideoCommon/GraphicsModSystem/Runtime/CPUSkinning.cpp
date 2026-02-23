// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CPUSkinning.h"

#include <array>

#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <Eigen/Sparse>

#include "Common/Logging/Log.h"

#include "VideoCommon/GraphicsModSystem/Runtime/SkeletonManager.h"

static Common::Vec3 ReadVec3(const u8* vert_ptr, u32 v3_offset)
{
  Common::Vec3 v3_result;
  std::memcpy(&v3_result.x, vert_ptr + v3_offset, sizeof(float));
  std::memcpy(&v3_result.y, vert_ptr + sizeof(float) + v3_offset, sizeof(float));
  std::memcpy(&v3_result.z, vert_ptr + sizeof(float) * 2 + v3_offset, sizeof(float));
  return v3_result;
}

static std::array<float, 4> ReadWeights(const u8* vert_ptr, u32 weight_offset)
{
  std::array<float, 4> weight_data;
  std::memcpy(&weight_data[0], vert_ptr + weight_offset, sizeof(float));
  std::memcpy(&weight_data[1], vert_ptr + sizeof(float) + weight_offset, sizeof(float));
  std::memcpy(&weight_data[2], vert_ptr + sizeof(float) * 2 + weight_offset, sizeof(float));
  std::memcpy(&weight_data[3], vert_ptr + sizeof(float) * 3 + weight_offset, sizeof(float));
  return weight_data;
}

static std::array<int, 4> ReadTransformIds(const u8* vert_ptr, u32 transform_id_offset)
{
  std::array<int, 4> transform_ids;
  std::memcpy(&transform_ids[0], vert_ptr + transform_id_offset, sizeof(u32));
  std::memcpy(&transform_ids[1], vert_ptr + sizeof(u32) + transform_id_offset, sizeof(u32));
  std::memcpy(&transform_ids[2], vert_ptr + sizeof(u32) * 2 + transform_id_offset, sizeof(u32));
  std::memcpy(&transform_ids[3], vert_ptr + sizeof(u32) * 3 + transform_id_offset, sizeof(u32));
  return transform_ids;
}

static void WriteVec3(u8* vert_ptr, u32 v3_offset, const Common::Vec3& v3_value)
{
  std::memcpy(vert_ptr + v3_offset, &v3_value.x, sizeof(float));
  std::memcpy(vert_ptr + v3_offset + sizeof(float), &v3_value.y, sizeof(float));
  std::memcpy(vert_ptr + v3_offset + sizeof(float) * 2, &v3_value.z, sizeof(float));
}

static void SolveChunkBoneTransforms(GraphicsModSystem::DrawCallID draw_call,
                                     const VideoCommon::SkinningRig& replacement_rig,
                                     const std::vector<Eigen::Vector3f>& original_positions,
                                     const GraphicsModSystem::DrawDataView& native_draw_data,
                                     std::vector<Eigen::Affine3f>& out_bone_matrices,
                                     std::vector<float>& out_bone_total_weights)
{
  auto it = replacement_rig.draw_call_rig_details.find(draw_call);
  if (it == replacement_rig.draw_call_rig_details.end())
    return;

  const auto& chunk_rig_data = it->second;
  // Initialize with Identity to avoid garbage data for unsolved bones
  out_bone_matrices.assign(replacement_rig.bone_rest_centers.size(), Eigen::Affine3f::Identity());
  out_bone_total_weights.assign(replacement_rig.bone_rest_centers.size(), 0.0f);

  const auto get_position = [](int index, const GraphicsModSystem::DrawDataView& native_draw_data) {
    const u8* read_ptr =
        native_draw_data.vertex_data + index * native_draw_data.vertex_format->GetVertexStride();
    const auto native_position =
        ReadVec3(read_ptr, native_draw_data.vertex_format->GetVertexDeclaration().position.offset);
    return Eigen::Vector3f(native_position.x, native_position.y, native_position.z);
  };

  for (auto const& [bone_id, local_bone_group] : chunk_rig_data.bone_groups)
  {
    const size_t N = local_bone_group.original_indices.size();

    // 1. Compute Centroids of the points as they are in the WORLD
    float total_w = 0.0f;
    Eigen::Vector3f c_rest = Eigen::Vector3f::Zero();
    Eigen::Vector3f c_curr = Eigen::Vector3f::Zero();

    if (original_positions.size() < N)
      continue;

    for (size_t i = 0; i < N; ++i)
    {
      float w = local_bone_group.weights[i];
      c_rest += w * original_positions[local_bone_group.original_indices[i]];
      c_curr += w * get_position(local_bone_group.original_indices[i], native_draw_data);
      total_w += w;
    }

    if (total_w < 0.001f)
      continue;  // Skip if no meaningful weight

    c_rest /= (float)total_w;
    c_curr /= (float)total_w;

    // 2. Covariance H
    Eigen::Matrix3f H = Eigen::Matrix3f::Zero();
    for (size_t i = 0; i < N; ++i)
    {
      float w = local_bone_group.weights[i];
      Eigen::Vector3f p_r = original_positions[local_bone_group.original_indices[i]] - c_rest;
      Eigen::Vector3f p_c =
          get_position(local_bone_group.original_indices[i], native_draw_data) - c_curr;
      H += w * (p_c * p_r.transpose());  // Order: Current * Rest^T
    }

    // 3. SVD for Rotation
    Eigen::JacobiSVD<Eigen::Matrix3f> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3f R = svd.matrixU() * svd.matrixV().transpose();
    if (R.determinant() < 0)
    {
      Eigen::Matrix3f V = svd.matrixV();
      V.col(2) *= -1.0f;
      R = svd.matrixU() * V.transpose();
    }

    // 4. Transform
    out_bone_matrices[bone_id].linear() = R;
    out_bone_matrices[bone_id].translation() = c_curr - R * c_rest;
    out_bone_total_weights[bone_id] = total_w;
  }
}

static void ApplyLinearBlendedSkinning(
    const VideoCommon::MeshResource::MeshChunk& replacement_chunk,  // The replacement chunk
    const VideoCommon::SkinningRig& replacement_rig,
    const std::vector<Eigen::Affine3f>& bone_matrices,  // Output from SolveChunkBoneTransforms
    std::vector<u8>* out)                               // The final data to be written
{
  const auto chunk_stride = replacement_chunk.GetVertexStride();
  const auto& chunk_declaration = replacement_chunk.GetNativeVertexFormat()->GetVertexDeclaration();
  for (std::size_t i = 0; i < replacement_chunk.GetVertexCount(); i++)
  {
    const u8* read_ptr = replacement_chunk.GetVertexData().data() + i * chunk_stride;
    u8* write_ptr = out->data() + i * chunk_stride;

    const auto replacement_weights = ReadWeights(read_ptr, chunk_declaration.weights.offset);
    const auto replacement_bone_ids = ReadTransformIds(read_ptr, chunk_declaration.posmtx.offset);

    {
      const auto replacement_position = ReadVec3(read_ptr, chunk_declaration.position.offset);
      const Eigen::Vector3f pos(replacement_position.x, replacement_position.y,
                                replacement_position.z);

      Eigen::Vector3f blended_pos = Eigen::Vector3f::Zero();

      for (int k = 0; k < 4; ++k)
      {
        const float w = replacement_weights[k];
        if (w > 0.0f)
        {
          const int b_id = replacement_bone_ids[k];

          // Apply the solved transform for this bone
          // Position uses the full Transform (Rotation + Translation)
          blended_pos += w * (bone_matrices[b_id] * pos);
        }
      }

      const Common::Vec3 blended_position(blended_pos.x(), blended_pos.y(), blended_pos.z());
      WriteVec3(write_ptr, chunk_declaration.position.offset, blended_position);
    }

    if (chunk_declaration.normals.size() > 0 && chunk_declaration.normals[0].enable)
    {
      const auto replacement_normal = ReadVec3(read_ptr, chunk_declaration.normals[0].offset);
      const Eigen::Vector3f normal(replacement_normal.x, replacement_normal.y,
                                   replacement_normal.z);

      Eigen::Vector3f blended_norm = Eigen::Vector3f::Zero();

      for (int k = 0; k < 4; ++k)
      {
        const float w = replacement_weights[k];
        if (w > 0.0f)
        {
          const int b_id = replacement_bone_ids[k];

          // Apply the solved transform for this bone
          // Normal uses ONLY the Rotation (Translation doesn't affect direction)
          blended_norm += w * (bone_matrices[b_id].linear() * normal);
        }
      }

      const Common::Vec3 blended_normal(blended_norm.x(), blended_norm.y(), blended_norm.z());
      WriteVec3(write_ptr, chunk_declaration.normals[0].offset, blended_normal.Normalized());
    }
  }
}

static bool IsValidTransform(const Eigen::Affine3f& transform)
{
  // 1. Check for NaNs (happens if SVD fails to converge or has 0-weight input)
  if (transform.matrix().array().isNaN().any())
    return false;

  // 2. Determinant Check (ensure it's a valid rotation, not a flat scale/reflection)
  // A pure rotation matrix must have a determinant of ~1.0
  float det = transform.linear().determinant();
  if (std::abs(det - 1.0f) > 0.01f)
    return false;

  return true;
}

namespace GraphicsModSystem::CPUSkinning
{
void UpdateSkeleton(GraphicsModSystem::DrawCallID draw_call,
                    const VideoCommon::SkinningRig& replacement_rig,
                    const std::vector<Eigen::Vector3f>& original_positions,
                    const GraphicsModSystem::DrawDataView& native_draw_data,
                    VideoCommon::SkeletonInstance* skeleton)
{
  auto it = replacement_rig.draw_call_rig_details.find(draw_call);
  if (it == replacement_rig.draw_call_rig_details.end())
    return;

  if (!skeleton) [[unlikely]]
    return;

  // 1. Partial Solve: Can we solve for any bones using ONLY this chunk?
  std::vector<Eigen::Affine3f> local_bone_solutions;
  std::vector<float> local_bone_weights;
  SolveChunkBoneTransforms(draw_call, replacement_rig, original_positions, native_draw_data,
                           local_bone_solutions, local_bone_weights);

  // 2. Update Global State:
  for (std::size_t bone_id = 0; bone_id < local_bone_solutions.size(); ++bone_id)
  {
    if (skeleton->solved_this_frame[bone_id])
      continue;

    // ONLY update if this chunk is a "Better" representative for this bone
    if (local_bone_weights[bone_id] > skeleton->best_confidence[bone_id])
    {
      if (IsValidTransform(local_bone_solutions[bone_id]))
      {
        skeleton->bone_matrices[bone_id] = local_bone_solutions[bone_id];
        skeleton->best_confidence[bone_id] = local_bone_weights[bone_id];
        skeleton->solved_this_frame[bone_id] = true;
      }
    }
  }
}

void ApplySkeleton(const VideoCommon::MeshResource::MeshChunk& replacement_chunk,
                   const VideoCommon::SkinningRig& replacement_rig,
                   const VideoCommon::SkeletonInstance& skeleton, std::vector<u8>* out)
{
  ApplyLinearBlendedSkinning(replacement_chunk, replacement_rig, skeleton.bone_matrices, out);
}
}  // namespace GraphicsModSystem::CPUSkinning
