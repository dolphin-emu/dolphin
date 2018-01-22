// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

enum class GLInterfaceMode
{
  MODE_DETECT,
  MODE_OPENGL,
  MODE_OPENGLES2,
  MODE_OPENGLES3,
};

class cInterfaceBase
{
protected:
  // Window dimensions.
  u32 s_backbuffer_width = 0;
  u32 s_backbuffer_height = 0;
  bool m_core = false;
  bool m_is_shared = false;

  GLInterfaceMode s_opengl_mode = GLInterfaceMode::MODE_DETECT;

public:
  virtual ~cInterfaceBase() {}
  virtual void Swap() {}
  virtual void SetMode(GLInterfaceMode mode) { s_opengl_mode = GLInterfaceMode::MODE_OPENGL; }
  virtual GLInterfaceMode GetMode() { return s_opengl_mode; }
  virtual void* GetFuncAddress(const std::string& name) { return nullptr; }
  virtual bool Create(void* window_handle, bool stereo = false, bool core = true) { return true; }
  virtual bool Create(cInterfaceBase* main_context) { return true; }
  virtual bool CreateOffscreen() { return true; }
  virtual bool MakeCurrent() { return true; }
  virtual bool MakeCurrentOffscreen() { return true; }
  virtual bool ClearCurrent() { return true; }
  virtual bool ClearCurrentOffscreen() { return true; }
  virtual void Shutdown() {}
  virtual void ShutdownOffscreen() {}
  virtual void SwapInterval(int Interval) {}
  virtual u32 GetBackBufferWidth() { return s_backbuffer_width; }
  virtual u32 GetBackBufferHeight() { return s_backbuffer_height; }
  virtual void SetBackBufferDimensions(u32 W, u32 H)
  {
    s_backbuffer_width = W;
    s_backbuffer_height = H;
  }
  virtual void Update() {}
  virtual bool PeekMessages() { return false; }
  virtual void UpdateHandle(void* window_handle) {}
  virtual void UpdateSurface() {}
  virtual std::unique_ptr<cInterfaceBase> CreateSharedContext() { return nullptr; }
};

extern std::unique_ptr<cInterfaceBase> GLInterface;

// This function has to be defined along the Host_ functions from Core/Host.h.
// Current canonical implementation: DolphinWX/GLInterface/GLInterface.cpp.
std::unique_ptr<cInterfaceBase> HostGL_CreateGLInterface();
