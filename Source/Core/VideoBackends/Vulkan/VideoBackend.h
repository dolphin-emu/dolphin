// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan
{
class VideoBackend : public VideoBackendBase
{
public:
  bool InitializeBackend(const WindowSystemInfo& wsi) override;
  void ShutdownBackend() override;

  std::string GetName() const override { return NAME; }
  std::string GetDisplayName() const override { return _trans("Vulkan"); }
  void InitBackendInfo() override;
  void PrepareWindow(WindowSystemInfo& wsi) override;
  void UnPrepareWindow(WindowSystemInfo& wsi) override;

  static constexpr const char* NAME = "Vulkan";
};
}  // namespace Vulkan
