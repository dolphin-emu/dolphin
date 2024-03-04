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
  XrSwapchain GetHandle() { return m_handle; }

  void Acquire();
  void Release();
  void SetCurrent();

private:
#if XR_USE_GRAPHICS_API_OPENGL_ES
  bool CreateGL(XrSession session, int width, int height);
#endif

  int m_width;
  int m_height;
  bool m_acquired;
  XrSwapchain m_handle;

  uint32_t m_swapchain_index;
  uint32_t m_swapchain_length;
  void* m_swapchain_image;

  unsigned int* m_gl_depth_buffers;
  unsigned int* m_gl_frame_buffers;
};
}  // namespace Common::VR
