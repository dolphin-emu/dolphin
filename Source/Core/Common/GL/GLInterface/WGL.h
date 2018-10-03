// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  bool Initialize(void* display_handle, void* window_handle, bool stereo, bool core) override;

  static HGLRC CreateCoreContext(HDC dc, HGLRC share_context);
  static bool CreatePBuffer(HDC onscreen_dc, int width, int height, HANDLE* pbuffer_handle,
                            HDC* pbuffer_dc);

  HWND m_window_handle = nullptr;
  HANDLE m_pbuffer_handle = nullptr;
  HDC m_dc = nullptr;
  HGLRC m_rc = nullptr;
};
