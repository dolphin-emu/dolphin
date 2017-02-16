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
  bool InitializeOtherThread(void *, std::thread *) override;
  void Shutdown() override;
  void ShutdownOtherThread() override;

  std::string GetName() const override { return "Vulkan"; }
  std::string GetDisplayName() const override { return "Vulkan (experimental, no VR)"; }
  void Video_Prepare() override;
  void Video_PrepareOtherThread() override;
  void Video_Cleanup() override;
  void Video_CleanupOtherThread() override;

  void InitBackendInfo() override;

  unsigned int PeekMessages() override { return 0; }
};
}
