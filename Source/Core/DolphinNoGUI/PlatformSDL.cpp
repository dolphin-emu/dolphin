// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include <cstdio>
#include <thread>

#include <SDL.h>

// webOS (webosbrew SDL) uses the Wayland video driver. Force the SysWM Wayland
// layout even when the SDK's SDL_config.h was built without that define.
#ifndef SDL_VIDEO_DRIVER_WAYLAND
#define SDL_VIDEO_DRIVER_WAYLAND 1
#endif
#include <SDL_syswm.h>

#include <wayland-egl.h>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "Core/System.h"

namespace
{
class PlatformSDL final : public Platform
{
public:
  ~PlatformSDL() override;

  bool Init() override;
  void SetTitle(const std::string& title) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  void ProcessEvents();
  bool InitWaylandEGLWindow();

  SDL_Window* m_window = nullptr;
  wl_egl_window* m_wl_egl_window = nullptr;
  bool m_owns_wl_egl_window = false;
  void* m_wl_display = nullptr;
  void* m_wl_surface = nullptr;
};

PlatformSDL::~PlatformSDL()
{
  if (m_owns_wl_egl_window && m_wl_egl_window)
  {
    wl_egl_window_destroy(m_wl_egl_window);
    m_wl_egl_window = nullptr;
  }
  if (m_window)
  {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool PlatformSDL::InitWaylandEGLWindow()
{
  SDL_SysWMinfo info{};
  SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(m_window, &info))
  {
    std::fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
    return false;
  }

  if (info.subsystem != SDL_SYSWM_WAYLAND)
  {
    std::fprintf(stderr, "PlatformSDL: expected Wayland SysWM (got %d); webOS needs webosbrew SDL2\n",
                 static_cast<int>(info.subsystem));
    return false;
  }

  m_wl_display = info.info.wl.display;
  m_wl_surface = info.info.wl.surface;
  if (!m_wl_display || !m_wl_surface)
  {
    std::fprintf(stderr, "PlatformSDL: Wayland display/surface missing from SysWM\n");
    return false;
  }

  // Prefer an existing wl_egl_window from SDL; otherwise create our own so Dolphin's
  // EGL path owns the surface (no SDL_GL_CreateContext).
  if (info.info.wl.egl_window)
  {
    m_wl_egl_window = info.info.wl.egl_window;
    m_owns_wl_egl_window = false;
  }
  else
  {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_window, &width, &height);
    if (width <= 0 || height <= 0)
    {
      width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
      height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
    }
    m_wl_egl_window = wl_egl_window_create(static_cast<wl_surface*>(m_wl_surface), width, height);
    if (!m_wl_egl_window)
    {
      std::fprintf(stderr, "PlatformSDL: wl_egl_window_create failed\n");
      return false;
    }
    m_owns_wl_egl_window = true;
  }

  return true;
}

bool PlatformSDL::Init()
{
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
  {
    std::fprintf(stderr, "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
    return false;
  }

  // Window only — GL/GLES context comes from Dolphin's EGL interface.
  const int width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  const int height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
  Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
  // webOS: prefer SDL_WINDOW_FULLSCREEN over FULLSCREEN_DESKTOP (older firmwares).
  if (Config::Get(Config::MAIN_FULLSCREEN))
    flags |= SDL_WINDOW_FULLSCREEN;

  m_window =
      SDL_CreateWindow("Dolphin", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
                       flags);
  if (!m_window)
  {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  if (!InitWaylandEGLWindow())
    return false;

  m_window_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
  return true;
}

void PlatformSDL::SetTitle(const std::string& title)
{
  if (m_window)
    SDL_SetWindowTitle(m_window, title.c_str());
}

void PlatformSDL::ProcessEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
    case SDL_QUIT:
      RequestShutdown();
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
        m_window_focus = true;
      else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        m_window_focus = false;
      else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && m_wl_egl_window &&
               m_owns_wl_egl_window)
      {
        wl_egl_window_resize(m_wl_egl_window, event.window.data1, event.window.data2, 0, 0);
      }
      break;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE)
      {
        RequestShutdown();
      }
      else if (event.key.keysym.sym == SDLK_F10)
      {
        if (Core::GetState(Core::System::GetInstance()) == Core::State::Running)
          Core::SetState(Core::System::GetInstance(), Core::State::Paused);
        else
          Core::SetState(Core::System::GetInstance(), Core::State::Running);
      }
      else if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F8)
      {
        const int slot_number = event.key.keysym.sym - SDLK_F1 + 1;
        if (event.key.keysym.mod & KMOD_SHIFT)
          State::Save(Core::System::GetInstance(), slot_number);
        else
          State::Load(Core::System::GetInstance(), slot_number);
      }
      else if (event.key.keysym.sym == SDLK_F9)
      {
        Core::SaveScreenShot();
      }
      else if (event.key.keysym.sym == SDLK_F11)
      {
        State::LoadLastSaved(Core::System::GetInstance());
      }
      else if (event.key.keysym.sym == SDLK_F12)
      {
        if (event.key.keysym.mod & KMOD_SHIFT)
          State::UndoLoadState(Core::System::GetInstance());
        else
          State::UndoSaveState(Core::System::GetInstance());
      }
      break;
    default:
      break;
    }
  }
}

void PlatformSDL::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());
    ProcessEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformSDL::GetWindowSystemInfo() const
{
  // Present as Wayland so GLContext picks Dolphin's EGL backend (not SDL_GL).
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Wayland;
  wsi.display_connection = m_wl_display;
  wsi.render_window = m_wl_surface;
  wsi.render_surface = m_wl_egl_window;
  return wsi;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateSDLPlatform()
{
  return std::make_unique<PlatformSDL>();
}
