// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CameraManager.h"

#include <algorithm>

#include <fmt/format.h>
#include <xxh3.h>

#include "Common/EnumUtils.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/FramebufferManager.h"
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

  const bool inactive = internal_camera.frame == 0;
  internal_camera.frame = m_last_frame;

  if (internal_camera.camera.generates_render_targets)
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

AbstractTexture*
CameraManager::GetRenderTarget(GraphicsModSystem::DrawCallID originating_draw_call_id,
                               std::string_view render_target_name) const
{
  // If we have the default camera, we use special names for the color/depth buffer
  // managed by Dolphin
  if (originating_draw_call_id == GraphicsModSystem::DrawCallID::INVALID)
  {
    if (render_target_name == "color")
    {
      return g_gfx->GetCurrentFramebuffer()->GetColorAttachment();
    }
    else if (render_target_name == "depth")
    {
      return g_gfx->GetCurrentFramebuffer()->GetDepthAttachment();
    }
  }

  if (const auto iter = m_camera_draw_call_to_render_target.find(originating_draw_call_id);
      iter != m_camera_draw_call_to_render_target.end())
  {
    if (const auto render_iter = iter->second.find(render_target_name);
        render_iter != iter->second.end())
    {
      return render_iter->second;
    }
  }

  return nullptr;
}

CameraManager::RenderTargetMap
CameraManager::GetRenderTargets(GraphicsModSystem::DrawCallID originating_draw_call_id) const
{
  if (const auto iter = m_camera_draw_call_to_render_target.find(originating_draw_call_id);
      iter != m_camera_draw_call_to_render_target.end())
  {
    return iter->second;
  }

  return {};
}

CameraManager::CameraView CameraManager::GetCurrentCameraView(
    std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets)
{
  if (render_targets.empty())
  {
    const GraphicsModSystem::DrawCallID draw_call_id =
        m_current_camera ? m_current_camera->id : GraphicsModSystem::DrawCallID::INVALID;
    const auto transform =
        m_current_camera ? std::make_optional<>(m_current_camera->camera.transform) : std::nullopt;
    return {draw_call_id, nullptr, transform};
  }
  return GetViewFromCamera(m_current_camera, render_targets);
}

std::vector<CameraManager::CameraView> CameraManager::GetAdditionalViews(
    std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets)
{
  std::vector<CameraManager::CameraView> result;
  for (const auto& camera : m_additional_active_cameras)
  {
    result.push_back(GetViewFromCamera(camera, render_targets));
  }
  return result;
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
      //                            g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f :
        //                                                                                    0.0f);
  }
  g_gfx->SetFramebuffer(frame_buffer);*/

  const auto frame_buffer = g_gfx->GetCurrentFramebuffer();
  for (const auto& [id, target_frame_buffer] : m_render_target_camera_hash_to_framebuffers)
  {
    g_gfx->SetAndClearFramebuffer(target_frame_buffer.get(), {{0.0f, 0.0f, 0.0f, 1.0f}},
                                  g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f :
                                                                                            0.0f);
  }
  g_gfx->SetFramebuffer(frame_buffer);
}

void CameraManager::FrameUpdate(u64 frame)
{
  std::vector<GraphicsModSystem::DrawCallID> ids_to_return_to_pool;

  // Can the current camera be reset?
  if (m_current_camera && (m_current_camera->frame + CAMERA_FRAME_LIVE_TIME) < m_last_frame)
  {
    m_current_camera->frame = 0;
    ids_to_return_to_pool.push_back(GraphicsModSystem::DrawCallID::INVALID);
    m_current_camera = nullptr;
  }

  // Find additional cameras that can be reset
  for (const auto& camera : m_additional_active_cameras)
  {
    if (camera->frame != m_last_frame)
    {
      ids_to_return_to_pool.push_back(camera->id);
      camera->frame = 0;
    }
  }
  // Now we can erase them
  std::erase_if(m_additional_active_cameras, [&](InternalCamera* camera) {
    return (camera->frame + CAMERA_FRAME_LIVE_TIME) < m_last_frame;
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

  const auto frame_buffer = g_gfx->GetCurrentFramebuffer();
  for (const auto& [id, target_frame_buffer] : m_render_target_camera_hash_to_framebuffers)
  {
    g_gfx->SetAndClearFramebuffer(target_frame_buffer.get(), {{0.0f, 0.0f, 0.0f, 1.0f}},
                                  g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f :
                                                                                            0.0f);
  }
  g_gfx->SetFramebuffer(frame_buffer);

  m_last_frame = frame;
}

AbstractTexture* CameraManager::EnsureRenderTarget(
    InternalCamera* camera, const GraphicsModSystem::OutputRenderTargetResource& render_target)
{
  const auto id = camera ? camera->id : GraphicsModSystem::DrawCallID::INVALID;
  auto& render_target_map = m_camera_draw_call_to_render_target[id];
  const auto [it, added] = render_target_map.try_emplace(render_target.name, nullptr);
  if (added)
  {
    const float resolution_multiplier = camera ? camera->camera.resolution_multiplier : 1.0;
    const u32 width = g_framebuffer_manager->GetEFBWidth() * resolution_multiplier;
    const u32 height = g_framebuffer_manager->GetEFBHeight() * resolution_multiplier;
    const TextureConfig texture_config(
        width, height, 1, 1, 1, render_target.format, AbstractTextureFlag_RenderTarget,
        camera ? camera->camera.render_target_type : AbstractTextureType::Texture_2D);

    // If we have the default camera, we use special names for the color/depth buffer
    // managed by Dolphin
    if (id == GraphicsModSystem::DrawCallID::INVALID)
    {
      if (render_target.name == "color")
      {
        it->second = g_gfx->GetCurrentFramebuffer()->GetColorAttachment();
        return it->second;
      }
      else if (render_target.name == "depth")
      {
        it->second = g_gfx->GetCurrentFramebuffer()->GetDepthAttachment();
        return it->second;
      }
    }

    // Otherwise it's a custom render target, we must create it
    const auto full_name = fmt::format("{}_{}", Common::ToUnderlying(id), render_target.name);
    if (const auto texture = m_texture_cache.GetTextureFromConfig(full_name, texture_config))
    {
      it->second = *texture;
    }
  }

  return it->second;
}

CameraManager::CameraView CameraManager::GetViewFromCamera(
    InternalCamera* camera, std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets)
{
  XXH3_state_t view_state;
  XXH3_INITSTATE(&view_state);
  XXH3_64bits_reset_withSeed(&view_state, static_cast<XXH64_hash_t>(1));

  if (camera)
  {
    XXH3_64bits_update(&view_state, &camera->id, sizeof(GraphicsModSystem::DrawCallID));
    XXH3_64bits_update(&view_state, &camera->camera, sizeof(GraphicsModSystem::Camera));
  }
  for (const auto& render_target : render_targets)
  {
    XXH3_64bits_update(&view_state, render_target.name.data(), render_target.name.size());
    XXH3_64bits_update(&view_state, &render_target.format, sizeof(render_target.format));
  }

  const auto [it, added] = m_render_target_camera_hash_to_framebuffers.try_emplace(
      XXH3_64bits_digest(&view_state), nullptr);
  if (added)
  {
    static const GraphicsModSystem::OutputRenderTargetResource color_target_resource{
        .format = g_framebuffer_manager->GetEFBColorFormat(), .name = "color"};
    static const GraphicsModSystem::OutputRenderTargetResource depth_target_resource{
        .format = g_framebuffer_manager->GetEFBDepthFormat(), .name = "depth"};
    const auto color_target = EnsureRenderTarget(camera, color_target_resource);
    const auto depth_target = EnsureRenderTarget(camera, depth_target_resource);
    std::vector<AbstractTexture*> additional_render_targets;
    for (const auto& render_target : render_targets)
    {
      additional_render_targets.push_back(EnsureRenderTarget(camera, render_target));
    }
    it->second =
        g_gfx->CreateFramebuffer(color_target, depth_target, std::move(additional_render_targets));
  }

  const GraphicsModSystem::DrawCallID draw_call_id =
      camera ? camera->id : GraphicsModSystem::DrawCallID::INVALID;
  const auto transform = camera ? std::make_optional<>(camera->camera.transform) : std::nullopt;
  return {draw_call_id, it->second.get(), transform};
}
}  // namespace VideoCommon
