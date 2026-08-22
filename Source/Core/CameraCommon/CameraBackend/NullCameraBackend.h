// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "CameraCommon/CameraBackend/CameraBackend.h"
#include "CameraCommon/CameraInterface/CameraInterface.h"

class NullCameraBackend : public CameraBackend
{
public:
  std::vector<std::unique_ptr<CameraInterface>> Enumerate() override;
};
