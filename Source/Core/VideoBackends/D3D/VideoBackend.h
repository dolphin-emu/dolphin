// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace DX11
{
class VideoBackend : public VideoBackendBase
{
public:
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  std::optional<std::string> GetWarningMessage() const override;

  void InitBackendInfo() override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager(AbstractGfx* gfx) override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery(AbstractGfx* gfx) override;
  std::unique_ptr<BoundingBox> CreateBoundingBox(AbstractGfx* gfx) override;

  static constexpr const char* NAME = "D3D";

private:
  void FillBackendInfo();
};
}  // namespace DX11
