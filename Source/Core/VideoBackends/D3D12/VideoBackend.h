// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace DX12
{
class VideoBackend final : public VideoBackendBase
{
public:
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;
  void InitBackendInfo() override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager(AbstractGfx* gfx) override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery(AbstractGfx* gfx) override;
  std::unique_ptr<BoundingBox> CreateBoundingBox(AbstractGfx* gfx) override;

  static constexpr const char* NAME = "D3D12";

private:
  void FillBackendInfo();
};
}  // namespace DX12
