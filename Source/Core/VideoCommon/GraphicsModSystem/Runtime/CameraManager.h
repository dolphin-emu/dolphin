// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache2.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace VideoCommon
{
class CameraManager
{
public:
  void EnsureCamera(GraphicsModSystem::DrawCallID originating_draw_call_id,
                    GraphicsModSystem::Camera camera);
  AbstractTexture* GetRenderTarget(GraphicsModSystem::DrawCallID originating_draw_call_id,
                                   std::string_view render_target_name) const;

  using RenderTargetMap = std::map<std::string_view, AbstractTexture*>;
  RenderTargetMap GetRenderTargets(GraphicsModSystem::DrawCallID originating_draw_call_id) const;

  struct CameraView
  {
    GraphicsModSystem::DrawCallID id;
    AbstractFramebuffer* framebuffer;
    std::optional<Common::Matrix44> transform;
    bool skip_orthographic = true;
  };
  CameraView
  GetCurrentCameraView(std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets);

  std::vector<CameraView>
  GetAdditionalViews(std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets);

  void ClearCameraRenderTargets(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                bool alpha_enable, bool z_enable, u32 color, u32 z);

  void FrameUpdate(u64 frame);

private:
  struct InternalCamera
  {
    GraphicsModSystem::Camera camera;
    GraphicsModSystem::DrawCallID id;
    u64 frame = 0;
  };

  AbstractTexture*
  EnsureRenderTarget(InternalCamera* camera,
                     const GraphicsModSystem::OutputRenderTargetResource& render_target);

  CameraView
  GetViewFromCamera(InternalCamera* camera,
                    std::span<GraphicsModSystem::OutputRenderTargetResource> render_targets);

  std::map<u64, std::unique_ptr<AbstractFramebuffer>> m_render_target_camera_hash_to_framebuffers;
  std::map<GraphicsModSystem::DrawCallID, RenderTargetMap> m_camera_draw_call_to_render_target;

  CustomTextureCache2 m_texture_cache;

  std::map<GraphicsModSystem::DrawCallID, InternalCamera> m_all_cameras;
  std::vector<InternalCamera*> m_additional_active_cameras;
  InternalCamera* m_current_camera = nullptr;

  u64 m_last_frame = 0;
};
}  // namespace VideoCommon
