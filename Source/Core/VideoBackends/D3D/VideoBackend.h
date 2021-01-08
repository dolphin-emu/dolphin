// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace DX11
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(const WindowSystemInfo& wsi) override;
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  bool InitBackendInfo() override;

  static constexpr const char* NAME = "D3D";

private:
  void FillBackendInfo();
};
}  // namespace DX11
