// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace SW
{
class VideoSoftware : public VideoBackendBase
{
  bool InitializeBackend(const WindowSystemInfo& wsi) override;
  void ShutdownBackend() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  void InitBackendInfo() override;

  static constexpr const char* NAME = "Software Renderer";
};
}  // namespace SW
