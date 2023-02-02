// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

class GLContext;
class OGLGfx;

namespace OGL
{
class VideoBackend : public VideoBackendBase
{
public:
  void Shutdown() override;

  std::string GetName() const override;
  std::string GetDisplayName() const override;

  void InitBackendInfo() override;

  std::unique_ptr<AbstractGfx> CreateGfx() override;
  std::unique_ptr<VertexManagerBase> CreateVertexManager() override;
  std::unique_ptr<PerfQueryBase> CreatePerfQuery() override;
  std::unique_ptr<BoundingBox> CreateBoundingBox() override;

  static constexpr const char* NAME = "OGL";

private:
  bool InitializeGLExtensions(GLContext* context);
  bool FillBackendInfo();

  bool m_is_gles = false;
};
}  // namespace OGL
