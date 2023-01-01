// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <windows.h>
#include <string>
#include "Common/GL/GLContext.h"

class GLContextWGL final : public GLContext
{
public:
  ~GLContextWGL();

  bool IsHeadless() const;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void Update() override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  static bool CheckForGLES(HDC dc);
  static HGLRC CreateCoreContext(HDC dc, HGLRC share_context, Mode* mode);
  static bool CreatePBuffer(HDC onscreen_dc, int width, int height, HANDLE* pbuffer_handle,
                            HDC* pbuffer_dc);

  HWND m_window_handle = nullptr;
  HANDLE m_pbuffer_handle = nullptr;
  HDC m_dc = nullptr;
  HGLRC m_rc = nullptr;
};
