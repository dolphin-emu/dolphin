// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexShaderGen.h"

class NativeVertexFormat;

namespace VideoCommon
{
// This version number must be incremented whenever any of the shader UID structures change.
// As pipelines encompass both shader UIDs and render states, changes to either of these should
// also increment the pipeline UID version. Incrementing the UID version will cause all UID
// caches to be invalidated.
constexpr u32 GX_PIPELINE_UID_VERSION = 4;  // Last changed in PR 6431

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
  GXPipelineUid(const GXPipelineUid& rhs)
  {
    std::memcpy(static_cast<void*>(this), &rhs, sizeof(*this));
  }
  GXPipelineUid& operator=(const GXPipelineUid& rhs)
  {
    std::memcpy(static_cast<void*>(this), &rhs, sizeof(*this));
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

// Disk cache of pipeline UIDs. We can't use the whole UID as a type as it contains pointers.
// This structure is safe to save to disk, and should be compiler/platform independent.
#pragma pack(push, 1)
struct SerializedGXPipelineUid
{
  PortableVertexDeclaration vertex_decl;
  VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  PixelShaderUid ps_uid;
  u32 rasterization_state_bits;
  u32 depth_state_bits;
  u32 blending_state_bits;
};
#pragma pack(pop)

}  // namespace VideoCommon
