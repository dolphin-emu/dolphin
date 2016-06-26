// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Null
{
class VideoBackend : public VideoBackendBase
{
  bool Initialize(void* window_handle) override;
  void Shutdown() override;

  std::string GetName() const override { return "Null"; }
  std::string GetDisplayName() const override { return "Null"; }
  void Video_Prepare() override;
  void Video_Cleanup() override;

  void InitBackendInfo() override;

  unsigned int PeekMessages() override { return 0; }
};
}
