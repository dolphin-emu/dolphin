// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/GLContextSDL.h"

#include <SDL.h>

#include "Common/Logging/Log.h"
#include "Common/WindowSystemInfo.h"

GLContextSDL::~GLContextSDL()
{
  if (m_gl_context)
  {
    if (SDL_GL_GetCurrentContext() == m_gl_context)
      SDL_GL_MakeCurrent(m_window, nullptr);
    SDL_GL_DeleteContext(m_gl_context);
    m_gl_context = nullptr;
  }

  if (m_owns_window && m_window)
  {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
}

bool GLContextSDL::IsHeadless() const
{
  return m_window == nullptr;
}

bool GLContextSDL::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  m_window = static_cast<SDL_Window*>(wsi.render_window);
  if (!m_window)
  {
    ERROR_LOG_FMT(VIDEO, "GLContextSDL: missing SDL_Window in WindowSystemInfo");
    return false;
  }

  return InitializeContext(m_window, nullptr, stereo, core);
}

bool GLContextSDL::InitializeContext(SDL_Window* window, SDL_GLContext share_context, bool stereo,
                                     bool core)
{
  m_window = window;
  m_opengl_mode = Mode::OpenGLES;

  if (share_context)
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

  // Prefer GLES 3.x (webOS), fall back to GLES 2 if needed.
  const std::pair<int, int> versions[] = {{3, 2}, {3, 1}, {3, 0}, {2, 0}};
  for (const auto& [major, minor] : versions)
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    if (stereo)
      SDL_GL_SetAttribute(SDL_GL_STEREO, 1);

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (m_gl_context)
    {
      INFO_LOG_FMT(VIDEO, "GLContextSDL: created GLES {}.{} context", major, minor);
      break;
    }
  }

  if (share_context)
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

  if (!m_gl_context)
  {
    ERROR_LOG_FMT(VIDEO, "GLContextSDL: SDL_GL_CreateContext failed: {}", SDL_GetError());
    return false;
  }

  if (!MakeCurrent())
    return false;

  int width = 0;
  int height = 0;
  SDL_GL_GetDrawableSize(m_window, &width, &height);
  m_backbuffer_width = static_cast<u32>(width);
  m_backbuffer_height = static_cast<u32>(height);
  return true;
}

std::unique_ptr<GLContext> GLContextSDL::CreateSharedContext()
{
  auto context = std::make_unique<GLContextSDL>();
  if (!context->InitializeContext(m_window, m_gl_context, false, true))
    return nullptr;
  context->m_is_shared = true;
  return context;
}

bool GLContextSDL::MakeCurrent()
{
  return SDL_GL_MakeCurrent(m_window, m_gl_context) == 0;
}

bool GLContextSDL::ClearCurrent()
{
  return SDL_GL_MakeCurrent(m_window, nullptr) == 0;
}

void GLContextSDL::Update()
{
  if (!m_window)
    return;
  int width = 0;
  int height = 0;
  SDL_GL_GetDrawableSize(m_window, &width, &height);
  m_backbuffer_width = static_cast<u32>(width);
  m_backbuffer_height = static_cast<u32>(height);
}

void GLContextSDL::Swap()
{
  SDL_GL_SwapWindow(m_window);
}

void GLContextSDL::SwapInterval(int interval)
{
  SDL_GL_SetSwapInterval(interval);
}

void* GLContextSDL::GetFuncAddress(const std::string& name)
{
  return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name.c_str()));
}
