// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/VRBase.h"

namespace Common::VR
{
  void AppClear(App* app);
  void AppDestroy(App* app);
  bool HandleXrEvents(App* app);

  void FramebufferAcquire(Framebuffer* framebuffer);
  void FramebufferRelease(Framebuffer* framebuffer);
  void FramebufferSetCurrent(Framebuffer* framebuffer);

  void RendererCreate(XrSession session, Renderer* renderer, int width, int height, bool multiview);
  void RendererDestroy(Renderer* renderer);
  void RendererMouseCursor(int x, int y, int sx, int sy);
#ifdef ANDROID
  void RendererSetFoveation(XrInstance* instance, XrSession* session, Renderer* renderer,
                            XrFoveationLevelFB level, float offset, XrFoveationDynamicFB dynamic);
#endif
}
