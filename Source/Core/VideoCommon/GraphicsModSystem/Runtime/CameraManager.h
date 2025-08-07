// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/Resources/RenderTargetResource.h"

namespace VideoCommon
{
class CameraManager
{
public:
  void EnsureCamera(GraphicsModSystem::DrawCallID originating_draw_call_id,
                    GraphicsModSystem::Camera camera);
  struct CameraView
  {
    GraphicsModSystem::DrawCallID id;
    AbstractFramebuffer* framebuffer;
    std::optional<Common::Matrix44> transform;
    bool skip_orthographic = true;
  };
  CameraView GetCurrentCameraView();

  std::vector<CameraView> GetAdditionalViews();

  std::vector<GraphicsModSystem::DrawCallID> GetDrawCallsWithCameras() const;

  void ClearCameraRenderTargets(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                bool alpha_enable, bool z_enable, u32 color, u32 z);

  void FrameUpdate(u64 frame);

private:
  struct InternalCamera
  {
    std::shared_ptr<RenderTargetResource::Data> render_target_data;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    GraphicsModSystem::Camera camera;
    GraphicsModSystem::DrawCallID id;
    u64 frame = 0;
  };

  CameraView GetViewFromCamera(InternalCamera* camera);

  std::map<GraphicsModSystem::DrawCallID, InternalCamera> m_all_cameras;
  std::vector<InternalCamera*> m_additional_active_cameras;
  InternalCamera* m_current_camera = nullptr;

  u64 m_last_frame = 0;
};
}  // namespace VideoCommon
