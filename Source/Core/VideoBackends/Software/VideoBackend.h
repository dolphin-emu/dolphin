// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "VideoCommon/VideoBackendBase.h"

namespace Core
{
class System;
}

namespace SW
{
class VideoSoftware : public VideoBackendBase
{
  bool Initialize(Core::System& system, const WindowSystemInfo& wsi) override;
  void Shutdown(Core::System& system) override;

  std::string GetConfigName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  void InitBackendInfo(const WindowSystemInfo& wsi) override;

  static constexpr const char* CONFIG_NAME = "Software Renderer";
};
}  // namespace SW
