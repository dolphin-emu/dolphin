// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

struct BPMemory;

enum class AbstractTextureFormat : u32;

enum class CompareMode : u32;
enum class CullMode : u32;
enum class DstBlendFactor : u32;
enum class FilterMode : u32;
enum class LODType : u32;
enum class LogicOp : u32;
enum class PixelFormat : u32;
enum class SrcBlendFactor : u32;
enum class WrapMode : u32;

enum class PrimitiveType : u32
{
  Points,
  Lines,
  Triangles,
  TriangleStrip,
};

union RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  RasterizationState() = default;
  RasterizationState(const RasterizationState&) = default;
  RasterizationState& operator=(const RasterizationState& rhs)
  {
    hex = rhs.hex;
    return *this;
  }
  RasterizationState(RasterizationState&&) = default;
  RasterizationState& operator=(RasterizationState&& rhs)
  {
    hex = rhs.hex;
    return *this;
  }

  bool operator==(const RasterizationState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const RasterizationState& rhs) const { return !operator==(rhs); }
  bool operator<(const RasterizationState& rhs) const { return hex < rhs.hex; }

  BitField<0, 2, CullMode> cullmode;
  BitField<3, 2, PrimitiveType> primitive;

  u32 hex = 0;
};

union FramebufferState
{
  FramebufferState() = default;
  FramebufferState(const FramebufferState&) = default;
  FramebufferState& operator=(const FramebufferState& rhs)
  {
    hex = rhs.hex;
    return *this;
  }
  FramebufferState(FramebufferState&&) = default;
  FramebufferState& operator=(FramebufferState&& rhs)
  {
    hex = rhs.hex;
    return *this;
  }

  bool operator==(const FramebufferState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const FramebufferState& rhs) const { return !operator==(rhs); }

  BitField<0, 8, AbstractTextureFormat> color_texture_format;
  BitField<8, 8, AbstractTextureFormat> depth_texture_format;
  BitField<16, 8, u32> samples;
  BitField<24, 1, u32> per_sample_shading;

  u32 hex = 0;
};

union DepthState
{
  void Generate(const BPMemory& bp);

  DepthState() = default;
  DepthState(const DepthState&) = default;
  DepthState& operator=(const DepthState& rhs)
  {
    hex = rhs.hex;
    return *this;
  }
  DepthState(DepthState&&) = default;
  DepthState& operator=(DepthState&& rhs)
  {
    hex = rhs.hex;
    return *this;
  }

  bool operator==(const DepthState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const DepthState& rhs) const { return !operator==(rhs); }
  bool operator<(const DepthState& rhs) const { return hex < rhs.hex; }

  BitField<0, 1, u32> testenable;
  BitField<1, 1, u32> updateenable;
  BitField<2, 3, CompareMode> func;

  u32 hex = 0;
};

union BlendingState
{
  void Generate(const BPMemory& bp);

  // HACK: Replaces logical operations with blend operations.
  // Will not be bit-correct, and in some cases not even remotely in the same ballpark.
  void ApproximateLogicOpWithBlending();
  bool LogicOpApproximationIsExact();
  bool LogicOpApproximationWantsShaderHelp();

  BlendingState() = default;
  BlendingState(const BlendingState&) = default;
  BlendingState& operator=(const BlendingState& rhs)
  {
    hex = rhs.hex;
    return *this;
  }
  BlendingState(BlendingState&&) = default;
  BlendingState& operator=(BlendingState&& rhs)
  {
    hex = rhs.hex;
    return *this;
  }

  bool operator==(const BlendingState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const BlendingState& rhs) const { return !operator==(rhs); }
  bool operator<(const BlendingState& rhs) const { return hex < rhs.hex; }

  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  BitField<5, 1, u32> subtract;
  BitField<6, 1, u32> subtractAlpha;
  BitField<7, 1, u32> usedualsrc;
  BitField<8, 3, DstBlendFactor> dstfactor;
  BitField<11, 3, SrcBlendFactor> srcfactor;
  BitField<14, 3, DstBlendFactor> dstfactoralpha;
  BitField<17, 3, SrcBlendFactor> srcfactoralpha;
  BitField<20, 4, LogicOp> logicmode;

  bool RequiresDualSrc() const;

  u32 hex = 0;
};

struct SamplerState
{
  void Generate(const BPMemory& bp, u32 index);

  SamplerState() = default;
  SamplerState(const SamplerState&) = default;
  SamplerState& operator=(const SamplerState& rhs)
  {
    tm0.hex = rhs.tm0.hex;
    tm1.hex = rhs.tm1.hex;
    return *this;
  }
  SamplerState(SamplerState&&) = default;
  SamplerState& operator=(SamplerState&& rhs)
  {
    tm0.hex = rhs.tm0.hex;
    tm1.hex = rhs.tm1.hex;
    return *this;
  }

  bool operator==(const SamplerState& rhs) const { return Hex() == rhs.Hex(); }
  bool operator!=(const SamplerState& rhs) const { return !operator==(rhs); }
  bool operator<(const SamplerState& rhs) const { return Hex() < rhs.Hex(); }

  constexpr u64 Hex() const { return tm0.hex | (static_cast<u64>(tm1.hex) << 32); }

  // Based on BPMemory TexMode0/TexMode1, but with slightly higher precision and some
  // simplifications
  union TM0
  {
    // BP's mipmap_filter can be None, but that is represented here by setting min_lod and max_lod
    // to 0
    BitField<0, 1, FilterMode> min_filter;
    BitField<1, 1, FilterMode> mag_filter;
    BitField<2, 1, FilterMode> mipmap_filter;
    // Guaranteed to be valid values (i.e. not 3)
    BitField<3, 2, WrapMode> wrap_u;
    BitField<5, 2, WrapMode> wrap_v;
    BitField<7, 1, LODType> diag_lod;
    BitField<8, 16, s32> lod_bias;         // multiplied by 256, higher precision than normal
    BitField<24, 1, bool, u32> lod_clamp;  // TODO: This isn't currently implemented
    BitField<25, 1, bool, u32> anisotropic_filtering;  // TODO: This doesn't use the BP one yet
    u32 hex = 0;
  };
  union TM1
  {
    // Min is guaranteed to be less than or equal to max
    BitField<0, 8, u32> min_lod;  // multiplied by 16
    BitField<8, 8, u32> max_lod;  // multiplied by 16
    u32 hex = 0;
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
