// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CameraManager.h"

#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
constexpr u64 CAMERA_FRAME_LIVE_TIME = 5;
}

namespace VideoCommon
{
void CameraManager::EnsureCamera(GraphicsModSystem::DrawCallID originating_draw_call_id,
                                 GraphicsModSystem::Camera camera)
{
  auto& internal_camera = m_all_cameras[originating_draw_call_id];
  internal_camera.camera = std::move(camera);

  // Note: invalid denotes the default Dolphin camera
  internal_camera.id = GraphicsModSystem::DrawCallID::INVALID;

  const bool inactive = internal_camera.xfb == 0;
  internal_camera.xfb = m_xfb;

  if (internal_camera.camera.color_asset_id != "" || internal_camera.camera.depth_asset_id != "")
  {
    bool camera_changed = false;
    if (m_current_camera == &internal_camera)
    {
      // Camera changed by user?
      m_current_camera = nullptr;
      camera_changed = true;
    }

    internal_camera.id = originating_draw_call_id;
    if (inactive || camera_changed)
    {
      m_additional_active_cameras.push_back(&internal_camera);
    }
  }
  else
  {
    m_current_camera = &internal_camera;
  }
}

CameraManager::CameraView CameraManager::GetCurrentCameraView()
{
  return GetViewFromCamera(m_current_camera);
}

std::vector<CameraManager::CameraView> CameraManager::GetAdditionalViews()
{
  std::vector<CameraManager::CameraView> result;
  for (const auto& camera : m_additional_active_cameras)
  {
    if (camera->xfb != m_xfb)
      result.push_back(GetViewFromCamera(camera));
  }
  return result;
}

std::vector<GraphicsModSystem::DrawCallID> CameraManager::GetDrawCallsWithCameras() const
{
  std::vector<GraphicsModSystem::DrawCallID> draw_calls;
  for (const auto& [draw_call, camera] : m_all_cameras)
  {
    draw_calls.push_back(draw_call);
  }

  return draw_calls;
}

void CameraManager::ClearCameraRenderTargets(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                             bool alpha_enable, bool z_enable, u32 color, u32 z)
{
  /*const auto frame_buffer = g_gfx->GetCurrentFramebuffer();
  for (const auto& [id, target_frame_buffer] : m_render_target_camera_hash_to_framebuffers)
  {
    g_gfx->SetFramebuffer(target_frame_buffer.get());
    // Native -> EFB coordinates
    MathUtil::Rectangle<int> target_rc = g_framebuffer_manager->ConvertEFBRectangle(rc);
    target_rc = g_gfx->ConvertFramebufferRectangle(target_rc, target_frame_buffer.get());
    target_rc.ClampUL(0, 0, target_frame_buffer->GetWidth(), target_frame_buffer->GetWidth());

    // Determine whether the EFB has an alpha channel. If it doesn't, we can clear the alpha
    // channel to 0xFF.
    // On backends that don't allow masking Alpha clears, this allows us to use the fast path
    // almost all the time
    if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16 ||
        bpmem.zcontrol.pixel_format == PixelFormat::RGB8_Z24 ||
        bpmem.zcontrol.pixel_format == PixelFormat::Z24)
    {
      // Force alpha writes, and clear the alpha channel.
      alpha_enable = true;
      color &= 0x00FFFFFF;
    }

    g_gfx->ClearRegion(target_rc, color_enable, alpha_enable, z_enable, color, z);
    //g_gfx->SetAndClearFramebuffer(target_frame_buffer.get(), {{0.0f, 0.0f, 0.0f, 1.0f}},
      //                            g_backend_info.bSupportsReversedDepthRange ? 1.0f :
        //                                                                                    0.0f);
  }
  g_gfx->SetFramebuffer(frame_buffer);*/

  // const auto frame_buffer = g_gfx->GetCurrentFramebuffer();
  for (auto& [id, camera] : m_all_cameras)
  {
    camera.should_clear = true;
  }
  // g_gfx->SetFramebuffer(frame_buffer);
}

void CameraManager::XFBTriggered()
{
  std::vector<GraphicsModSystem::DrawCallID> ids_to_return_to_pool;

  // Can the current camera be reset?
  if (m_current_camera && (m_current_camera->xfb + CAMERA_FRAME_LIVE_TIME) < m_xfb)
  {
    m_current_camera->xfb = 0;
    ids_to_return_to_pool.push_back(GraphicsModSystem::DrawCallID::INVALID);
    m_current_camera = nullptr;
  }

  // Find additional cameras that can be reset
  for (const auto& camera : m_additional_active_cameras)
  {
    if (camera->xfb != m_xfb)
    {
      ids_to_return_to_pool.push_back(camera->id);
      camera->xfb = 0;
    }
  }
  // Now we can erase them
  std::erase_if(m_additional_active_cameras, [&](InternalCamera* camera) {
    return (camera->xfb + CAMERA_FRAME_LIVE_TIME) < m_xfb;
  });

  // Return any render targets to the pool
  /*for (const auto id : ids_to_return_to_pool)
  {
    auto& render_target_map = m_camera_draw_call_to_render_target[id];
    for (const auto& [name, texture] : render_target_map)
    {
      const auto full_name = fmt::format("{}_{}", Common::ToUnderlying(id), name);
      m_texture_cache.ReleaseToPool(full_name);
    }
    m_camera_draw_call_to_render_target.erase(id);
  }*/

  /*const auto frame_buffer = g_gfx->GetCurrentFramebuffer();
  for (const auto& [id, camera] : m_all_cameras)
  {
    if (!camera.framebuffer)
      continue;

    g_gfx->SetAndClearFramebuffer(camera.framebuffer.get(), {{0.0f, 0.0f, 0.0f, 1.0f}},
                                  g_backend_info.bSupportsReversedDepthRange ? 1.0f : 0.0f);
  }
  g_gfx->SetFramebuffer(frame_buffer);*/

  for (auto& [id, camera] : m_all_cameras)
  {
    camera.should_clear = true;
  }

  m_xfb++;
}

CameraManager::CameraView CameraManager::GetViewFromCamera(InternalCamera* camera)
{
  if (!camera)
    return {};

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();

  std::shared_ptr<VideoCommon::RenderTargetResource::Data> color_data;
  std::shared_ptr<VideoCommon::RenderTargetResource::Data> depth_data;

  if (camera->camera.color_asset_id != "")
  {
    const auto color_resource = resource_manager.GetRenderTargetFromAsset(
        camera->camera.color_asset_id, camera->camera.asset_library);
    color_data = color_resource->GetData();
  }

  if (camera->camera.depth_asset_id != "")
  {
    const auto depth_resource = resource_manager.GetRenderTargetFromAsset(
        camera->camera.depth_asset_id, camera->camera.asset_library);
    depth_data = depth_resource->GetData();
  }

  if ((!color_data && camera->camera.color_asset_id != "") ||
      (!depth_data && camera->camera.depth_asset_id != ""))
    return {};

  const auto color_load_time = color_data ? color_data->GetLoadTime() : CustomAsset::TimeType{};
  const auto depth_load_time = depth_data ? depth_data->GetLoadTime() : CustomAsset::TimeType{};
  if (color_load_time > camera->color_load_time || depth_load_time > camera->depth_load_time)
  {
    camera->color_rt_data = color_data;
    camera->depth_rt_data = depth_data;

    AbstractTexture* color_texture = nullptr;
    if (color_data)
    {
      camera->color_load_time = color_data->GetLoadTime();
      camera->clear_color = color_data->GetClearColor();
      camera->clear_color[0] /= 255.0f;
      camera->clear_color[1] /= 255.0f;
      camera->clear_color[2] /= 255.0f;
      color_texture = color_data->GetTexture();
    }
    else
    {
      camera->color_load_time = CustomAsset::TimeType{};
      camera->clear_color = RenderTargetData::Color{};
    }

    AbstractTexture* depth_texture = nullptr;
    if (depth_data)
    {
      camera->depth_load_time = depth_data->GetLoadTime();
      camera->clear_depth_value = depth_data->GetClearColor()[0];
      camera->clear_depth_value /= 255.0f;
      depth_texture = depth_data->GetTexture();
    }
    else
    {
      camera->depth_load_time = CustomAsset::TimeType{};
      camera->clear_depth_value = 0.0f;
    }
    camera->framebuffer = g_gfx->CreateFramebuffer(color_texture, depth_texture, {});
  }

  CameraManager::CameraView result;
  result.framebuffer = camera->framebuffer.get();
  result.id = camera->id;
  result.skip_orthographic = camera->camera.skip_orthographic;
  result.transform = camera->camera.transform;
  result.should_clear = &camera->should_clear;
  if (*result.should_clear)
  {
    result.clear_color = camera->clear_color;
    result.clear_depth_value = camera->clear_depth_value;
  }
  return result;
}
}  // namespace VideoCommon
