// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraCommon/CameraBackend/SDLCameraBackend.h"

#include <SDL3/SDL.h>

#include "CameraCommon/CameraInterface/SDLCameraInterface.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"

SDLCameraBackend::SDLCameraBackend()
{
  if (SDL_WasInit(SDL_INIT_CAMERA))
    return;
  if (!SDL_InitSubSystem(SDL_INIT_CAMERA))
  {
    ERROR_LOG_FMT(CAMERA, "Failed to initialize SDL Camera: {}", SDL_GetError());
    return;
  }
}

SDLCameraBackend::~SDLCameraBackend()
{
  if (SDL_WasInit(SDL_INIT_CAMERA))
    SDL_QuitSubSystem(SDL_INIT_CAMERA);
}

std::vector<std::unique_ptr<CameraInterface>> SDLCameraBackend::Enumerate()
{
  std::vector<std::unique_ptr<CameraInterface>> cameras;

  int count = 0;
  SDL_CameraID* ids = SDL_GetCameras(&count);

  if (!ids)
  {
    ERROR_LOG_FMT(CAMERA, "SDL: Failed to enumerate cameras: {}", SDL_GetError());
    return cameras;
  }

  Common::ScopeGuard id_guard([ids] { SDL_free(ids); });

  if (count == 0)
    return cameras;

  for (int i = 0; i < count; ++i)
  {
    const char* sdl_name = SDL_GetCameraName(ids[i]);
    const std::string name = sdl_name ? sdl_name : Common::GetStringT("Unknown Camera");
    cameras.push_back(std::make_unique<SDLCameraInterface>(name, ids[i]));
  }

  return cameras;
}
