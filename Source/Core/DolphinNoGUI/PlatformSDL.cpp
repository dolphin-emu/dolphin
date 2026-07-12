// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include <cstdio>
#include <thread>

#include <SDL.h>

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

  SDL_Window* m_window = nullptr;
};

PlatformSDL::~PlatformSDL()
{
  if (m_window)
  {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool PlatformSDL::Init()
{
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
  {
    std::fprintf(stderr, "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
    return false;
  }

  // Request an OpenGL ES capable window; the actual context is created by GLContextSDL.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  const int width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  const int height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
  if (Config::Get(Config::MAIN_FULLSCREEN))
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

  m_window =
      SDL_CreateWindow("Dolphin", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
                       flags);
  if (!m_window)
  {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  m_window_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
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
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::SDL;
  wsi.display_connection = nullptr;
  wsi.render_window = m_window;
  wsi.render_surface = m_window;
  return wsi;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateSDLPlatform()
{
  return std::make_unique<PlatformSDL>();
}
