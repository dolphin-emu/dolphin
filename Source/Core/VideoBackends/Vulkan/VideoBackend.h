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
  bool Initialize(void* display_handle, void* window_handle) override;
  void Shutdown() override;

  std::string GetName() const override { return "Vulkan"; }
  std::string GetDisplayName() const override { return _trans("Vulkan"); }
  void InitBackendInfo() override;
};
}  // namespace Vulkan
