// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"

#include "Core/System.h"

#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexLoaderManager.h"

namespace GraphicsModSystem::Runtime
{
void GraphicsModBackend::OnTextureCreate(const TextureView& texture)
{
  if (texture.texture_type == TextureType::XFB)
  {
    auto& system = Core::System::GetInstance();
    auto& custom_resource_manager = system.GetCustomResourceManager();
    custom_resource_manager.XFBTriggered();
    m_camera_manager.XFBTriggered();
  }
}

void GraphicsModBackend::OnFramePresented(const PresentInfo&)
{
}

void GraphicsModBackend::SetHostConfig(const ShaderHostConfig& config)
{
  m_shader_host_config.bits = config.bits;

  auto& system = Core::System::GetInstance();
  auto& custom_resource_manager = system.GetCustomResourceManager();
  custom_resource_manager.SetHostConfig(config);
}

void GraphicsModBackend::ClearAdditionalCameras(const MathUtil::Rectangle<int>& rc,
                                                bool color_enable, bool alpha_enable, bool z_enable,
                                                u32 color, u32 z)
{
  m_camera_manager.ClearCameraRenderTargets(rc, color_enable, alpha_enable, z_enable, color, z);
}

void GraphicsModBackend::CustomDraw(const DrawDataView& draw_data,
                                    VertexManagerBase* vertex_manager,
                                    std::span<GraphicsModAction*> actions, DrawCallID draw_call)
{
  bool skip = false;
  std::optional<Common::Matrix44> custom_transform;
  VideoCommon::MaterialResource* material_resource = nullptr;
  VideoCommon::MeshResource* mesh_resource = nullptr;
  bool ignore_mesh_transform = false;
  std::optional<GraphicsModSystem::Camera> camera;
  GraphicsModActionData::DrawStarted draw_started{draw_data,
                                                  VertexLoaderManager::g_current_components,
                                                  &skip,
                                                  &material_resource,
                                                  &mesh_resource,
                                                  &ignore_mesh_transform,
                                                  &custom_transform,
                                                  &camera};

  static Common::Matrix44 identity = Common::Matrix44::Identity();

  for (const auto& action : actions)
  {
    action->OnDrawStarted(&draw_started);
    if (camera)
    {
      m_camera_manager.EnsureCamera(draw_call, *camera);
    }
    if (mesh_resource)
    {
      const auto mesh_data = mesh_resource->GetData();
      if (mesh_data)
      {
        vertex_manager->DrawCustomMesh(draw_call, *mesh_data, draw_data,
                                       custom_transform.value_or(identity), ignore_mesh_transform,
                                       m_camera_manager);
        return;
      }
    }
  }

  if (skip)
  {
    return;
  }

  if (material_resource)
  {
    const auto material_data = material_resource->GetData();
    if (material_data)
    {
      vertex_manager->DrawEmulatedMesh(*material_data, draw_data,
                                       custom_transform.value_or(identity), m_camera_manager);
      return;
    }
  }

  vertex_manager->DrawEmulatedMesh(m_camera_manager, custom_transform.value_or(identity));
}

DrawCallID GraphicsModBackend::GetSkinnedDrawCallID(DrawCallID draw_call_id, MaterialID material_id,
                                                    const DrawDataView& draw_data)
{
  const bool is_draw_gpu_skinned =
      IsDrawGPUSkinned(draw_data.vertex_format, draw_data.uid->rasterization_state.primitive);
  if (is_draw_gpu_skinned && m_last_draw_gpu_skinned && m_last_material_id == material_id)
  {
    draw_call_id = m_last_draw_call_id;
  }
  m_last_draw_call_id = draw_call_id;
  m_last_material_id = material_id;
  m_last_draw_gpu_skinned = is_draw_gpu_skinned;

  return draw_call_id;
}

bool GraphicsModBackend::IsDrawGPUSkinned(NativeVertexFormat* format, PrimitiveType primitive_type)
{
  if (primitive_type != PrimitiveType::Triangles && primitive_type != PrimitiveType::TriangleStrip)
  {
    return false;
  }

  const PortableVertexDeclaration vert_decl = format->GetVertexDeclaration();
  return vert_decl.posmtx.enable;
}
}  // namespace GraphicsModSystem::Runtime
