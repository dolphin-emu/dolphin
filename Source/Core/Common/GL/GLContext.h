// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"

class GLContext
{
public:
  enum class Mode
  {
    Detect,
    OpenGL,
    OpenGLES
  };

  virtual ~GLContext();

  Mode GetMode() { return m_opengl_mode; }
  bool IsGLES() const { return m_opengl_mode == Mode::OpenGLES; }

  u32 GetBackBufferWidth() { return m_backbuffer_width; }
  u32 GetBackBufferHeight() { return m_backbuffer_height; }

  virtual bool IsHeadless() const;

  virtual std::unique_ptr<GLContext> CreateSharedContext();
  virtual void Shutdown();

  virtual bool MakeCurrent();
  virtual bool ClearCurrent();

  virtual void Update();
  virtual void UpdateSurface(void* window_handle);

  virtual void Swap();
  virtual void SwapInterval(int interval);

  virtual void* GetFuncAddress(const std::string& name);

  // Creates an instance of GLInterface specific to the platform we are running on.
  static std::unique_ptr<GLContext> Create(const WindowSystemInfo& wsi, bool stereo = false,
                                           bool core = true);

protected:
  virtual bool Initialize(void* display_handle, void* window_handle, bool stereo, bool core);
  virtual bool Initialize(GLContext* main_context);

  Mode m_opengl_mode = Mode::Detect;

  // Window dimensions.
  u32 m_backbuffer_width = 0;
  u32 m_backbuffer_height = 0;
  bool m_is_core_context = false;
  bool m_is_shared = false;
};

extern std::unique_ptr<GLContext> g_main_gl_context;
