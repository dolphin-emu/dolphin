// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "CameraCommon/CameraInterface/CameraInterface.h"

class CameraBackend
{
public:
  CameraBackend() = default;
  virtual ~CameraBackend() = default;

  CameraBackend(const CameraBackend&) = delete;
  CameraBackend& operator=(const CameraBackend&) = delete;
  CameraBackend(CameraBackend&&) = delete;
  CameraBackend& operator=(CameraBackend&&) = delete;

  virtual std::vector<std::unique_ptr<CameraInterface>> Enumerate() = 0;
};
