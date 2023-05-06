// Copyright 2023 Dolphin Emulator Project
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

void DisplayCreate(XrSession session, Display* display, int width, int height, bool multiview);
void DisplayDestroy(Display* display);
void DisplayMouseCursor(int x, int y, int sx, int sy);
#ifdef ANDROID
void DisplaySetFoveation(XrInstance* instance, XrSession* session, Display* display,
                         XrFoveationLevelFB level, float offset, XrFoveationDynamicFB dynamic);
#endif
}  // namespace Common::VR
