// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "CameraCommon/CameraBackend/CameraBackend.h"
#include "CameraCommon/CameraInterface/CameraInterface.h"

class SDLCameraBackend : public CameraBackend
{
public:
  SDLCameraBackend();
  ~SDLCameraBackend() override;

  std::vector<std::unique_ptr<CameraInterface>> Enumerate() override;
};
