// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/BGL.h"

#include <GLView.h>
#include <Size.h>
#include <Window.h>

#include "Common/Assert.h"

BGLView* GLContextBGL::s_current = nullptr;

GLContextBGL::~GLContextBGL()
{
  if (!m_window)
    delete m_gl;
}

bool GLContextBGL::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  m_window = static_cast<BWindow*>(wsi.render_window);

  m_gl = new BGLView(m_window ? m_window->Bounds() : BRect(), "GLContextBGL", B_FOLLOW_ALL_SIDES, 0,
                     BGL_RGB | BGL_DOUBLE | BGL_ALPHA);
  if (m_window)
    m_window->AddChild(m_gl);

  m_opengl_mode = Mode::OpenGL;

  m_gl->LockLooper();
  BRect size = m_gl->Frame();
  m_gl->UnlockLooper();

  m_backbuffer_width = size.IntegerWidth();
  m_backbuffer_height = size.IntegerHeight();

  MakeCurrent();

  return true;
}

bool GLContextBGL::IsHeadless() const
{
  return m_window == nullptr;
}

bool GLContextBGL::MakeCurrent()
{
  if (s_current)
    s_current->UnlockGL();
  m_gl->LockGL();
  s_current = m_gl;
  return true;
}

bool GLContextBGL::ClearCurrent()
{
  if (!s_current)
    return true;

  ASSERT(m_gl == s_current);
  s_current->UnlockGL();
  s_current = nullptr;
  return true;
}

void GLContextBGL::Swap()
{
  m_gl->SwapBuffers();
}

void GLContextBGL::Update()
{
  m_gl->LockLooper();
  BRect size = m_gl->Frame();
  if (m_backbuffer_width == size.IntegerWidth() && m_backbuffer_height == size.IntegerHeight())
  {
    m_gl->UnlockLooper();
    return;
  }

  ClearCurrent();
  m_gl->FrameResized(size.Width(), size.Height());
  MakeCurrent();
  m_gl->UnlockLooper();

  m_backbuffer_width = size.IntegerWidth();
  m_backbuffer_height = size.IntegerHeight();
}

void* GLContextBGL::GetFuncAddress(const std::string& name)
{
  return m_gl->GetGLProcAddress(name.c_str());
}
