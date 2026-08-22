// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraCommon/CameraBackend/NullCameraBackend.h"

#include "CameraCommon/CameraInterface/NullCameraInterface.h"

std::vector<std::unique_ptr<CameraInterface>> NullCameraBackend::Enumerate()
{
  std::vector<std::unique_ptr<CameraInterface>> cameras;
  cameras.push_back(std::make_unique<NullCameraInterface>("Null", 0));
  return cameras;
}
