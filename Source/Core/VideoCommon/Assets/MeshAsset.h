// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderState.h"

namespace File
{
class IOFile;
}

namespace VideoCommon
{
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

  std::vector<MeshDataChunk> m_mesh_chunks;
  std::map<std::string, CustomAssetLibrary::AssetID, std::less<>>
      m_mesh_material_to_material_asset_id;
};

class MeshAsset final : public CustomLoadableAsset<MeshData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};
}  // namespace VideoCommon
