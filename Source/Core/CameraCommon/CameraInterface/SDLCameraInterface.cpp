// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraCommon/CameraInterface/SDLCameraInterface.h"

#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

SDLCameraInterface::~SDLCameraInterface()
{
  if (m_is_open)
    Close();
}

std::expected<void, CameraError> SDLCameraInterface::Open()
{
  if (m_is_open)
    return {};

  SDL_CameraSpec spec{};
  spec.format = SDL_PIXELFORMAT_RGB24;

  m_camera = SDL_OpenCamera(static_cast<SDL_CameraID>(m_id), &spec);
  if (!m_camera)
  {
    ERROR_LOG_FMT(CAMERA, "SDL: Failed to open camera {}/{}: {}", m_id, m_name, SDL_GetError());
    return std::unexpected(CameraError::DeviceNotFound);
  }

  if (!SDL_GetCameraFormat(m_camera, &spec))
  {
    ERROR_LOG_FMT(CAMERA, "SDL: Failed to get camera format for {}/{}: {}", m_id, m_name,
                  SDL_GetError());
    SDL_CloseCamera(m_camera);
    m_camera = nullptr;
    return std::unexpected(CameraError::PermissionDenied);
  }

  m_width = spec.width;
  m_height = spec.height;
  m_channels = SDL_BYTESPERPIXEL(spec.format);
  m_current_frame = Common::StbImage{std::vector<u8>(m_width * m_height * m_channels, 0), m_width,
                                     m_height, m_channels};
  m_is_open = true;
  DEBUG_LOG_FMT(CAMERA, "SDL: {}/{}: Format {}, Resolution {}x{}", m_id, m_name,
                SDL_GetPixelFormatName(spec.format), m_width, m_height);
  return {};
}

void SDLCameraInterface::Close()
{
  if (!SDL_WasInit(SDL_INIT_CAMERA))
    return;

  if (!m_is_open)
    return;

  if (m_camera)
  {
    SDL_CloseCamera(m_camera);
    m_camera = nullptr;
  }

  m_is_open = false;
}

Common::StbImage SDLCameraInterface::CaptureFrame()
{
  ASSERT(m_is_open);
  ASSERT(m_camera);

  const int frame_size = m_width * m_height * m_channels;

  u64 timestamp_ns = 0;
  SDL_Surface* frame = SDL_AcquireCameraFrame(m_camera, &timestamp_ns);

  if (!frame)
    return m_current_frame;

  std::vector<u8> pixels(frame_size);
  std::memcpy(pixels.data(), frame->pixels, frame_size);

  SDL_ReleaseCameraFrame(m_camera, frame);

  m_current_frame = Common::StbImage(std::move(pixels), m_width, m_height, m_channels);

  return m_current_frame;
}
