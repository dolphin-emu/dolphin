// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Null
{
class VideoBackend final : public VideoBackendBase
{
public:
  std::string GetName() const override { return NAME; }
  std::string GetDisplayName() const override;
  void InitBackendInfo() override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager(AbstractGfx* gfx) override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery(AbstractGfx* gfx) override;
  std::unique_ptr<BoundingBox> CreateBoundingBox(AbstractGfx* gfx) override;
  std::unique_ptr<Renderer> CreateRenderer(AbstractGfx* gfx) override;
  std::unique_ptr<TextureCacheBase> CreateTextureCache(AbstractGfx* gfx) override;

  static constexpr const char* NAME = "Null";
};
}  // namespace Null
