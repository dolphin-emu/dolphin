// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(void* window_handle) override;
  void Shutdown() override;

  std::string GetName() const override { return "Vulkan"; }
  std::string GetDisplayName() const override { return "Vulkan (experimental)"; }
  void Video_Prepare() override;
  void Video_Cleanup() override;

  void InitBackendInfo() override;

  unsigned int PeekMessages() override { return 0; }
};
}
