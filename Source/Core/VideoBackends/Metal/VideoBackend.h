// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace Metal
{
class VideoBackend : public VideoBackendBase
{
public:
  bool InitializeBackend(const WindowSystemInfo& wsi) override;
  void ShutdownBackend() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  void InitBackendInfo() override;

  void PrepareWindow(WindowSystemInfo& wsi) override;
  void UnPrepareWindow(WindowSystemInfo& wsi) override;

  static constexpr const char* NAME = "Metal";
};
}  // namespace Metal
