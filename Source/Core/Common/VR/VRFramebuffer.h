// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/OpenXRLoader.h"

namespace Common::VR
{
class Framebuffer
{
public:
  bool Create(XrSession session, int width, int height, bool multiview);
  void Destroy();

  int GetWidth() { return width; }
  int GetHeight() { return height; }
  XrSwapchain GetHandle() { return handle; }

  void Acquire();
  void Release();
  void SetCurrent();

private:
#if XR_USE_GRAPHICS_API_OPENGL_ES
  bool CreateGL(XrSession session, int width, int height, bool multiview);
#endif

  int width;
  int height;
  bool acquired;
  XrSwapchain handle;

  uint32_t swapchain_index;
  uint32_t swapchain_length;
  void* swapchain_image;

  unsigned int* gl_depth_buffers;
  unsigned int* gl_frame_buffers;
};
}  // namespace Common::VR
