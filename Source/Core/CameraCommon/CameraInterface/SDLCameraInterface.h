// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SDL3/SDL_camera.h>
#include "CameraCommon/CameraInterface/CameraInterface.h"

class SDLCameraInterface final : public CameraInterface
{
public:
  SDLCameraInterface(std::string name, const u32 id) : CameraInterface(std::move(name), id) {}
  ~SDLCameraInterface() override;

  std::string_view Backend() const override { return "SDL"; }
  Common::StbImage CaptureFrame() override;

protected:
  std::expected<void, CameraError> Open() override;
  void Close() override;

private:
  mutable SDL_Camera* m_camera = nullptr;
  mutable Common::StbImage m_current_frame;
  int m_channels;
};
