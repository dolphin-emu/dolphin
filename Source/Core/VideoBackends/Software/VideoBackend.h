// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace SW
{
class VideoSoftware : public VideoBackendBase
{
public:
  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager(AbstractGfx* gfx) override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery(AbstractGfx* gfx) override;
  std::unique_ptr<BoundingBox> CreateBoundingBox(AbstractGfx* gfx) override;
  std::unique_ptr<Renderer> CreateRenderer(AbstractGfx* gfx) override;
  std::unique_ptr<TextureCacheBase> CreateTextureCache(AbstractGfx* gfx) override;

  void InitBackendInfo() override;

  static constexpr const char* NAME = "Software Renderer";
};
}  // namespace SW
