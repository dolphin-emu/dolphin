// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include "Common/GL/GLContext.h"

struct SDL_Window;
typedef void* SDL_GLContext;

// OpenGL ES context via SDL2 (uses EGL on webOS / Wayland under the hood).
class GLContextSDL final : public GLContext
{
public:
  ~GLContextSDL() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void Update() override;
  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

private:
  bool InitializeContext(SDL_Window* window, SDL_GLContext share_context, bool stereo, bool core);

  SDL_Window* m_window = nullptr;
  SDL_GLContext m_gl_context = nullptr;
  bool m_owns_window = false;
};
