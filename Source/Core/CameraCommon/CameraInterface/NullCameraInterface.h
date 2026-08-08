// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "CameraCommon/CameraInterface/CameraInterface.h"

class NullCameraInterface final : public CameraInterface
{
public:
  NullCameraInterface(std::string name, const u32 id) : CameraInterface(std::move(name), id) {}

  std::string_view Backend() const override { return "NULL"; }
  Common::StbImage CaptureFrame() override;

protected:
  const int m_width = 320;
  const int m_height = 240;

  std::expected<void, CameraError> Open() override;
  void Close() override;

private:
  constexpr static int m_channels = 3;
};
