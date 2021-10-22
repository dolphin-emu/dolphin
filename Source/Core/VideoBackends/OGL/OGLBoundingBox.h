// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"

#include "VideoCommon/BoundingBox.h"

namespace OGL
{
class OGLBoundingBox final : public BoundingBox
{
public:
  ~OGLBoundingBox() override;

  bool Initialize() override;

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override;
  void Write(u32 index, const std::vector<BBoxType>& values) override;

private:
  GLuint m_buffer_id = 0;
};

}  // namespace OGL
