// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/BGL.h"

#include <GLView.h>
#include <Size.h>
#include <Window.h>

void cInterfaceBGL::Swap()
{
  m_gl->SwapBuffers();
}

bool cInterfaceBGL::Create(void* window_handle, bool stereo, bool core)
{
  m_window = static_cast<BWindow*>(window_handle);

  m_gl = new BGLView(m_window->Bounds(), "cInterfaceBGL", B_FOLLOW_ALL_SIDES, 0,
                     BGL_RGB | BGL_DOUBLE | BGL_ALPHA);
  m_window->AddChild(m_gl);

  s_opengl_mode = GLInterfaceMode::MODE_OPENGL;

  // Control m_window size and picture scaling
  BRect size = m_gl->Frame();
  s_backbuffer_width = size.IntegerWidth();
  s_backbuffer_height = size.IntegerHeight();

  return true;
}

bool cInterfaceBGL::MakeCurrent()
{
  m_gl->LockGL();
  return true;
}

bool cInterfaceBGL::ClearCurrent()
{
  m_gl->UnlockGL();
  return true;
}

void cInterfaceBGL::Shutdown()
{
  // We don't need to delete m_gl, it's owned by the BWindow.
  m_gl = nullptr;
}

void cInterfaceBGL::Update()
{
  BRect size = m_gl->Frame();

  if (s_backbuffer_width == size.IntegerWidth() && s_backbuffer_height == size.IntegerHeight())
    return;

  s_backbuffer_width = size.IntegerWidth();
  s_backbuffer_height = size.IntegerHeight();
}

void cInterfaceBGL::SwapInterval(int interval)
{
}

void* cInterfaceBGL::GetFuncAddress(const std::string& name)
{
  return m_gl->GetGLProcAddress(name.c_str());
}
