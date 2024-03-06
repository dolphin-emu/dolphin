// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/OpenXRLoader.h"

namespace Common::VR
{
class Framebuffer
{
public:
  bool Create(XrSession session, int width, int height);
  void Destroy();

  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
  XrSwapchain GetHandle() { return m_color_handle; }

  void Acquire();
  void Release();
  void SetCurrent();

private:
  XrSwapchainImageBaseHeader* CreateImages(XrSwapchain handle);
  void CreateSwapchain(XrSession session, XrSwapchain& handle, int64_t format, XrFlags64 usage);

  int m_width;
  int m_height;
  bool m_acquired;
  uint32_t m_index;
  uint32_t m_length;

  XrSwapchain m_color_handle;
  XrSwapchain m_depth_handle;
  XrSwapchainImageBaseHeader* m_color_images;
  XrSwapchainImageBaseHeader* m_depth_images;
  void* m_frame_buffers;
};
}  // namespace Common::VR
