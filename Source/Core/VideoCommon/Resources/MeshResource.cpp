// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/MeshResource.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/Resources/MaterialResource.h"

namespace VideoCommon
{
namespace
{
GXPipelineUid CalculateUidForCustomMesh(const GXPipelineUid& original,
                                        const MeshDataChunk& mesh_chunk,
                                        NativeVertexFormat* vertex_format)
{
  GXPipelineUid result;
  memcpy(static_cast<void*>(&result), static_cast<const void*>(&original),
         sizeof(result));  // copy padding
  result.vertex_format = vertex_format;
  vertex_shader_uid_data* const vs_uid_data = result.vs_uid.GetUidData();
  vs_uid_data->components = mesh_chunk.components_available;

  auto& tex_coords = vertex_format->GetVertexDeclaration().texcoords;
  for (u32 i = 0; i < 8; ++i)
  {
    auto& texcoord = tex_coords[i];
    if (texcoord.enable)
    {
      if ((vs_uid_data->components & (VB_HAS_UV0 << i)) != 0)
      {
        auto& texinfo = vs_uid_data->texMtxInfo[i];
        if (texinfo.texgentype == TexGenType::Regular)
        {
          texinfo.texgentype = TexGenType::Passthrough;
          texinfo.inputform = TexInputForm::ABC1;
          texinfo.sourcerow = static_cast<SourceRow>(static_cast<u32>(SourceRow::Tex0) + i);
        }
      }
    }
  }

  // This is a bit hacky and we may need to revisit this change if there are problems
  // but this avoids mesh replacement needing know how it will be used
  // There are some games where numTexGens might NOT match the number of
  // active UV sets, see Tales Of Symphonia as an example
  vs_uid_data->numTexGens = 8;

  auto& colors = vertex_format->GetVertexDeclaration().colors;
  u32 color_count = 0;
  for (u32 i = 0; i < 2; ++i)
  {
    auto& color = colors[i];
    if (color.enable)
    {
      color_count++;
    }
  }
  vs_uid_data->numColorChans = color_count;

  vs_uid_data->dualTexTrans_enabled = false;

  pixel_shader_uid_data* const ps_uid_data = result.ps_uid.GetUidData();
  ps_uid_data->useDstAlpha = false;

  ps_uid_data->genMode_numindstages = 0;
  ps_uid_data->genMode_numtevstages = 0;
  ps_uid_data->genMode_numtexgens = vs_uid_data->numTexGens;
  ps_uid_data->bounding_box = false;
  ps_uid_data->rgba6_format = false;
  ps_uid_data->dither = false;
  ps_uid_data->uint_output = false;

  geometry_shader_uid_data* const gs_uid_data = result.gs_uid.GetUidData();
  gs_uid_data->primitive_type = static_cast<u32>(mesh_chunk.primitive_type);
  gs_uid_data->numTexGens = vs_uid_data->numTexGens;

  result.rasterization_state.primitive = mesh_chunk.primitive_type;
  return result;
}

const CustomAssetLibrary::AssetID& GetMaterialAssetFromName(std::string_view name,
                                                            const MeshData& mesh_data)
{
  if (const auto iter = mesh_data.m_mesh_material_to_material_asset_id.find(name);
      iter != mesh_data.m_mesh_material_to_material_asset_id.end())
  {
    return iter->second;
  }

  static CustomAssetLibrary::AssetID invalid = "";
  return invalid;
}
}  // namespace

MeshResource::MeshChunk::MeshChunk(CustomAssetLibrary::AssetID asset_id,
                                   const MeshDataChunk* data_chunk,
                                   const GXPipelineUid& original_uid)
    : m_asset_id(std::move(asset_id)), m_data_chunk(data_chunk),
      m_native_vertex_format(g_gfx->CreateNativeVertexFormat(m_data_chunk->vertex_declaration)),
      m_uid(CalculateUidForCustomMesh(original_uid, *data_chunk, m_native_vertex_format.get()))
{
}

MeshResource::MeshChunk::~MeshChunk() = default;

void MeshResource::MeshChunk::UpdateMaterial(CustomResourceManager* resource_manager,
                                             std::shared_ptr<CustomAssetLibrary> asset_library)
{
  m_material = resource_manager->GetMaterialFromAsset(m_asset_id, m_uid, std::move(asset_library));
}

MeshResource::MeshResource(Resource::ResourceContext resource_context,
                           const GXPipelineUid& pipeline_uid)
    : Resource(std::move(resource_context)), m_uid(pipeline_uid)
{
  m_mesh_asset = m_resource_context.asset_cache->CreateAsset<MeshAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
  m_uid_vertex_format_copy =
      g_gfx->CreateNativeVertexFormat(m_uid.vertex_format->GetVertexDeclaration());
  m_uid.vertex_format = m_uid_vertex_format_copy.get();
}

void MeshResource::MarkAsActive()
{
  if (!m_current_data) [[unlikely]]
    return;

  m_resource_context.asset_cache->MarkAssetActive(m_mesh_asset);

  for (const auto& chunk : m_current_data->m_mesh_chunks)
  {
    chunk.m_material->MarkAsActive();
  }

  for (const auto& [draw_call, chunks] : m_current_data->m_skinned_mesh_chunks)
  {
    for (const auto& chunk : chunks)
    {
      chunk.m_material->MarkAsActive();
    }
  }
}

void MeshResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_mesh_asset);
}

std::span<const MeshResource::MeshChunk>
MeshResource::Data::GetMeshChunks(GraphicsModSystem::DrawCallID draw_call) const
{
  if (const auto iter = m_skinned_mesh_chunks.find(draw_call); iter != m_skinned_mesh_chunks.end())
  {
    return iter->second;
  }

  return m_mesh_chunks;
}

const CPUSkinningData*
MeshResource::Data::GetCPUSkinningData(GraphicsModSystem::DrawCallID draw_call) const
{
  if (const auto iter = m_cpu_skinning_data.find(draw_call); iter != m_cpu_skinning_data.end())
  {
    return &iter->second;
  }

  return nullptr;
}

void MeshResource::Data::GenerateChunks(const GXPipelineUid& uid)
{
  for (const auto& [draw_call_id, chunks] : m_mesh_data->m_skinning_chunks)
  {
    auto& skinned_mesh_chunks = m_skinned_mesh_chunks[draw_call_id];
    for (const auto& chunk : chunks)
    {
      const auto& asset_id = GetMaterialAssetFromName(chunk.material_name, *m_mesh_data);
      if (asset_id == "")
        continue;
      skinned_mesh_chunks.emplace_back(asset_id, &chunk, uid);
    }
  }

  for (const auto& [draw_call_id, skinning_data] : m_mesh_data->m_cpu_skinning_data)
  {
    m_cpu_skinning_data[draw_call_id] = skinning_data;
  }

  for (const auto& chunk : m_mesh_data->m_mesh_chunks)
  {
    const auto& asset_id = GetMaterialAssetFromName(chunk.material_name, *m_mesh_data);
    if (asset_id == "")
      continue;
    m_mesh_chunks.emplace_back(asset_id, &chunk, uid);
  }
}

void MeshResource::ResetData()
{
  if (m_current_data)
  {
    for (auto& chunk : m_current_data->m_mesh_chunks)
    {
      chunk.m_material->RemoveReference(this);
    }

    for (auto& [draw_call, chunks] : m_current_data->m_skinned_mesh_chunks)
    {
      for (auto& chunk : chunks)
      {
        chunk.m_material->RemoveReference(this);
      }
    }
  }
  m_load_data = std::make_shared<Data>();
}

Resource::TaskComplete MeshResource::CollectPrimaryData()
{
  const auto mesh_data = m_mesh_asset->GetData();
  if (!mesh_data) [[unlikely]]
  {
    return TaskComplete::No;
  }
  m_load_data->m_mesh_data = mesh_data;
  m_load_data->GenerateChunks(m_uid);
  return TaskComplete::Yes;
}

Resource::TaskComplete MeshResource::CollectDependencyData()
{
  bool loaded = true;
  for (auto& chunk : m_load_data->m_mesh_chunks)
  {
    chunk.UpdateMaterial(m_resource_context.resource_manager, m_resource_context.asset_library);
    chunk.m_material->AddReference(this);

    const auto data_processed = chunk.m_material->IsDataProcessed();
    if (data_processed == TaskComplete::Error)
      return TaskComplete::Error;

    loaded &= data_processed == TaskComplete::Yes;
  }

  for (auto& [draw_call, chunks] : m_load_data->m_skinned_mesh_chunks)
  {
    for (auto& chunk : chunks)
    {
      chunk.UpdateMaterial(m_resource_context.resource_manager, m_resource_context.asset_library);
      chunk.m_material->AddReference(this);

      const auto data_processed = chunk.m_material->IsDataProcessed();
      if (data_processed == TaskComplete::Error)
        return TaskComplete::Error;

      loaded &= data_processed == TaskComplete::Yes;
    }
  }

  return loaded ? TaskComplete::Yes : TaskComplete::No;
}

Resource::TaskComplete MeshResource::ProcessData()
{
  std::swap(m_current_data, m_load_data);
  return TaskComplete::Yes;
}
}  // namespace VideoCommon
