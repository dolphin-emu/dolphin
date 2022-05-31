// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Common/BitFieldView.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BPStructs.h"

enum class AbstractTextureFormat : u32;

enum class PrimitiveType : u32
{
  Points,
  Lines,
  Triangles,
  TriangleStrip,
};

struct RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  RasterizationState() = default;

  bool operator==(const RasterizationState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const RasterizationState& rhs) const { return !operator==(rhs); }
  bool operator<(const RasterizationState& rhs) const { return hex < rhs.hex; }

  u32 hex;

  BFVIEW_M(hex, CullMode, 0, 2, cullmode);
  BFVIEW_M(hex, PrimitiveType, 3, 2, primitive);
};

struct FramebufferState
{
  FramebufferState() = default;

  bool operator==(const FramebufferState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const FramebufferState& rhs) const { return !operator==(rhs); }

  u32 hex;

  BFVIEW_M(hex, AbstractTextureFormat, 0, 8, color_texture_format);
  BFVIEW_M(hex, AbstractTextureFormat, 8, 8, depth_texture_format);
  BFVIEW_M(hex, u32, 16, 8, samples);
  BFVIEW_M(hex, bool, 24, 1, per_sample_shading);
};

struct DepthState
{
  void Generate(const BPMemory& bp);

  DepthState() = default;

  bool operator==(const DepthState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const DepthState& rhs) const { return !operator==(rhs); }
  bool operator<(const DepthState& rhs) const { return hex < rhs.hex; }

  u32 hex;

  BFVIEW_M(hex, bool, 0, 1, testenable);
  BFVIEW_M(hex, bool, 1, 1, updateenable);
  BFVIEW_M(hex, CompareMode, 2, 3, func);
};

struct BlendingState
{
  void Generate(const BPMemory& bp);

  // HACK: Replaces logical operations with blend operations.
  // Will not be bit-correct, and in some cases not even remotely in the same ballpark.
  void ApproximateLogicOpWithBlending();

  BlendingState() = default;

  bool operator==(const BlendingState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const BlendingState& rhs) const { return !operator==(rhs); }
  bool operator<(const BlendingState& rhs) const { return hex < rhs.hex; }

  u32 hex;

  BFVIEW_M(hex, bool, 0, 1, blendenable);
  BFVIEW_M(hex, bool, 1, 1, logicopenable);
  BFVIEW_M(hex, bool, 2, 1, dstalpha);
  BFVIEW_M(hex, bool, 3, 1, colorupdate);
  BFVIEW_M(hex, bool, 4, 1, alphaupdate);
  BFVIEW_M(hex, bool, 5, 1, subtract);
  BFVIEW_M(hex, bool, 6, 1, subtractAlpha);
  BFVIEW_M(hex, bool, 7, 1, usedualsrc);
  BFVIEW_M(hex, DstBlendFactor, 8, 3, dstfactor);
  BFVIEW_M(hex, SrcBlendFactor, 11, 3, srcfactor);
  BFVIEW_M(hex, DstBlendFactor, 14, 3, dstfactoralpha);
  BFVIEW_M(hex, SrcBlendFactor, 17, 3, srcfactoralpha);
  BFVIEW_M(hex, LogicOp, 20, 4, logicmode);
};

struct SamplerState
{
  void Generate(const BPMemory& bp, u32 index);

  SamplerState() = default;

  bool operator==(const SamplerState& rhs) const { return Hex() == rhs.Hex(); }
  bool operator!=(const SamplerState& rhs) const { return !operator==(rhs); }
  bool operator<(const SamplerState& rhs) const { return Hex() < rhs.Hex(); }

  constexpr u64 Hex() const { return tm0.hex | (static_cast<u64>(tm1.hex) << 32); }

  // Based on BPMemory TexMode0/TexMode1, but with slightly higher precision and some
  // simplifications
  union TM0
  {
    u32 hex;

    // BP's mipmap_filter can be None, but that is represented here by setting min_lod and max_lod
    // to 0
    BFVIEW_M(hex, FilterMode, 0, 1, min_filter);
    BFVIEW_M(hex, FilterMode, 1, 1, mag_filter);
    BFVIEW_M(hex, FilterMode, 2, 1, mipmap_filter);
    // Guaranteed to be valid values (i.e. not 3)
    BFVIEW_M(hex, WrapMode, 3, 2, wrap_u);
    BFVIEW_M(hex, WrapMode, 5, 2, wrap_v);
    BFVIEW_M(hex, LODType, 7, 1, diag_lod);
    BFVIEW_M(hex, s32, 8, 16, lod_bias);    // multiplied by 256, higher precision than normal
    BFVIEW_M(hex, bool, 24, 1, lod_clamp);  // TODO: This isn't currently implemented
    BFVIEW_M(hex, bool, 25, 1, anisotropic_filtering);  // TODO: This doesn't use the BP one yet
  };
  union TM1
  {
    u32 hex;

    // Min is guaranteed to be less than or equal to max
    BFVIEW_M(hex, u8, 0, 8, min_lod);  // multiplied by 16
    BFVIEW_M(hex, u8, 8, 8, max_lod);  // multiplied by 16
  };

  TM0 tm0;
  TM1 tm1;
};

namespace std
{
template <>
struct hash<SamplerState>
{
  std::size_t operator()(SamplerState const& state) const noexcept
  {
    return std::hash<u64>{}(state.Hex());
  }
};
}  // namespace std

namespace RenderState
{
RasterizationState GetInvalidRasterizationState();
RasterizationState GetNoCullRasterizationState(PrimitiveType primitive);
RasterizationState GetCullBackFaceRasterizationState(PrimitiveType primitive);
DepthState GetInvalidDepthState();
DepthState GetNoDepthTestingDepthState();
DepthState GetAlwaysWriteDepthState();
BlendingState GetInvalidBlendingState();
BlendingState GetNoBlendingBlendState();
BlendingState GetNoColorWriteBlendState();
SamplerState GetInvalidSamplerState();
SamplerState GetPointSamplerState();
SamplerState GetLinearSamplerState();
FramebufferState GetColorFramebufferState(AbstractTextureFormat format);
FramebufferState GetRGBA8FramebufferState();
}  // namespace RenderState
