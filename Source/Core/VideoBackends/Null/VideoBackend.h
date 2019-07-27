// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Null
{
class VideoBackend final : public VideoBackendBase
{
  bool Initialize(const WindowSystemInfo& wsi) override;
  void Shutdown() override;

  std::string GetName() const override { return "Null"; }
  std::string GetDisplayName() const override;
  void InitBackendInfo() override;
};
}  // namespace Null
