// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/RenderState.h"
#include <algorithm>
#include <array>
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/TextureConfig.h"

void RasterizationState::Generate(const BPMemory& bp, PrimitiveType primitive_type)
{
  cullmode = bp.genMode.cullmode;
  primitive = primitive_type;

  // Back-face culling should be disabled for points/lines.
  if (primitive_type != PrimitiveType::Triangles && primitive_type != PrimitiveType::TriangleStrip)
    cullmode = CullMode::None;
}

RasterizationState& RasterizationState::operator=(const RasterizationState& rhs)
{
  hex = rhs.hex;
  return *this;
}

FramebufferState& FramebufferState::operator=(const FramebufferState& rhs)
{
  hex = rhs.hex;
  return *this;
}

void DepthState::Generate(const BPMemory& bp)
{
  testenable = bp.zmode.testenable.Value();
  updateenable = bp.zmode.updateenable.Value();
  func = bp.zmode.func.Value();
}

DepthState& DepthState::operator=(const DepthState& rhs)
{
  hex = rhs.hex;
  return *this;
}

// If the framebuffer format has no alpha channel, it is assumed to
// ONE on blending. As the backends may emulate this framebuffer
// configuration with an alpha channel, we just drop all references
// to the destination alpha channel.
static SrcBlendFactor RemoveDstAlphaUsage(SrcBlendFactor factor)
{
  switch (factor)
  {
  case SrcBlendFactor::DstAlpha:
    return SrcBlendFactor::One;
  case SrcBlendFactor::InvDstAlpha:
    return SrcBlendFactor::Zero;
  default:
    return factor;
  }
}

static DstBlendFactor RemoveDstAlphaUsage(DstBlendFactor factor)
{
  switch (factor)
  {
  case DstBlendFactor::DstAlpha:
    return DstBlendFactor::One;
  case DstBlendFactor::InvDstAlpha:
    return DstBlendFactor::Zero;
  default:
    return factor;
  }
}

// We separate the blending parameter for rgb and alpha. For blending
// the alpha component, CLR and ALPHA are indentical. So just always
// use ALPHA as this makes it easier for the backends to use the second
// alpha value of dual source blending.
static DstBlendFactor RemoveSrcColorUsage(DstBlendFactor factor)
{
  switch (factor)
  {
  case DstBlendFactor::SrcClr:
    return DstBlendFactor::SrcAlpha;
  case DstBlendFactor::InvSrcClr:
    return DstBlendFactor::InvSrcAlpha;
  default:
    return factor;
  }
}

// Same as RemoveSrcColorUsage, but because of the overlapping enum,
// this must be written as another function.
static SrcBlendFactor RemoveDstColorUsage(SrcBlendFactor factor)
{
  switch (factor)
  {
  case SrcBlendFactor::DstClr:
    return SrcBlendFactor::DstAlpha;
  case SrcBlendFactor::InvDstClr:
    return SrcBlendFactor::InvDstAlpha;
  default:
    return factor;
  }
}

void BlendingState::Generate(const BPMemory& bp)
{
  // Start with everything disabled.
  hex = 0;

  bool target_has_alpha = bp.zcontrol.pixel_format == PixelFormat::RGBA6_Z24;
  bool alpha_test_may_succeed = bp.alpha_test.TestResult() != AlphaTestResult::Fail;

  colorupdate = bp.blendmode.colorupdate && alpha_test_may_succeed;
  alphaupdate = bp.blendmode.alphaupdate && target_has_alpha && alpha_test_may_succeed;
  dstalpha = bp.dstalpha.enable && alphaupdate;
  usedualsrc = true;

  // The subtract bit has the highest priority
  if (bp.blendmode.subtract)
  {
    blendenable = true;
    subtractAlpha = subtract = true;
    srcfactoralpha = srcfactor = SrcBlendFactor::One;
    dstfactoralpha = dstfactor = DstBlendFactor::One;

    if (dstalpha)
    {
      subtractAlpha = false;
      srcfactoralpha = SrcBlendFactor::One;
      dstfactoralpha = DstBlendFactor::Zero;
    }
  }

  // The blendenable bit has the middle priority
  else if (bp.blendmode.blendenable)
  {
    blendenable = true;
    srcfactor = bp.blendmode.srcfactor;
    dstfactor = bp.blendmode.dstfactor;
    if (!target_has_alpha)
    {
      // uses ONE instead of DSTALPHA
      srcfactor = RemoveDstAlphaUsage(srcfactor);
      dstfactor = RemoveDstAlphaUsage(dstfactor);
    }
    // replaces SrcClr with SrcAlpha and DstClr with DstAlpha, it is important to
    // use the dst function for the src factor and vice versa
    srcfactoralpha = RemoveDstColorUsage(srcfactor);
    dstfactoralpha = RemoveSrcColorUsage(dstfactor);

    if (dstalpha)
    {
      srcfactoralpha = SrcBlendFactor::One;
      dstfactoralpha = DstBlendFactor::Zero;
    }
  }

  // The logicop bit has the lowest priority
  else if (bp.blendmode.logicopenable)
  {
    if (bp.blendmode.logicmode == LogicOp::NoOp)
    {
      // Fast path for Kirby's Return to Dreamland, they use it with dstAlpha.
      colorupdate = false;
      alphaupdate = alphaupdate && dstalpha;
    }
    else
    {
      logicopenable = true;
      logicmode = bp.blendmode.logicmode;

      if (dstalpha)
      {
        // TODO: Not supported by backends.
      }
    }
  }
}

void BlendingState::ApproximateLogicOpWithBlending()
{
  struct LogicOpApproximation
  {
    bool subtract;
    SrcBlendFactor srcfactor;
    DstBlendFactor dstfactor;
  };
  // TODO: This previously had a warning about SRC and DST being aliased and not to mix them,
  // but INVSRCCLR and INVDSTCLR were also aliased and were mixed.
  // Thus, NOR, EQUIV, INVERT, COPY_INVERTED, and OR_INVERTED duplicate(d) other values.
  static constexpr std::array<LogicOpApproximation, 16> approximations = {{
      {false, SrcBlendFactor::Zero, DstBlendFactor::Zero},            // CLEAR
      {false, SrcBlendFactor::DstClr, DstBlendFactor::Zero},          // AND
      {true, SrcBlendFactor::One, DstBlendFactor::InvSrcClr},         // AND_REVERSE
      {false, SrcBlendFactor::One, DstBlendFactor::Zero},             // COPY
      {true, SrcBlendFactor::DstClr, DstBlendFactor::One},            // AND_INVERTED
      {false, SrcBlendFactor::Zero, DstBlendFactor::One},             // NOOP
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::InvSrcClr},  // XOR
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::One},        // OR
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::InvSrcClr},  // NOR
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::Zero},       // EQUIV
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::InvSrcClr},  // INVERT
      {false, SrcBlendFactor::One, DstBlendFactor::InvDstAlpha},      // OR_REVERSE
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::InvSrcClr},  // COPY_INVERTED
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::One},        // OR_INVERTED
      {false, SrcBlendFactor::InvDstClr, DstBlendFactor::InvSrcClr},  // NAND
      {false, SrcBlendFactor::One, DstBlendFactor::One},              // SET
  }};

  logicopenable = false;
  blendenable = true;
  subtract = approximations[u32(logicmode.Value())].subtract;
  srcfactor = approximations[u32(logicmode.Value())].srcfactor;
  dstfactor = approximations[u32(logicmode.Value())].dstfactor;
}

BlendingState& BlendingState::operator=(const BlendingState& rhs)
{
  hex = rhs.hex;
  return *this;
}

void SamplerState::Generate(const BPMemory& bp, u32 index)
{
  const FourTexUnits& tex = bpmem.tex[index / 4];
  const TexMode0& tm0 = tex.texMode0[index % 4];
  const TexMode1& tm1 = tex.texMode1[index % 4];

  // GX can configure the mip filter to none. However, D3D and Vulkan can't express this in their
  // sampler states. Therefore, we set the min/max LOD to zero if this option is used.
  min_filter = tm0.min_filter == FilterMode::Linear ? Filter::Linear : Filter::Point;
  mipmap_filter = tm0.mipmap_filter == MipMode::Linear ? Filter::Linear : Filter::Point;
  mag_filter = tm0.mag_filter == FilterMode::Linear ? Filter::Linear : Filter::Point;

  // If mipmaps are disabled, clamp min/max lod
  max_lod = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm1.max_lod.Value() : 0;
  min_lod = std::min(max_lod.Value(), static_cast<u64>(tm1.min_lod));
  lod_bias = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm0.lod_bias * (256 / 32) : 0;

  // Address modes
  // Hardware testing indicates that wrap_mode set to 3 behaves the same as clamp.
  static constexpr std::array<AddressMode, 4> address_modes = {
      {AddressMode::Clamp, AddressMode::Repeat, AddressMode::MirroredRepeat, AddressMode::Clamp}};
  wrap_u = address_modes[u32(tm0.wrap_s.Value())];
  wrap_v = address_modes[u32(tm0.wrap_t.Value())];
  anisotropic_filtering = 0;
}

SamplerState& SamplerState::operator=(const SamplerState& rhs)
{
  hex = rhs.hex;
  return *this;
}

namespace RenderState
{
RasterizationState GetInvalidRasterizationState()
{
  RasterizationState state;
  state.hex = UINT32_C(0xFFFFFFFF);
  return state;
}

RasterizationState GetNoCullRasterizationState(PrimitiveType primitive)
{
  RasterizationState state = {};
  state.cullmode = CullMode::None;
  state.primitive = primitive;
  return state;
}

RasterizationState GetCullBackFaceRasterizationState(PrimitiveType primitive)
{
  RasterizationState state = {};
  state.cullmode = CullMode::Back;
  state.primitive = primitive;
  return state;
}

DepthState GetInvalidDepthState()
{
  DepthState state;
  state.hex = UINT32_C(0xFFFFFFFF);
  return state;
}

DepthState GetNoDepthTestingDepthState()
{
  DepthState state = {};
  state.testenable = false;
  state.updateenable = false;
  state.func = CompareMode::Always;
  return state;
}

DepthState GetAlwaysWriteDepthState()
{
  DepthState state = {};
  state.testenable = true;
  state.updateenable = true;
  state.func = CompareMode::Always;
  return state;
}

BlendingState GetInvalidBlendingState()
{
  BlendingState state;
  state.hex = UINT32_C(0xFFFFFFFF);
  return state;
}

BlendingState GetNoBlendingBlendState()
{
  BlendingState state = {};
  state.usedualsrc = false;
  state.blendenable = false;
  state.srcfactor = SrcBlendFactor::One;
  state.srcfactoralpha = SrcBlendFactor::One;
  state.dstfactor = DstBlendFactor::Zero;
  state.dstfactoralpha = DstBlendFactor::Zero;
  state.logicopenable = false;
  state.colorupdate = true;
  state.alphaupdate = true;
  return state;
}

BlendingState GetNoColorWriteBlendState()
{
  BlendingState state = {};
  state.usedualsrc = false;
  state.blendenable = false;
  state.srcfactor = SrcBlendFactor::One;
  state.srcfactoralpha = SrcBlendFactor::One;
  state.dstfactor = DstBlendFactor::Zero;
  state.dstfactoralpha = DstBlendFactor::Zero;
  state.logicopenable = false;
  state.colorupdate = false;
  state.alphaupdate = false;
  return state;
}

SamplerState GetInvalidSamplerState()
{
  SamplerState state;
  state.hex = UINT64_C(0xFFFFFFFFFFFFFFFF);
  return state;
}

SamplerState GetPointSamplerState()
{
  SamplerState state = {};
  state.min_filter = SamplerState::Filter::Point;
  state.mag_filter = SamplerState::Filter::Point;
  state.mipmap_filter = SamplerState::Filter::Point;
  state.wrap_u = SamplerState::AddressMode::Clamp;
  state.wrap_v = SamplerState::AddressMode::Clamp;
  state.min_lod = 0;
  state.max_lod = 255;
  state.lod_bias = 0;
  state.anisotropic_filtering = false;
  return state;
}

SamplerState GetLinearSamplerState()
{
  SamplerState state = {};
  state.min_filter = SamplerState::Filter::Linear;
  state.mag_filter = SamplerState::Filter::Linear;
  state.mipmap_filter = SamplerState::Filter::Linear;
  state.wrap_u = SamplerState::AddressMode::Clamp;
  state.wrap_v = SamplerState::AddressMode::Clamp;
  state.min_lod = 0;
  state.max_lod = 255;
  state.lod_bias = 0;
  state.anisotropic_filtering = false;
  return state;
}

FramebufferState GetColorFramebufferState(AbstractTextureFormat format)
{
  FramebufferState state = {};
  state.color_texture_format = format;
  state.depth_texture_format = AbstractTextureFormat::Undefined;
  state.per_sample_shading = false;
  state.samples = 1;
  return state;
}

FramebufferState GetRGBA8FramebufferState()
{
  return GetColorFramebufferState(AbstractTextureFormat::RGBA8);
}

}  // namespace RenderState
