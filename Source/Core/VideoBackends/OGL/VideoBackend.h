// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

class GLContext;

namespace OGL
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(const WindowSystemInfo& wsi) override;
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;

  void InitBackendInfo() override;

  static constexpr const char* NAME = "OGL";

private:
  bool InitializeGLExtensions(GLContext* context);
  bool FillBackendInfo();
};
}  // namespace OGL
