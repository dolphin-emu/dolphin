// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/PixelShaderManager.h"

#include <iterator>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/OnScreenDisplay.h"


static bool s_bFogRangeAdjustChanged;
static bool s_bViewPortChanged;
static bool s_bIndirectDirty;
static bool s_bDestAlphaDirty;

static u32 s_alphaTest;
static u32 s_fogRangeBase;
static u32 s_dstalpha;
static u32 s_late_ztest;
static u32 s_rgba6_format;
static u32 s_dither;

static u32 s_blend_enable;
static u32 s_blend_src_factor;
static u32 s_blend_src_factor_alpha;
static u32 s_blend_dst_factor;
static u32 s_blend_dst_factor_alpha;
static u32 s_blend_subtract;
static u32 s_blend_subtract_alpha;

PixelShaderConstants PixelShaderManager::constants;
using uint4 = std::array<u32, 4>;
using int4 = std::array<s32, 4>;
static struct ConstantsPadding
{
  u32 genmode;                  // .z
  u32 alphaTest;                // .w
  u32 fogParam3;                // .x
  u32 fogRangeBase;             // .y
  u32 dstalpha;                 // .z
  u32 ztex_op;                  // .w
  u32 late_ztest;               // .x (bool)
  u32 rgba6_format;             // .y (bool)
  u32 dither;                   // .z (bool)
  u32 bounding_box;             // .w (bool)
  std::array<uint4, 16> pack1;  // .xy - combiners, .z - tevind, .w - iref
  std::array<uint4, 8> pack2;   // .x - tevorder, .y - tevksel
  std::array<int4, 32> konst;   // .rgba
  // The following are used in ubershaders when using shader_framebuffer_fetch blending
  u32 blend_enable;
  u32 blend_src_factor;
  u32 blend_src_factor_alpha;
  u32 blend_dst_factor;
  u32 blend_dst_factor_alpha;
  u32 blend_subtract;
  u32 blend_subtract_alpha;
} StatePaddingAfter{};

bool PixelShaderManager::dirty;

void PixelShaderManager::Init()
{
  constants = {};

  // Init any intial constants which aren't zero when bpmem is zero.
  s_bFogRangeAdjustChanged = true;
  s_bViewPortChanged = false;
  s_bIndirectDirty = false;
  s_bDestAlphaDirty = true;

  s_alphaTest = 0;
  s_fogRangeBase = 0;
  s_dstalpha = 0;
  s_late_ztest = 0;
  s_rgba6_format = 0;
  s_dither = 0;

  s_blend_enable = 0;
  s_blend_src_factor = 0;
  s_blend_src_factor_alpha = 0;
  s_blend_dst_factor = 0;
  s_blend_dst_factor_alpha = 0;
  s_blend_subtract = 0;
  s_blend_subtract_alpha = 0;

  SetIndMatrixChanged(0);
  SetIndMatrixChanged(1);
  SetIndMatrixChanged(2);
  SetZTextureTypeChanged();
  SetTexCoordChanged(0);
  SetTexCoordChanged(1);
  SetTexCoordChanged(2);
  SetTexCoordChanged(3);
  SetTexCoordChanged(4);
  SetTexCoordChanged(5);
  SetTexCoordChanged(6);
  SetTexCoordChanged(7);

  dirty = true;
}

void PixelShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  s_bFogRangeAdjustChanged = true;

  SetEfbScaleChanged(g_renderer->EFBToScaledXf(1), g_renderer->EFBToScaledYf(1));
  SetFogParamChanged();

  dirty = true;
}

void PixelShaderManager::SetConstants()
{
  if (s_bFogRangeAdjustChanged)
  {
    // set by two components, so keep changed flag here
    // TODO: try to split both registers and move this logic to the shader
    if (!g_ActiveConfig.bDisableFog && bpmem.fogRange.Base.Enabled == 1)
    {
      // bpmem.fogRange.Base.Center : center of the viewport in x axis. observation:
      // bpmem.fogRange.Base.Center = realcenter + 342;
      int center = ((u32)bpmem.fogRange.Base.Center) - 342;
      // normalize center to make calculations easy
      float ScreenSpaceCenter = center / (2.0f * xfmem.viewport.wd);
      ScreenSpaceCenter = (ScreenSpaceCenter * 2.0f) - 1.0f;
      // bpmem.fogRange.K seems to be  a table of precalculated coefficients for the adjust factor
      // observations: bpmem.fogRange.K[0].LO appears to be the lowest value and
      // bpmem.fogRange.K[4].HI the largest
      // they always seems to be larger than 256 so my theory is :
      // they are the coefficients from the center to the border of the screen
      // so to simplify I use the hi coefficient as K in the shader taking 256 as the scale
      // TODO: Shouldn't this be EFBToScaledXf?
      constants.fogf[2] = ScreenSpaceCenter;
      constants.fogf[3] =
          static_cast<float>(g_renderer->EFBToScaledX(static_cast<int>(2.0f * xfmem.viewport.wd)));

      for (size_t i = 0, vec_index = 0; i < std::size(bpmem.fogRange.K); i++)
      {
        constexpr float scale = 4.0f;
        constants.fogrange[vec_index / 4][vec_index % 4] = bpmem.fogRange.K[i].GetValue(0) * scale;
        vec_index++;
        constants.fogrange[vec_index / 4][vec_index % 4] = bpmem.fogRange.K[i].GetValue(1) * scale;
        vec_index++;
      }
    }
    else
    {
      constants.fogf[2] = 0;
      constants.fogf[3] = 1;
    }
    dirty = true;

    s_bFogRangeAdjustChanged = false;
  }

  if (s_bViewPortChanged)
  {
    constants.zbias[1][0] = (s32)xfmem.viewport.farZ;
    constants.zbias[1][1] = (s32)xfmem.viewport.zRange;
    dirty = true;
    s_bViewPortChanged = false;
  }

  if (s_bIndirectDirty)
  {
    dirty = true;
    s_bIndirectDirty = false;
  }

  if (s_bDestAlphaDirty)
  {
    // Destination alpha is only enabled if alpha writes are enabled. Force entire uniform to zero
    // when disabled.
    u32 dstalpha = bpmem.blendmode.alphaupdate && bpmem.dstalpha.enable &&
                           bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24 ?
                       bpmem.dstalpha.hex :
                       0;

    if (s_dstalpha != dstalpha)
    {
      s_dstalpha = dstalpha;
      dirty = true;
    }
    s_bDestAlphaDirty = false;
  }
}

void PixelShaderManager::SetTevColor(int index, int component, s32 value)
{
  auto& c = constants.colors[index];
  if(c[component] != value)
  {
    c[component] = value;
    dirty = true;
  }
}

void PixelShaderManager::SetTevKonstColor(int index, int component, s32 value)
{
  auto& c = constants.kcolors[index];
  if(c[component] != value)
  {
    c[component] = value;
    dirty = true;
  }
}

void PixelShaderManager::SetTevOrder(int index, u32 order)
{

}

void PixelShaderManager::SetTevKSel(int index, u32 ksel)
{

}

void PixelShaderManager::SetTevCombiner(int index, int alpha, u32 combiner)
{

}

void PixelShaderManager::SetTevIndirectChanged()
{

}

void PixelShaderManager::SetAlpha()
{
  constants.alpha[0] = bpmem.alpha_test.ref0;
  constants.alpha[1] = bpmem.alpha_test.ref1;
  constants.alpha[3] = static_cast<s32>(bpmem.dstalpha.alpha);
  dirty = true;
}

void PixelShaderManager::SetAlphaTestChanged()
{
  // Force alphaTest Uniform to zero if it will always pass.
  // (set an extra bit to distinguish from "never && never")
  // TODO: we could optimize this further and check the actual constants,
  // i.e. "a <= 0" and "a >= 255" will always pass.
  u32 alpha_test =
      bpmem.alpha_test.TestResult() != AlphaTest::PASS ? bpmem.alpha_test.hex | 1 << 31 : 0;
  if (s_alphaTest != alpha_test)
  {
    s_alphaTest = alpha_test;
    dirty = true;
  }
}

void PixelShaderManager::SetDestAlphaChanged()
{
  s_bDestAlphaDirty = true;
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height)
{
  float rwidth = 1.0f / (width * 128.0f);
  float rheight = 1.0f / (height * 128.0f);

  // TODO: move this check out to callee. There we could just call this function on texture changes
  // or better, use textureSize() in glsl
  if (constants.texdims[texmapid][0] != rwidth || constants.texdims[texmapid][1] != rheight)
    dirty = true;

  constants.texdims[texmapid][0] = rwidth;
  constants.texdims[texmapid][1] = rheight;
}

void PixelShaderManager::SetZTextureBias()
{
  constants.zbias[1][3] = bpmem.ztex1.bias;
  dirty = true;
}

void PixelShaderManager::SetViewportChanged()
{
  s_bViewPortChanged = true;
  s_bFogRangeAdjustChanged =
      true;  // TODO: Shouldn't be necessary with an accurate fog range adjust implementation
}

void PixelShaderManager::SetEfbScaleChanged(float scalex, float scaley)
{
  constants.efbscale[0] = 1.0f / scalex;
  constants.efbscale[1] = 1.0f / scaley;
  dirty = true;
}

void PixelShaderManager::SetZSlope(float dfdx, float dfdy, float f0)
{
  constants.zslope[0] = dfdx;
  constants.zslope[1] = dfdy;
  constants.zslope[2] = f0;
  dirty = true;
}

void PixelShaderManager::SetIndTexScaleChanged(bool high)
{
  constants.indtexscale[high][0] = bpmem.texscale[high].ss0;
  constants.indtexscale[high][1] = bpmem.texscale[high].ts0;
  constants.indtexscale[high][2] = bpmem.texscale[high].ss1;
  constants.indtexscale[high][3] = bpmem.texscale[high].ts1;
  dirty = true;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
  int scale = ((u32)bpmem.indmtx[matrixidx].col0.s0 << 0) |
              ((u32)bpmem.indmtx[matrixidx].col1.s1 << 2) |
              ((u32)bpmem.indmtx[matrixidx].col2.s2 << 4);

  // xyz - static matrix
  // w - dynamic matrix scale / 128
  constants.indtexmtx[2 * matrixidx][0] = bpmem.indmtx[matrixidx].col0.ma;
  constants.indtexmtx[2 * matrixidx][1] = bpmem.indmtx[matrixidx].col1.mc;
  constants.indtexmtx[2 * matrixidx][2] = bpmem.indmtx[matrixidx].col2.me;
  constants.indtexmtx[2 * matrixidx][3] = 17 - scale;
  constants.indtexmtx[2 * matrixidx + 1][0] = bpmem.indmtx[matrixidx].col0.mb;
  constants.indtexmtx[2 * matrixidx + 1][1] = bpmem.indmtx[matrixidx].col1.md;
  constants.indtexmtx[2 * matrixidx + 1][2] = bpmem.indmtx[matrixidx].col2.mf;
  constants.indtexmtx[2 * matrixidx + 1][3] = 17 - scale;
  dirty = true;

  PRIM_LOG("indmtx%d: scale=%d, mat=(%d %d %d; %d %d %d)", matrixidx, scale,
           bpmem.indmtx[matrixidx].col0.ma, bpmem.indmtx[matrixidx].col1.mc,
           bpmem.indmtx[matrixidx].col2.me, bpmem.indmtx[matrixidx].col0.mb,
           bpmem.indmtx[matrixidx].col1.md, bpmem.indmtx[matrixidx].col2.mf);
}

void PixelShaderManager::SetZTextureTypeChanged()
{
  switch (bpmem.ztex2.type)
  {
  case TEV_ZTEX_TYPE_U8:
    constants.zbias[0][0] = 0;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 1;
    break;
  case TEV_ZTEX_TYPE_U16:
    constants.zbias[0][0] = 1;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 256;
    break;
  case TEV_ZTEX_TYPE_U24:
    constants.zbias[0][0] = 65536;
    constants.zbias[0][1] = 256;
    constants.zbias[0][2] = 1;
    constants.zbias[0][3] = 0;
    break;
  default:
    break;
  }
  dirty = true;
}

void PixelShaderManager::SetZTextureOpChanged()
{

}

void PixelShaderManager::SetTexCoordChanged(u8 texmapid)
{
  TCoordInfo& tc = bpmem.texcoords[texmapid];
  constants.texdims[texmapid][2] = (float)(tc.s.scale_minus_1 + 1) * 128.0f;
  constants.texdims[texmapid][3] = (float)(tc.t.scale_minus_1 + 1) * 128.0f;
  dirty = true;
}

void PixelShaderManager::SetFogColorChanged()
{
  if (g_ActiveConfig.bDisableFog)
    return;

  constants.fogcolor[0] = bpmem.fog.color.r;
  constants.fogcolor[1] = bpmem.fog.color.g;
  constants.fogcolor[2] = bpmem.fog.color.b;
  dirty = true;
}

void PixelShaderManager::SetFogParamChanged()
{
  if (!g_ActiveConfig.bDisableFog)
  {
    constants.fogf[0] = bpmem.fog.GetA();
    constants.fogf[1] = bpmem.fog.GetC();
    constants.fogi[1] = bpmem.fog.b_magnitude;
    constants.fogi[3] = bpmem.fog.b_shift;
  }
  else
  {
    constants.fogf[0] = 0.f;
    constants.fogf[1] = 0.f;
    constants.fogi[1] = 1;
    constants.fogi[3] = 1;
  }
  dirty = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
  if (g_ActiveConfig.bDisableFog)
    return;

  s_bFogRangeAdjustChanged = true;

  if (s_fogRangeBase != bpmem.fogRange.Base.hex)
  {
    s_fogRangeBase = bpmem.fogRange.Base.hex;
    dirty = true;
  }
}

void PixelShaderManager::SetGenModeChanged()
{

}

void PixelShaderManager::SetZModeControl()
{
  u32 late_ztest = bpmem.zmode.testenable && !bpmem.zcontrol.early_ztest;
  u32 rgba6_format = (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24 && !g_ActiveConfig.bForceTrueColor) ? 1 : 0;
  u32 dither = rgba6_format && bpmem.blendmode.dither;
  if (s_late_ztest != late_ztest || s_rgba6_format != rgba6_format || s_dither != dither)
  {
    s_late_ztest = late_ztest;
    s_rgba6_format = rgba6_format;
    s_dither = dither;
    dirty = true;
  }
  s_bDestAlphaDirty = true;
}

void PixelShaderManager::SetBlendModeChanged()
{
  u32 dither = s_rgba6_format && bpmem.blendmode.dither;
  if (s_dither != dither)
  {
    s_dither = dither;
    dirty = true;
  }
  BlendingState state = {};
  state.Generate(bpmem);
  if (s_blend_enable != state.blendenable)
  {
    s_blend_enable = state.blendenable;
    dirty = true;
  }
  if (s_blend_src_factor != state.srcfactor)
  {
    s_blend_src_factor = state.srcfactor;
    dirty = true;
  }
  if (s_blend_src_factor_alpha != state.srcfactoralpha)
  {
    s_blend_src_factor_alpha = state.srcfactoralpha;
    dirty = true;
  }
  if (s_blend_dst_factor != state.dstfactor)
  {
    s_blend_dst_factor = state.dstfactor;
    dirty = true;
  }
  if (s_blend_dst_factor_alpha != state.dstfactoralpha)
  {
    s_blend_dst_factor_alpha = state.dstfactoralpha;
    dirty = true;
  }
  if (s_blend_subtract != state.subtract)
  {
    s_blend_subtract = state.subtract;
    dirty = true;
  }
  if (s_blend_subtract_alpha != state.subtractAlpha)
  {
    s_blend_subtract_alpha = state.subtractAlpha;
    dirty = true;
  }
  s_bDestAlphaDirty = true;
}

void PixelShaderManager::SetBoundingBoxActive(bool active)
{
  if (active)
  {
    if (!g_ActiveConfig.backend_info.bSupportsBBox)
    {
      OSD::AddTypedMessage(OSD::MessageType::BoundingBoxNotice, "Bounding box is not available!",
                           4000);
    }
    else if (g_ActiveConfig.bBBoxEnable)
    {
      OSD::AddTypedMessage(OSD::MessageType::BoundingBoxNotice, "Bounding box is active.", 4000);
    }
  }
}

void PixelShaderManager::DoState(PointerWrap& p)
{
  p.Do(s_bFogRangeAdjustChanged);
  p.Do(s_bViewPortChanged);
  p.Do(s_bIndirectDirty);
  p.Do(s_bDestAlphaDirty);

  p.Do(constants);
  p.Do(StatePaddingAfter);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // Fixup the current state from global GPU state
    // NOTE: This requires that all GPU memory has been loaded already.
    Dirty();
  }
}
