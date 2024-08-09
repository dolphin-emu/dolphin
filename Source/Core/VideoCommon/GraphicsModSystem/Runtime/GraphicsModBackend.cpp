// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"

#include "Common/SmallVector.h"
#include "Core/System.h"

#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModSystem::Runtime
{
namespace
{
VideoCommon::GXPipelineUid GetPipelineState(const GraphicsModActionData::MeshChunk& mesh_chunk)
{
  VideoCommon::GXPipelineUid result;
  result.vertex_format = mesh_chunk.vertex_format;
  result.vs_uid = GetVertexShaderUid();
  vertex_shader_uid_data* const vs_uid_data = result.vs_uid.GetUidData();
  vs_uid_data->components = mesh_chunk.components_available;

  auto& tex_coords = mesh_chunk.vertex_format->GetVertexDeclaration().texcoords;
  u32 texcoord_count = 0;
  for (u32 i = 0; i < 8; ++i)
  {
    auto& texcoord = tex_coords[i];
    if (texcoord.enable)
    {
      if ((vs_uid_data->components & (VB_HAS_UV0 << i)) != 0)
      {
        auto& texinfo = vs_uid_data->texMtxInfo[texcoord_count];
        texinfo.texgentype = TexGenType::Passthrough;
        texinfo.inputform = TexInputForm::ABC1;
        texinfo.sourcerow = static_cast<SourceRow>(static_cast<u32>(SourceRow::Tex0) + i);
      }
      texcoord_count++;
    }
  }
  vs_uid_data->numTexGens = texcoord_count;

  auto& colors = mesh_chunk.vertex_format->GetVertexDeclaration().colors;
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

  result.ps_uid = GetPixelShaderUid();
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

  result.rasterization_state.cullmode = mesh_chunk.cull_mode;
  result.rasterization_state.primitive = mesh_chunk.primitive_type;
  result.depth_state.func = CompareMode::LEqual;
  result.depth_state.testenable = true;
  result.depth_state.updateenable = true;
  result.blending_state = RenderState::GetNoBlendingBlendState();
  // result.depth_state.Generate(bpmem);
  // result.blending_state.Generate(bpmem);
  /*result.blending_state.blendenable = true;
  result.blending_state.srcfactor = SrcBlendFactor::SrcAlpha;
  result.blending_state.dstfactor = DstBlendFactor::InvSrcAlpha;
  result.blending_state.srcfactoralpha = SrcBlendFactor::Zero;
  result.blending_state.dstfactoralpha = DstBlendFactor::One;*/

  return result;
}

VideoCommon::GXUberPipelineUid
GetUberPipelineState(const GraphicsModActionData::MeshChunk& mesh_chunk)
{
  VideoCommon::GXUberPipelineUid result;
  result.vertex_format =
      VertexLoaderManager::GetUberVertexFormat(mesh_chunk.vertex_format->GetVertexDeclaration());
  result.vs_uid = UberShader::GetVertexShaderUid();
  UberShader::vertex_ubershader_uid_data* const vs_uid_data = result.vs_uid.GetUidData();

  auto& tex_coords = mesh_chunk.vertex_format->GetVertexDeclaration().texcoords;
  u32 texcoord_count = 0;
  for (u32 i = 0; i < 8; ++i)
  {
    auto& texcoord = tex_coords[i];
    if (texcoord.enable)
    {
      texcoord_count++;
    }
  }
  vs_uid_data->num_texgens = texcoord_count;

  result.ps_uid = UberShader::GetPixelShaderUid();
  UberShader::pixel_ubershader_uid_data* const ps_uid_data = result.ps_uid.GetUidData();
  ps_uid_data->num_texgens = vs_uid_data->num_texgens;
  ps_uid_data->uint_output = false;

  geometry_shader_uid_data* const gs_uid_data = result.gs_uid.GetUidData();
  gs_uid_data->primitive_type = static_cast<u32>(mesh_chunk.primitive_type);
  gs_uid_data->numTexGens = vs_uid_data->num_texgens;

  result.rasterization_state.cullmode = mesh_chunk.cull_mode;
  result.rasterization_state.primitive = mesh_chunk.primitive_type;
  result.depth_state.func = CompareMode::LEqual;
  result.depth_state.testenable = true;
  result.depth_state.updateenable = true;
  result.blending_state = RenderState::GetNoBlendingBlendState();
  // result.depth_state.Generate(bpmem);
  // result.blending_state.Generate(bpmem);
  /*result.blending_state.blendenable = true;
  result.blending_state.srcfactor = SrcBlendFactor::SrcAlpha;
  result.blending_state.dstfactor = DstBlendFactor::InvSrcAlpha;
  result.blending_state.srcfactoralpha = SrcBlendFactor::Zero;
  result.blending_state.dstfactoralpha = DstBlendFactor::One;*/

  return result;
}
bool IsDrawGPUSkinned(NativeVertexFormat* format, PrimitiveType primitive_type)
{
  if (primitive_type != PrimitiveType::Triangles && primitive_type != PrimitiveType::TriangleStrip)
  {
    return false;
  }

  const PortableVertexDeclaration vert_decl = format->GetVertexDeclaration();
  return vert_decl.posmtx.enable;
}
}  // namespace
void GraphicsModBackend::CustomDraw(const DrawDataView& draw_data,
                                    VertexManagerBase* vertex_manager,
                                    std::span<GraphicsModAction*> actions)
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();

  CustomPixelShaderContents custom_pixel_shader_contents;
  std::optional<CustomPixelShader> custom_pixel_shader;
  std::optional<Common::Matrix44> custom_transform;
  std::span<u8> custom_pixel_shader_uniforms;
  bool skip = false;
  bool more_data = true;
  u32 mesh_index = 0;
  std::optional<GraphicsModActionData::MeshChunk> mesh_chunk;
  GraphicsModActionData::DrawStarted draw_started{draw_data,
                                                  VertexLoaderManager::g_current_components,
                                                  &skip,
                                                  &custom_pixel_shader,
                                                  &custom_pixel_shader_uniforms,
                                                  &custom_transform,
                                                  &mesh_chunk,
                                                  &mesh_index,
                                                  &more_data};

  for (const auto& action : actions)
  {
    more_data = true;
    mesh_index = 0;
    while (more_data)
    {
      more_data = false;
      action->OnDrawStarted(&draw_started);
      if (custom_pixel_shader)
      {
        custom_pixel_shader_contents.shaders.clear();
        custom_pixel_shader_contents.shaders.push_back(*custom_pixel_shader);
      }
      custom_pixel_shader = std::nullopt;

      if (mesh_chunk)
      {
        vertex_shader_manager.SetVertexFormat(mesh_chunk->components_available,
                                              mesh_chunk->vertex_format->GetVertexDeclaration());
        vertex_manager->DrawCustomMesh(
            custom_pixel_shader_contents, mesh_chunk->transform, custom_pixel_shader_uniforms,
            mesh_chunk->vertex_data, mesh_chunk->index_data, mesh_chunk->primitive_type,
            mesh_chunk->vertex_format->GetVertexStride(), GetPipelineState(*mesh_chunk),
            GetUberPipelineState(*mesh_chunk));

        // For now skip the normal mesh
        skip = true;
      }
    }
  }

  if (!skip)
  {
    vertex_manager->DrawEmulatedMesh(custom_pixel_shader_contents, custom_transform,
                                     custom_pixel_shader_uniforms);
  }
}

DrawCallID GraphicsModBackend::GetSkinnedDrawCallID(DrawCallID draw_call_id, MaterialID material_id,
                                                    const DrawDataView& draw_data)
{
  const bool is_draw_gpu_skinned =
      IsDrawGPUSkinned(draw_data.vertex_format, draw_data.rasterization_state.primitive);
  if (is_draw_gpu_skinned && m_last_draw_gpu_skinned && m_last_material_id == material_id)
  {
    draw_call_id = m_last_draw_call_id;
  }
  m_last_draw_call_id = draw_call_id;
  m_last_material_id = material_id;
  m_last_draw_gpu_skinned = is_draw_gpu_skinned;

  return draw_call_id;
}
}  // namespace GraphicsModSystem::Runtime
