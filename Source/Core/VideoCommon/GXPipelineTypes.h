// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

class NativeVertexFormat;

namespace VideoCommon
{
// This version number must be incremented whenever any of the shader UID structures change.
// As pipelines encompass both shader UIDs and render states, changes to either of these should
// also increment the pipeline UID version. Incrementing the UID version will cause all UID
// caches to be invalidated.
constexpr u32 GX_PIPELINE_UID_VERSION = 6;  // Last changed in PR 10890

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
  GXPipelineUid() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }
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

  GXUberPipelineUid() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }
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

// Disk cache of pipeline UIDs. We can't use the whole UID as a type as it contains pointers.
// This structure is safe to save to disk, and should be compiler/platform independent.
#pragma pack(push, 1)
struct SerializedGXPipelineUid
{
  PortableVertexDeclaration vertex_decl{};
  VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  PixelShaderUid ps_uid;
  u32 rasterization_state_bits = 0;
  u32 depth_state_bits = 0;
  u32 blending_state_bits = 0;
};
struct SerializedGXUberPipelineUid
{
  PortableVertexDeclaration vertex_decl{};
  UberShader::VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  UberShader::PixelShaderUid ps_uid;
  u32 rasterization_state_bits = 0;
  u32 depth_state_bits = 0;
  u32 blending_state_bits = 0;
};
#pragma pack(pop)

}  // namespace VideoCommon
