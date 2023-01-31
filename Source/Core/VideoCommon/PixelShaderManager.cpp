// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PixelShaderManager.h"

#include <iterator>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

void PixelShaderManager::Init()
{
  constants = {};

  // Init any intial constants which aren't zero when bpmem is zero.
  m_fog_range_adjusted_changed = true;
  m_viewport_changed = false;

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

  // fixed Konstants
  for (int component = 0; component < 4; component++)
  {
    constants.konst[0][component] = 255;  // 1
    constants.konst[1][component] = 223;  // 7/8
    constants.konst[2][component] = 191;  // 3/4
    constants.konst[3][component] = 159;  // 5/8
    constants.konst[4][component] = 128;  // 1/2
    constants.konst[5][component] = 96;   // 3/8
    constants.konst[6][component] = 64;   // 1/4
    constants.konst[7][component] = 32;   // 1/8

    // Invalid Konstants (reads as zero on hardware)
    constants.konst[8][component] = 0;
    constants.konst[9][component] = 0;
    constants.konst[10][component] = 0;
    constants.konst[11][component] = 0;

    // Annoyingly, alpha reads zero values for the .rgb colors (offically
    // defined as invalid)
    // If it wasn't for this, we could just use one of the first 3 colunms
    // instead of
    // wasting an entire 4th column just for alpha.
    if (component == 3)
    {
      constants.konst[12][component] = 0;
      constants.konst[13][component] = 0;
      constants.konst[14][component] = 0;
      constants.konst[15][component] = 0;
    }
  }

  Dirty();
}

void PixelShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  m_fog_range_adjusted_changed = true;

  SetEfbScaleChanged(g_framebuffer_manager->EFBToScaledXf(1),
                     g_framebuffer_manager->EFBToScaledYf(1));
  SetFogParamChanged();

  dirty = true;
}

void PixelShaderManager::SetConstants()
{
  if (m_fog_range_adjusted_changed)
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
      constants.fogf[3] = static_cast<float>(
          g_framebuffer_manager->EFBToScaledX(static_cast<int>(2.0f * xfmem.viewport.wd)));

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

    m_fog_range_adjusted_changed = false;
  }

  if (m_viewport_changed)
  {
    constants.zbias[1][0] = (s32)xfmem.viewport.farZ;
    constants.zbias[1][1] = (s32)xfmem.viewport.zRange;
    dirty = true;
    m_viewport_changed = false;
  }

  if (m_indirect_dirty)
  {
    for (int i = 0; i < 4; i++)
      constants.pack1[i][3] = 0;

    for (u32 i = 0; i < (bpmem.genMode.numtevstages + 1); ++i)
    {
      // Note: a tevind of zero just happens to be a passthrough, so no need
      // to set an extra bit.  Furthermore, wrap and add to previous apply even if there is no
      // indirect stage.
      constants.pack1[i][2] = bpmem.tevind[i].hex;

      u32 stage = bpmem.tevind[i].bt;

      // We use an extra bit (1 << 16) to provide a fast way of testing if this feature is in use.
      // Note also that this is indexed by indirect stage, not by TEV stage.
      if (bpmem.tevind[i].IsActive() && stage < bpmem.genMode.numindstages)
        constants.pack1[stage][3] =
            bpmem.tevindref.getTexCoord(stage) | bpmem.tevindref.getTexMap(stage) << 8 | 1 << 16;
    }

    dirty = true;
    m_indirect_dirty = false;
  }

  if (m_dest_alpha_dirty)
  {
    // Destination alpha is only enabled if alpha writes are enabled. Force entire uniform to zero
    // when disabled.
    u32 dstalpha = bpmem.blendmode.alphaupdate && bpmem.dstalpha.enable &&
                           bpmem.zcontrol.pixel_format == PixelFormat::RGBA6_Z24 ?
                       bpmem.dstalpha.hex :
                       0;

    if (constants.dstalpha != dstalpha)
    {
      constants.dstalpha = dstalpha;
      dirty = true;
    }
  }
}

void PixelShaderManager::SetTevColor(int index, int component, s32 value)
{
  auto& c = constants.colors[index];
  c[component] = value;
  dirty = true;

  PRIM_LOG("tev color{}: {} {} {} {}", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetTevKonstColor(int index, int component, s32 value)
{
  auto& c = constants.kcolors[index];
  c[component] = value;
  dirty = true;

  // Konst for ubershaders. We build the whole array on cpu so the gpu can do a single indirect
  // access.
  if (component != 3)  // Alpha doesn't included in the .rgb konsts
    constants.konst[index + 12][component] = value;

  // .rrrr .gggg .bbbb .aaaa konsts
  constants.konst[index + 16 + component * 4][0] = value;
  constants.konst[index + 16 + component * 4][1] = value;
  constants.konst[index + 16 + component * 4][2] = value;
  constants.konst[index + 16 + component * 4][3] = value;

  PRIM_LOG("tev konst color{}: {} {} {} {}", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetTevOrder(int index, u32 order)
{
  if (constants.pack2[index][0] != order)
  {
    constants.pack2[index][0] = order;
    dirty = true;
  }
}

void PixelShaderManager::SetTevKSel(int index, u32 ksel)
{
  if (constants.pack2[index][1] != ksel)
  {
    constants.pack2[index][1] = ksel;
    dirty = true;
  }
}

void PixelShaderManager::SetTevCombiner(int index, int alpha, u32 combiner)
{
  if (constants.pack1[index][alpha] != combiner)
  {
    constants.pack1[index][alpha] = combiner;
    dirty = true;
  }
}

void PixelShaderManager::SetTevIndirectChanged()
{
  m_indirect_dirty = true;
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
      bpmem.alpha_test.TestResult() != AlphaTestResult::Pass ? bpmem.alpha_test.hex | 1 << 31 : 0;
  if (constants.alphaTest != alpha_test)
  {
    constants.alphaTest = alpha_test;
    dirty = true;
  }
}

void PixelShaderManager::SetDestAlphaChanged()
{
  m_dest_alpha_dirty = true;
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height)
{
  // TODO: move this check out to callee. There we could just call this function on texture changes
  // or better, use textureSize() in glsl
  if (constants.texdims[texmapid][0] != width || constants.texdims[texmapid][1] != height)
    dirty = true;

  constants.texdims[texmapid][0] = width;
  constants.texdims[texmapid][1] = height;
}

void PixelShaderManager::SetSamplerState(int texmapid, u32 tm0, u32 tm1)
{
  if (constants.pack2[texmapid][2] != tm0 || constants.pack2[texmapid][3] != tm1)
    dirty = true;

  constants.pack2[texmapid][2] = tm0;
  constants.pack2[texmapid][3] = tm1;
}

void PixelShaderManager::SetZTextureBias()
{
  constants.zbias[1][3] = bpmem.ztex1.bias;
  dirty = true;
}

void PixelShaderManager::SetViewportChanged()
{
  m_viewport_changed = true;
  m_fog_range_adjusted_changed =
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
  const u8 scale = bpmem.indmtx[matrixidx].GetScale();

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

  PRIM_LOG("indmtx{}: scale={}, mat=({} {} {}; {} {} {})", matrixidx, scale,
           bpmem.indmtx[matrixidx].col0.ma, bpmem.indmtx[matrixidx].col1.mc,
           bpmem.indmtx[matrixidx].col2.me, bpmem.indmtx[matrixidx].col0.mb,
           bpmem.indmtx[matrixidx].col1.md, bpmem.indmtx[matrixidx].col2.mf);
}

void PixelShaderManager::SetZTextureTypeChanged()
{
  switch (bpmem.ztex2.type)
  {
  case ZTexFormat::U8:
    constants.zbias[0][0] = 0;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 1;
    break;
  case ZTexFormat::U16:
    constants.zbias[0][0] = 1;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 256;
    break;
  case ZTexFormat::U24:
    constants.zbias[0][0] = 65536;
    constants.zbias[0][1] = 256;
    constants.zbias[0][2] = 1;
    constants.zbias[0][3] = 0;
    break;
  default:
    PanicAlertFmt("Invalid ztex format {}", bpmem.ztex2.type);
    break;
  }
  dirty = true;
}

void PixelShaderManager::SetZTextureOpChanged()
{
  constants.ztex_op = bpmem.ztex2.op;
  dirty = true;
}

void PixelShaderManager::SetTexCoordChanged(u8 texmapid)
{
  TCoordInfo& tc = bpmem.texcoords[texmapid];
  constants.texdims[texmapid][2] = tc.s.scale_minus_1 + 1;
  constants.texdims[texmapid][3] = tc.t.scale_minus_1 + 1;
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
    constants.fogParam3 = bpmem.fog.c_proj_fsel.hex;
  }
  else
  {
    constants.fogf[0] = 0.f;
    constants.fogf[1] = 0.f;
    constants.fogi[1] = 1;
    constants.fogi[3] = 1;
    constants.fogParam3 = 0;
  }
  dirty = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
  if (g_ActiveConfig.bDisableFog)
    return;

  m_fog_range_adjusted_changed = true;

  if (constants.fogRangeBase != bpmem.fogRange.Base.hex)
  {
    constants.fogRangeBase = bpmem.fogRange.Base.hex;
    dirty = true;
  }
}

void PixelShaderManager::SetGenModeChanged()
{
  constants.genmode = bpmem.genMode.hex;
  m_indirect_dirty = true;
  dirty = true;
}

void PixelShaderManager::SetZModeControl()
{
  u32 late_ztest = bpmem.GetEmulatedZ() == EmulatedZ::Late;
  u32 rgba6_format =
      (bpmem.zcontrol.pixel_format == PixelFormat::RGBA6_Z24 && !g_ActiveConfig.bForceTrueColor) ?
          1 :
          0;
  u32 dither = rgba6_format && bpmem.blendmode.dither;
  if (constants.late_ztest != late_ztest || constants.rgba6_format != rgba6_format ||
      constants.dither != dither)
  {
    constants.late_ztest = late_ztest;
    constants.rgba6_format = rgba6_format;
    constants.dither = dither;
    dirty = true;
  }
  m_dest_alpha_dirty = true;
}

void PixelShaderManager::SetBlendModeChanged()
{
  u32 dither = constants.rgba6_format && bpmem.blendmode.dither;
  if (constants.dither != dither)
  {
    constants.dither = dither;
    dirty = true;
  }
  BlendingState state = {};
  state.Generate(bpmem);
  if (constants.blend_enable != state.blendenable)
  {
    constants.blend_enable = state.blendenable;
    dirty = true;
  }
  if (constants.blend_src_factor != state.srcfactor)
  {
    constants.blend_src_factor = state.srcfactor;
    dirty = true;
  }
  if (constants.blend_src_factor_alpha != state.srcfactoralpha)
  {
    constants.blend_src_factor_alpha = state.srcfactoralpha;
    dirty = true;
  }
  if (constants.blend_dst_factor != state.dstfactor)
  {
    constants.blend_dst_factor = state.dstfactor;
    dirty = true;
  }
  if (constants.blend_dst_factor_alpha != state.dstfactoralpha)
  {
    constants.blend_dst_factor_alpha = state.dstfactoralpha;
    dirty = true;
  }
  if (constants.blend_subtract != state.subtract)
  {
    constants.blend_subtract = state.subtract;
    dirty = true;
  }
  if (constants.blend_subtract_alpha != state.subtractAlpha)
  {
    constants.blend_subtract_alpha = state.subtractAlpha;
    dirty = true;
  }
  if (constants.logic_op_enable != state.logicopenable)
  {
    constants.logic_op_enable = state.logicopenable;
    dirty = true;
  }
  if (constants.logic_op_mode != state.logicmode)
  {
    constants.logic_op_mode = state.logicmode;
    dirty = true;
  }
  m_dest_alpha_dirty = true;
}

void PixelShaderManager::SetBoundingBoxActive(bool active)
{
  const bool enable = active && g_ActiveConfig.bBBoxEnable;
  if (enable == (constants.bounding_box != 0))
    return;

  constants.bounding_box = active;
  dirty = true;
}

void PixelShaderManager::DoState(PointerWrap& p)
{
  p.Do(m_fog_range_adjusted_changed);
  p.Do(m_viewport_changed);
  p.Do(m_indirect_dirty);
  p.Do(m_dest_alpha_dirty);

  p.Do(constants);

  if (p.IsReadMode())
  {
    // Fixup the current state from global GPU state
    // NOTE: This requires that all GPU memory has been loaded already.
    Dirty();
  }
}
