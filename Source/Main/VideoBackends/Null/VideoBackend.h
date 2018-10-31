// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Null
{
class VideoBackend : public VideoBackendBase
{
  bool Initialize(const WindowSystemInfo& wsi) override;
  void Shutdown() override;

  std::string GetName() const override { return "Null"; }
  std::string GetDisplayName() const override
  {
    // i18n: Null is referring to the null video backend, which renders nothing
    return _trans("Null");
  }
  void InitBackendInfo() override;
};
}  // namespace Null
