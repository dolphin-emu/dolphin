// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace OGL
{
class VideoBackend : public VideoBackendBase
{
  bool Initialize(void*) override;
  bool InitializeOtherThread(void*, std::thread*) override;
  void Shutdown() override;
  void ShutdownOtherThread() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;

  void Video_Prepare() override;
  void Video_PrepareOtherThread() override;
  void Video_Cleanup() override;
  void Video_CleanupOtherThread() override;
  bool Video_CanDoAsync() override;

  void InitBackendInfo() override;

  unsigned int PeekMessages() override;

private:
  bool InitializeGLExtensions();
  bool FillBackendInfo();
};
}
