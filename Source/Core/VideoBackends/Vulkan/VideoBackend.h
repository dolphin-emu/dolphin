// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(const WindowSystemInfo& wsi) override;
  void Shutdown() override;

  std::string GetName() const override { return NAME; }
  std::string GetDisplayName() const override { return _trans("Vulkan"); }
  bool InitBackendInfo() override;
  void PrepareWindow(WindowSystemInfo& wsi) override;

  static constexpr const char* NAME = "Vulkan";
};
}  // namespace Vulkan
