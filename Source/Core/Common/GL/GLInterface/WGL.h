// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>
#include <string>
#include "Common/GL/GLInterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
  void SwapInterval(int interval) override;
  void Swap() override;
  void* GetFuncAddress(const std::string& name) override;
  bool Create(void* window_handle, bool stereo, bool core) override;
  bool Create(cInterfaceBase* main_context) override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  void Shutdown() override;

  void Update() override;
  bool PeekMessages() override;

  std::unique_ptr<cInterfaceBase> CreateSharedContext() override;

private:
  static HGLRC CreateCoreContext(HDC dc, HGLRC share_context);
  static bool CreatePBuffer(HDC onscreen_dc, int width, int height, HANDLE* pbuffer_handle,
                            HDC* pbuffer_dc);

  HWND m_window_handle = nullptr;
  HANDLE m_pbuffer_handle = nullptr;
  HDC m_dc = nullptr;
  HGLRC m_rc = nullptr;
};
