// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/RenderState.h"
#include <algorithm>
#include <array>
#include "VideoCommon/SamplerCommon.h"

void RasterizationState::Generate(const BPMemory& bp, PrimitiveType primitive_type, bool depth_clip)
{
  cullmode = bp.genMode.cullmode;
  primitive = primitive_type;
  depth_clip_enable = depth_clip;

  // Back-face culling should be disabled for points/lines.
  if (primitive_type != PrimitiveType::Triangles && primitive_type != PrimitiveType::TriangleStrip)
    cullmode = GenMode::CULL_NONE;
}

RasterizationState& RasterizationState::operator=(const RasterizationState& rhs)
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
static BlendMode::BlendFactor RemoveDstAlphaUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::DSTALPHA:
    return BlendMode::ONE;
  case BlendMode::INVDSTALPHA:
    return BlendMode::ZERO;
  default:
    return factor;
  }
}

// We separate the blending parameter for rgb and alpha. For blending
// the alpha component, CLR and ALPHA are indentical. So just always
// use ALPHA as this makes it easier for the backends to use the second
// alpha value of dual source blending.
static BlendMode::BlendFactor RemoveSrcColorUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::SRCCLR:
    return BlendMode::SRCALPHA;
  case BlendMode::INVSRCCLR:
    return BlendMode::INVSRCALPHA;
  default:
    return factor;
  }
}

// Same as RemoveSrcColorUsage, but because of the overlapping enum,
// this must be written as another function.
static BlendMode::BlendFactor RemoveDstColorUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::DSTCLR:
    return BlendMode::DSTALPHA;
  case BlendMode::INVDSTCLR:
    return BlendMode::INVDSTALPHA;
  default:
    return factor;
  }
}

void BlendingState::Generate(const BPMemory& bp)
{
  // Start with everything disabled.
  hex = 0;

  bool target_has_alpha = bp.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  bool alpha_test_may_success = bp.alpha_test.TestResult() != AlphaTest::FAIL;

  colorupdate = bp.blendmode.colorupdate && alpha_test_may_success;
  alphaupdate = bp.blendmode.alphaupdate && target_has_alpha && alpha_test_may_success;
  dstalpha = bp.dstalpha.enable && alphaupdate;
  usedualsrc = true;

  // The subtract bit has the highest priority
  if (bp.blendmode.subtract)
  {
    blendenable = true;
    subtractAlpha = subtract = true;
    srcfactoralpha = srcfactor = BlendMode::ONE;
    dstfactoralpha = dstfactor = BlendMode::ONE;

    if (dstalpha)
    {
      subtractAlpha = false;
      srcfactoralpha = BlendMode::ONE;
      dstfactoralpha = BlendMode::ZERO;
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
    // replaces SRCCLR with SRCALPHA and DSTCLR with DSTALPHA, it is important to
    // use the dst function for the src factor and vice versa
    srcfactoralpha = RemoveDstColorUsage(srcfactor);
    dstfactoralpha = RemoveSrcColorUsage(dstfactor);

    if (dstalpha)
    {
      srcfactoralpha = BlendMode::ONE;
      dstfactoralpha = BlendMode::ZERO;
    }
  }

  // The logicop bit has the lowest priority
  else if (bp.blendmode.logicopenable)
  {
    if (bp.blendmode.logicmode == BlendMode::NOOP)
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
  min_filter = (tm0.min_filter & 4) != 0 ? Filter::Linear : Filter::Point;
  mipmap_filter = (tm0.min_filter & 3) == TexMode0::TEXF_LINEAR ? Filter::Linear : Filter::Point;
  mag_filter = tm0.mag_filter != 0 ? Filter::Linear : Filter::Point;

  // If mipmaps are disabled, clamp min/max lod
  max_lod = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm1.max_lod : 0;
  min_lod = std::min(max_lod.Value(), static_cast<u64>(tm1.min_lod));
  lod_bias = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm0.lod_bias * (256 / 32) : 0;

  // Address modes
  static constexpr std::array<AddressMode, 4> address_modes = {
      {AddressMode::Clamp, AddressMode::Repeat, AddressMode::MirroredRepeat, AddressMode::Repeat}};
  wrap_u = address_modes[tm0.wrap_s];
  wrap_v = address_modes[tm0.wrap_t];
  anisotropic_filtering = 0;
}

SamplerState& SamplerState::operator=(const SamplerState& rhs)
{
  hex = rhs.hex;
  return *this;
}

namespace RenderState
{
RasterizationState GetNoCullRasterizationState()
{
  RasterizationState state = {};
  state.cullmode = GenMode::CULL_NONE;
  return state;
}

DepthState GetNoDepthTestingDepthStencilState()
{
  DepthState state = {};
  state.testenable = false;
  state.updateenable = false;
  state.func = ZMode::ALWAYS;
  return state;
}

BlendingState GetNoBlendingBlendState()
{
  BlendingState state = {};
  state.usedualsrc = false;
  state.blendenable = false;
  state.srcfactor = BlendMode::ONE;
  state.srcfactoralpha = BlendMode::ONE;
  state.dstfactor = BlendMode::ZERO;
  state.dstfactoralpha = BlendMode::ZERO;
  state.logicopenable = false;
  state.colorupdate = true;
  state.alphaupdate = true;
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
}
