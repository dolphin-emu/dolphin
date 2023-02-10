// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan
{
class VideoBackend : public VideoBackendBase
{
public:
  void Shutdown() override;

  std::string GetName() const override { return NAME; }
  std::string GetDisplayName() const override { return _trans("Vulkan"); }
  void InitBackendInfo() override;
  void PrepareWindow(WindowSystemInfo& wsi) override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager() override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery() override;
  std::unique_ptr<BoundingBox> CreateBoundingBox() override;

  static constexpr const char* NAME = "Vulkan";
};
}  // namespace Vulkan
