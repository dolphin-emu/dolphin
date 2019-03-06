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

  std::string GetName() const override { return "Vulkan"; }
  std::string GetDisplayName() const override
  {
#ifdef __APPLE__
    return _trans("Vulkan (MoltenVK)");
#else
    return _trans("Vulkan");
#endif
  }
  void InitBackendInfo() override;
  void PrepareWindow(const WindowSystemInfo& wsi) override;
};
}  // namespace Vulkan
