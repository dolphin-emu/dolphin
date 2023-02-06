// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Null
{
class VideoBackend final : public VideoBackendBase
{
public:
  bool InitializeBackend(const WindowSystemInfo& wsi) override;
  void ShutdownBackend() override;

  std::string GetName() const override { return NAME; }
  std::string GetDisplayName() const override;
  void InitBackendInfo() override;

  static constexpr const char* NAME = "Null";
};
}  // namespace Null
