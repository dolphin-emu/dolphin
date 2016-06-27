// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/GL/GLInterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
  void SwapInterval(int interval) override;
  void Swap() override;
  void* GetFuncAddress(const std::string& name) override;
  bool Create(void* window_handle, bool core) override;
  bool CreateOffscreen() override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  bool MakeCurrentOffscreen() override;
  bool ClearCurrentOffscreen() override;
  void Shutdown() override;
  void ShutdownOffscreen() override;

  void Update() override;
  bool PeekMessages() override;

  HWND m_window_handle = nullptr;
  HWND m_offscreen_window_handle = nullptr;
};
