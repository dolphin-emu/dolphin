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
    RenderTargetData::Color clear_color;
    float clear_depth_value = 0.0f;
    bool* should_clear;
    bool skip_orthographic = true;
  };
  CameraView GetCurrentCameraView();

  std::vector<CameraView> GetAdditionalViews();

  std::vector<GraphicsModSystem::DrawCallID> GetDrawCallsWithCameras() const;

  void ClearCameraRenderTargets(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                bool alpha_enable, bool z_enable, u32 color, u32 z);

  void XFBTriggered();

private:
  struct InternalCamera
  {
    std::shared_ptr<RenderTargetResource::Data> color_rt_data;
    CustomAsset::TimeType color_load_time = {};
    std::shared_ptr<RenderTargetResource::Data> depth_rt_data;
    CustomAsset::TimeType depth_load_time = {};
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    RenderTargetData::Color clear_color;
    float clear_depth_value = 0.0f;
    bool should_clear = true;
    GraphicsModSystem::Camera camera;
    GraphicsModSystem::DrawCallID id;
    u64 xfb = 0;
  };

  CameraView GetViewFromCamera(InternalCamera* camera);

  std::map<GraphicsModSystem::DrawCallID, InternalCamera> m_all_cameras;
  std::vector<InternalCamera*> m_additional_active_cameras;
  InternalCamera* m_current_camera = nullptr;

  u64 m_xfb = 0;
};
}  // namespace VideoCommon
