// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "VideoCommon/VideoBackendBase.h"

namespace Core
{
class System;
}

namespace Null
{
class VideoBackend final : public VideoBackendBase
{
public:
  bool Initialize(Core::System& system, const WindowSystemInfo& wsi) override;
  void Shutdown(Core::System& system) override;

  std::string GetConfigName() const override { return CONFIG_NAME; }
  std::string GetDisplayName() const override;
  void InitBackendInfo(const WindowSystemInfo& wsi) override;

  static constexpr const char* CONFIG_NAME = "Null";
};
}  // namespace Null
