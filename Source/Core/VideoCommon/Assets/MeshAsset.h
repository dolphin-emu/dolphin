// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <Eigen/Core>
#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderState.h"

namespace File
{
class IOFile;
}

namespace VideoCommon
{
struct LocalBoneGroup
{
  int bone_id;
  std::vector<int> welded_indices;
  std::vector<int> original_indices;
  std::vector<float> weights;  // Confidence for SVD
};

struct ChunkRigData
{
  GraphicsModSystem::DrawCallID draw_call_id;
  std::map<int, LocalBoneGroup> bone_groups;  // BoneID -> Group
};

class SkinningRig
{
public:
  std::vector<Eigen::Vector3f> welded_positions;
  float welded_rig_scale;
  Eigen::Vector3f welded_rig_centroid;
  std::vector<Eigen::Vector3f> bone_rest_centers;
  std::map<GraphicsModSystem::DrawCallID, ChunkRigData> draw_call_rig_details;

  static bool FromBinary(const u8* raw_data, std::size_t* offset, SkinningRig* data);
  static bool ToBinary(File::IOFile* file_data, const SkinningRig& data);
};

struct MeshDataChunk
{
  std::unique_ptr<u8[]> vertex_data;
  u32 vertex_stride;
  u32 num_vertices;
  std::unique_ptr<u16[]> indices;
  u32 num_indices;
  PortableVertexDeclaration vertex_declaration;
  PrimitiveType primitive_type;
  u32 components_available;
  Common::Vec3 minimum_position;
  Common::Vec3 maximum_position;
  Common::Matrix44 transform;
  std::string material_name;
};

struct MeshData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       MeshData* data);
  static void ToJson(picojson::object& obj, const MeshData& data);

  static bool FromDolphinMesh(std::span<const u8> raw_data, MeshData* data);

  static bool ToDolphinMesh(File::IOFile* file_data, const MeshData& data);

  static bool FromGLTF(std::string_view gltf_file, MeshData* data);

  void Report();

  std::vector<MeshDataChunk> m_mesh_chunks;
  std::map<std::string, CustomAssetLibrary::AssetID, std::less<>>
      m_mesh_material_to_material_asset_id;

  std::map<GraphicsModSystem::DrawCallID, std::vector<MeshDataChunk>> m_skinning_chunks;
  std::optional<SkinningRig> m_cpu_skinning_rig;

  std::map<GraphicsModSystem::DrawCallID, std::vector<Eigen::Vector3f>> m_original_positions;
};

class MeshAsset final : public CustomLoadableAsset<MeshData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
