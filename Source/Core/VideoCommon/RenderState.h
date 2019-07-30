// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"

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

union RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  RasterizationState& operator=(const RasterizationState& rhs);

  bool operator==(const RasterizationState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const RasterizationState& rhs) const { return hex != rhs.hex; }
  bool operator<(const RasterizationState& rhs) const { return hex < rhs.hex; }
  BitField<0, 2, GenMode::CullMode> cullmode;
  BitField<3, 2, PrimitiveType> primitive;

  u32 hex;
};

union FramebufferState
{
  BitField<0, 8, AbstractTextureFormat> color_texture_format;
  BitField<8, 8, AbstractTextureFormat> depth_texture_format;
  BitField<16, 8, u32> samples;
  BitField<24, 1, u32> per_sample_shading;

  bool operator==(const FramebufferState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const FramebufferState& rhs) const { return hex != rhs.hex; }
  FramebufferState& operator=(const FramebufferState& rhs);

  u32 hex;
};

union DepthState
{
  void Generate(const BPMemory& bp);

  DepthState& operator=(const DepthState& rhs);

  bool operator==(const DepthState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const DepthState& rhs) const { return hex != rhs.hex; }
  bool operator<(const DepthState& rhs) const { return hex < rhs.hex; }
  BitField<0, 1, u32> testenable;
  BitField<1, 1, u32> updateenable;
  BitField<2, 3, ZMode::CompareMode> func;

  u32 hex;
};

union BlendingState
{
  void Generate(const BPMemory& bp);

  bool IsDualSourceBlend() const
  {
    return dstalpha && (srcfactor == BlendMode::SRCALPHA || srcfactor == BlendMode::INVSRCALPHA);
  }

  BlendingState& operator=(const BlendingState& rhs);

  bool operator==(const BlendingState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const BlendingState& rhs) const { return hex != rhs.hex; }
  bool operator<(const BlendingState& rhs) const { return hex < rhs.hex; }
  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  BitField<2, 1, u32> dstalpha;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  BitField<5, 1, u32> subtract;
  BitField<6, 1, u32> subtractAlpha;
  BitField<7, 1, u32> _usedualsrc;  // useless
  BitField<8, 3, BlendMode::BlendFactor> dstfactor;
  BitField<11, 3, BlendMode::BlendFactor> srcfactor;
  BitField<14, 3, BlendMode::BlendFactor> dstfactoralpha;
  BitField<17, 3, BlendMode::BlendFactor> srcfactoralpha;
  BitField<20, 4, BlendMode::LogicOp> logicmode;

  u32 hex;
};

union SamplerState
{
  using StorageType = u64;

  enum class Filter : StorageType
  {
    Point,
    Linear
  };

  enum class AddressMode : StorageType
  {
    Clamp,
    Repeat,
    MirroredRepeat
  };

  void Generate(const BPMemory& bp, u32 index);

  SamplerState& operator=(const SamplerState& rhs);

  bool operator==(const SamplerState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const SamplerState& rhs) const { return hex != rhs.hex; }
  bool operator<(const SamplerState& rhs) const { return hex < rhs.hex; }
  BitField<0, 1, Filter> min_filter;
  BitField<1, 1, Filter> mag_filter;
  BitField<2, 1, Filter> mipmap_filter;
  BitField<3, 2, AddressMode> wrap_u;
  BitField<5, 2, AddressMode> wrap_v;
  BitField<7, 16, s64> lod_bias;  // multiplied by 256
  BitField<23, 8, u64> min_lod;   // multiplied by 16
  BitField<31, 8, u64> max_lod;   // multiplied by 16
  BitField<39, 1, u64> anisotropic_filtering;

  StorageType hex;
};

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
