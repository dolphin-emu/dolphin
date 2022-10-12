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

template <>
struct fmt::formatter<PrimitiveType> : EnumFormatter<PrimitiveType::TriangleStrip>
{
  constexpr formatter() : EnumFormatter({"Points", "Lines", "Triangles", "TriangleStrip"}) {}
};

struct RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  bool operator==(const RasterizationState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const RasterizationState& rhs) const { return !operator==(rhs); }
  bool operator<(const RasterizationState& rhs) const { return hex < rhs.hex; }

  BFVIEW(CullMode, 2, 0, cullmode)
  BFVIEW(PrimitiveType, 2, 3, primitive)

  u32 hex;
};

struct FramebufferState
{
  bool operator==(const FramebufferState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const FramebufferState& rhs) const { return !operator==(rhs); }

  BFVIEW(AbstractTextureFormat, 8, 0, color_texture_format)
  BFVIEW(AbstractTextureFormat, 8, 8, depth_texture_format)
  BFVIEW(u32, 8, 16, samples)
  BFVIEW(bool, 1, 24, per_sample_shading)

  u32 hex;
};

struct DepthState
{
  void Generate(const BPMemory& bp);

  bool operator==(const DepthState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const DepthState& rhs) const { return !operator==(rhs); }
  bool operator<(const DepthState& rhs) const { return hex < rhs.hex; }

  BFVIEW(bool, 1, 0, testenable)
  BFVIEW(bool, 1, 1, updateenable)
  BFVIEW(CompareMode, 3, 2, func)

  u32 hex;
};

struct BlendingState
{
  void Generate(const BPMemory& bp);

  // HACK: Replaces logical operations with blend operations.
  // Will not be bit-correct, and in some cases not even remotely in the same ballpark.
  void ApproximateLogicOpWithBlending();

  bool operator==(const BlendingState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const BlendingState& rhs) const { return !operator==(rhs); }
  bool operator<(const BlendingState& rhs) const { return hex < rhs.hex; }

  BFVIEW(bool, 1, 0, blendenable)
  BFVIEW(bool, 1, 1, logicopenable)
  BFVIEW(bool, 1, 3, colorupdate)
  BFVIEW(bool, 1, 4, alphaupdate)
  BFVIEW(bool, 1, 5, subtract)
  BFVIEW(bool, 1, 6, subtractAlpha)
  BFVIEW(bool, 1, 7, usedualsrc)
  BFVIEW(DstBlendFactor, 3, 8, dstfactor)
  BFVIEW(SrcBlendFactor, 3, 11, srcfactor)
  BFVIEW(DstBlendFactor, 3, 14, dstfactoralpha)
  BFVIEW(SrcBlendFactor, 3, 17, srcfactoralpha)
  BFVIEW(LogicOp, 4, 20, logicmode)

  bool RequiresDualSrc() const;

  u32 hex;
};

struct SamplerState
{
  void Generate(const BPMemory& bp, u32 index);

  bool operator==(const SamplerState& rhs) const { return Hex() == rhs.Hex(); }
  bool operator!=(const SamplerState& rhs) const { return !operator==(rhs); }
  bool operator<(const SamplerState& rhs) const { return Hex() < rhs.Hex(); }

  constexpr u64 Hex() const { return tm0.hex | (static_cast<u64>(tm1.hex) << 32); }

  // Based on BPMemory TexMode0/TexMode1, but with slightly higher precision and some
  // simplifications
  struct TM0
  {
    // BP's mipmap_filter can be None, but that is represented here by setting min_lod and max_lod
    // to 0
    BFVIEW(FilterMode, 1, 0, min_filter)
    BFVIEW(FilterMode, 1, 1, mag_filter)
    BFVIEW(FilterMode, 1, 2, mipmap_filter)
    // Guaranteed to be valid values (i.e. not 3)
    BFVIEW(WrapMode, 2, 3, wrap_u)
    BFVIEW(WrapMode, 2, 5, wrap_v)
    BFVIEW(LODType, 1, 7, diag_lod)
    BFVIEW(s32, 16, 8, lod_bias)                // multiplied by 256, higher precision than normal
    BFVIEW(bool, 1, 24, lod_clamp)              // TODO: This isn't currently implemented
    BFVIEW(bool, 1, 25, anisotropic_filtering)  // TODO: This doesn't use the BP one yet
    u32 hex;
  };
  struct TM1
  {
    // Min is guaranteed to be less than or equal to max
    BFVIEW(u8, 8, 0, min_lod)  // multiplied by 16
    BFVIEW(u8, 8, 8, max_lod)  // multiplied by 16
    u32 hex;
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
