// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace Metal
{
class VideoBackend : public VideoBackendBase
{
public:
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  void InitBackendInfo() override;

  void PrepareWindow(WindowSystemInfo& wsi) override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager() override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery() override;
  std::unique_ptr<BoundingBox> CreateBoundingBox() override;

  static constexpr const char* NAME = "Metal";
};
}  // namespace Metal
