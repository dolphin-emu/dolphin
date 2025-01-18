// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <span>
#include <string_view>

#include "VideoCommon/Resources/Resource.h"

#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/NativeVertexFormat.h"

class NativeVertexFormat;
namespace VideoCommon
{
class MaterialResource;
class MeshResource final : public Resource
{
public:
  MeshResource(Resource::ResourceContext resource_context, const GXPipelineUid& pipeline_uid);

  void MarkAsActive() override;
  void MarkAsPending() override;

  class MeshChunk
  {
  public:
    MeshChunk(CustomAssetLibrary::AssetID asset_id, const MeshDataChunk* data_chunk,
              const GXPipelineUid& original_uid);
    MeshChunk(const MeshChunk&) = delete;
    MeshChunk(MeshChunk&&) = default;
    MeshChunk& operator=(const MeshChunk&) = delete;
    MeshChunk& operator=(MeshChunk&&) = default;
    ~MeshChunk();

    std::span<const u8> GetVertexData() const
    {
      return std::span<const u8>(m_data_chunk->vertex_data.get(), m_data_chunk->num_vertices);
    }
    std::span<const u16> GetIndexData() const
    {
      return std::span<const u16>(m_data_chunk->indices.get(), m_data_chunk->num_indices);
    }
    u32 GetVertexStride() const { return m_data_chunk->vertex_stride; }
    u32 GetVertexCount() const { return m_data_chunk->num_vertices; }
    PrimitiveType GetPrimitiveType() const { return m_data_chunk->primitive_type; }
    u32 GetComponentsAvailable() const { return m_data_chunk->components_available; }
    Common::Matrix44 GetTransform() const { return m_data_chunk->transform; }
    const MaterialResource* GetMaterial() const { return m_material; }
    void SetMaterial(MaterialResource* material) { m_material = material; }
    NativeVertexFormat* GetNativeVertexFormat() const { return m_native_vertex_format.get(); }
    Common::Vec3 GetPivotPoint() const { return Common::Vec3{}; }

  private:
    friend class MeshResource;
    void UpdateMaterial(CustomResourceManager* resource_manager,
                        std::shared_ptr<CustomAssetLibrary> asset_library);

    CustomAssetLibrary::AssetID m_asset_id;
    MaterialResource* m_material = nullptr;
    const MeshDataChunk* m_data_chunk = nullptr;
    std::unique_ptr<NativeVertexFormat> m_native_vertex_format;
    GXPipelineUid m_uid;
  };

  class Data
  {
  public:
    std::span<const MeshChunk> GetMeshChunks(GraphicsModSystem::DrawCallID draw_call) const;
    const CPUSkinningData* GetCPUSkinningData(GraphicsModSystem::DrawCallID draw_call) const;

  private:
    friend class MeshResource;
    void GenerateChunks(const GXPipelineUid& uid);

    std::map<std::string, CustomAssetLibrary::AssetID, std::less<>> m_name_to_material_id;
    std::shared_ptr<MeshData> m_mesh_data = nullptr;
    std::map<GraphicsModSystem::DrawCallID, std::vector<MeshChunk>> m_skinned_mesh_chunks;
    std::map<GraphicsModSystem::DrawCallID, CPUSkinningData> m_cpu_skinning_data;
    std::vector<MeshChunk> m_mesh_chunks;
    std::atomic_bool m_processing_finished;
  };

  const std::shared_ptr<Data>& GetData() const { return m_current_data; }

private:
  void ResetData() override;
  TaskComplete CollectPrimaryData() override;
  TaskComplete CollectDependencyData() override;
  TaskComplete ProcessData() override;

  std::shared_ptr<Data> m_current_data;
  std::shared_ptr<Data> m_load_data;

  MeshAsset* m_mesh_asset = nullptr;

  GXPipelineUid m_uid;
  std::unique_ptr<NativeVertexFormat> m_uid_vertex_format_copy;
};
}  // namespace VideoCommon
