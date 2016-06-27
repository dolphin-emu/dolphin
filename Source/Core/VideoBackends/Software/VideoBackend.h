// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace MMIO
{
class Mapping;
}

namespace SW
{
class VideoSoftware : public VideoBackendBase
{
  bool Initialize(void* window_handle) override;
  bool InitializeOtherThread(void* window_handle, std::thread* video_thread) override;
  void Shutdown() override;
  void ShutdownOtherThread() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;

  void Video_Prepare() override;
  void Video_PrepareOtherThread() override;
  void Video_Cleanup() override;
  void Video_CleanupOtherThread() override;

  void ShowConfig(void* parent) override;

  unsigned int PeekMessages() override;
};
}
