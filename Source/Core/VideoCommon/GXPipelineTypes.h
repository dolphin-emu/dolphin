// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

class NativeVertexFormat;

namespace VideoCommon
{
struct GXPipelineUid
{
  const NativeVertexFormat* vertex_format;
  VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  PixelShaderUid ps_uid;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;

  // We use memcmp() for comparing pipelines as std::tie generates a large number of instructions,
  // and this map lookup can happen every draw call. However, as using memcmp() will also compare
  // any padding bytes, we have to ensure these are zeroed out.
  GXPipelineUid() { std::memset(this, 0, sizeof(*this)); }
  GXPipelineUid(const GXPipelineUid& rhs) { std::memcpy(this, &rhs, sizeof(*this)); }
  GXPipelineUid& operator=(const GXPipelineUid& rhs)
  {
    std::memcpy(this, &rhs, sizeof(*this));
    return *this;
  }
  bool operator<(const GXPipelineUid& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) < 0;
  }
  bool operator==(const GXPipelineUid& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) == 0;
  }
  bool operator!=(const GXPipelineUid& rhs) const { return !operator==(rhs); }
};
struct GXUberPipelineUid
{
  const NativeVertexFormat* vertex_format;
  UberShader::VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  UberShader::PixelShaderUid ps_uid;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;

  GXUberPipelineUid() { std::memset(this, 0, sizeof(*this)); }
  GXUberPipelineUid(const GXUberPipelineUid& rhs) { std::memcpy(this, &rhs, sizeof(*this)); }
  GXUberPipelineUid& operator=(const GXUberPipelineUid& rhs)
  {
    std::memcpy(this, &rhs, sizeof(*this));
    return *this;
  }
  bool operator<(const GXUberPipelineUid& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) < 0;
  }
  bool operator==(const GXUberPipelineUid& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) == 0;
  }
  bool operator!=(const GXUberPipelineUid& rhs) const { return !operator==(rhs); }
};
}  // namespace VideoCommon
