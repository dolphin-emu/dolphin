// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/Tev.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/System.h"

#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/SWBoundingBox.h"
#include "VideoBackends/Software/TextureSampler.h"

#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

#ifdef _DEBUG
#define ALLOW_TEV_DUMPS 1
#else
#define ALLOW_TEV_DUMPS 0
#endif

static inline s16 Clamp255(s16 in)
{
  return std::clamp<s16>(in, 0, 255);
}

static inline s16 Clamp1024(s16 in)
{
  return std::clamp<s16>(in, -1024, 1023);
}

void Tev::SetRasColor(RasColorChan colorChan, u32 swaptable)
{
  switch (colorChan)
  {
  case RasColorChan::Color0:
  {
    const u8* color = Color[0];
    const auto& swap = bpmem.tevksel.GetSwapTable(swaptable);
    RasColor.r = color[u32(swap[ColorChannel::Red])];
    RasColor.g = color[u32(swap[ColorChannel::Green])];
    RasColor.b = color[u32(swap[ColorChannel::Blue])];
    RasColor.a = color[u32(swap[ColorChannel::Alpha])];
  }
  break;
  case RasColorChan::Color1:
  {
    const u8* color = Color[1];
    const auto& swap = bpmem.tevksel.GetSwapTable(swaptable);
    RasColor.r = color[u32(swap[ColorChannel::Red])];
    RasColor.g = color[u32(swap[ColorChannel::Green])];
    RasColor.b = color[u32(swap[ColorChannel::Blue])];
    RasColor.a = color[u32(swap[ColorChannel::Alpha])];
  }
  break;
  case RasColorChan::AlphaBump:
  {
    RasColor = TevColor::All(AlphaBump);
  }
  break;
  case RasColorChan::NormalizedAlphaBump:
  {
    const u8 normalized = AlphaBump | AlphaBump >> 5;
    RasColor = TevColor::All(normalized);
  }
  break;
  default:
  {
    if (colorChan != RasColorChan::Zero)
      PanicAlertFmt("Invalid ras color channel: {}", colorChan);

    RasColor = TevColor::All(0);
  }
  break;
  }
}

void Tev::DrawColorRegular(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4])
{
  for (int i = BLU_C; i <= RED_C; i++)
  {
    const InputRegType& InputReg = inputs[i];

    const u16 c = InputReg.c + (InputReg.c >> 7);

    s32 temp = InputReg.a * (256 - c) + (InputReg.b * c);
    temp <<= s_ScaleLShiftLUT[cc.scale];
    temp += (cc.scale == TevScale::Divide2) ? 0 : (cc.op == TevOp::Sub) ? 127 : 128;
    temp >>= 8;
    temp = cc.op == TevOp::Sub ? -temp : temp;

    s32 result = ((InputReg.d + s_BiasLUT[cc.bias]) << s_ScaleLShiftLUT[cc.scale]) + temp;
    result = result >> s_ScaleRShiftLUT[cc.scale];

    Reg[cc.dest][i] = result;
  }
}

void Tev::DrawColorCompare(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4])
{
  for (int i = BLU_C; i <= RED_C; i++)
  {
    u32 a, b;
    switch (cc.compare_mode)
    {
    case TevCompareMode::R8:
      a = inputs[RED_C].a;
      b = inputs[RED_C].b;
      break;

    case TevCompareMode::GR16:
      a = (inputs[GRN_C].a << 8) | inputs[RED_C].a;
      b = (inputs[GRN_C].b << 8) | inputs[RED_C].b;
      break;

    case TevCompareMode::BGR24:
      a = (inputs[BLU_C].a << 16) | (inputs[GRN_C].a << 8) | inputs[RED_C].a;
      b = (inputs[BLU_C].b << 16) | (inputs[GRN_C].b << 8) | inputs[RED_C].b;
      break;

    case TevCompareMode::RGB8:
      a = inputs[i].a;
      b = inputs[i].b;
      break;

    default:
      PanicAlertFmt("Invalid compare mode {}", cc.compare_mode);
      continue;
    }

    if (cc.comparison == TevComparison::GT)
      Reg[cc.dest][i] = inputs[i].d + ((a > b) ? inputs[i].c : 0);
    else
      Reg[cc.dest][i] = inputs[i].d + ((a == b) ? inputs[i].c : 0);
  }
}

void Tev::DrawAlphaRegular(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4])
{
  const InputRegType& InputReg = inputs[ALP_C];

  const u16 c = InputReg.c + (InputReg.c >> 7);

  s32 temp = InputReg.a * (256 - c) + (InputReg.b * c);
  temp <<= s_ScaleLShiftLUT[ac.scale];
  temp += (ac.scale == TevScale::Divide2) ? 0 : (ac.op == TevOp::Sub) ? 127 : 128;
  temp = ac.op == TevOp::Sub ? (-temp >> 8) : (temp >> 8);

  s32 result = ((InputReg.d + s_BiasLUT[ac.bias]) << s_ScaleLShiftLUT[ac.scale]) + temp;
  result = result >> s_ScaleRShiftLUT[ac.scale];

  Reg[ac.dest].a = result;
}

void Tev::DrawAlphaCompare(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4])
{
  u32 a, b;
  switch (ac.compare_mode)
  {
  case TevCompareMode::R8:
    a = inputs[RED_C].a;
    b = inputs[RED_C].b;
    break;

  case TevCompareMode::GR16:
    a = (inputs[GRN_C].a << 8) | inputs[RED_C].a;
    b = (inputs[GRN_C].b << 8) | inputs[RED_C].b;
    break;

  case TevCompareMode::BGR24:
    a = (inputs[BLU_C].a << 16) | (inputs[GRN_C].a << 8) | inputs[RED_C].a;
    b = (inputs[BLU_C].b << 16) | (inputs[GRN_C].b << 8) | inputs[RED_C].b;
    break;

  case TevCompareMode::A8:
    a = inputs[ALP_C].a;
    b = inputs[ALP_C].b;
    break;

  default:
    PanicAlertFmt("Invalid compare mode {}", ac.compare_mode);
    return;
  }

  if (ac.comparison == TevComparison::GT)
    Reg[ac.dest].a = inputs[ALP_C].d + ((a > b) ? inputs[ALP_C].c : 0);
  else
    Reg[ac.dest].a = inputs[ALP_C].d + ((a == b) ? inputs[ALP_C].c : 0);
}

static bool AlphaCompare(int alpha, int ref, CompareMode comp)
{
  switch (comp)
  {
  case CompareMode::Always:
    return true;
  case CompareMode::Never:
    return false;
  case CompareMode::LEqual:
    return alpha <= ref;
  case CompareMode::Less:
    return alpha < ref;
  case CompareMode::GEqual:
    return alpha >= ref;
  case CompareMode::Greater:
    return alpha > ref;
  case CompareMode::Equal:
    return alpha == ref;
  case CompareMode::NEqual:
    return alpha != ref;
  default:
    PanicAlertFmt("Invalid compare mode {}", comp);
    return true;
  }
}

static bool TevAlphaTest(int alpha)
{
  const bool comp0 = AlphaCompare(alpha, bpmem.alpha_test.ref0, bpmem.alpha_test.comp0);
  const bool comp1 = AlphaCompare(alpha, bpmem.alpha_test.ref1, bpmem.alpha_test.comp1);

  switch (bpmem.alpha_test.logic)
  {
  case AlphaTestOp::And:
    return comp0 && comp1;
  case AlphaTestOp::Or:
    return comp0 || comp1;
  case AlphaTestOp::Xor:
    return comp0 ^ comp1;
  case AlphaTestOp::Xnor:
    return !(comp0 ^ comp1);
  default:
    PanicAlertFmt("Invalid AlphaTestOp {}", bpmem.alpha_test.logic);
    return true;
  }
}

static inline s32 WrapIndirectCoord(s32 coord, IndTexWrap wrapMode)
{
  switch (wrapMode)
  {
  case IndTexWrap::ITW_OFF:
    return coord;
  case IndTexWrap::ITW_256:
    return (coord & ((256 << 7) - 1));
  case IndTexWrap::ITW_128:
    return (coord & ((128 << 7) - 1));
  case IndTexWrap::ITW_64:
    return (coord & ((64 << 7) - 1));
  case IndTexWrap::ITW_32:
    return (coord & ((32 << 7) - 1));
  case IndTexWrap::ITW_16:
    return (coord & ((16 << 7) - 1));
  case IndTexWrap::ITW_0:
    return 0;
  default:
    PanicAlertFmt("Invalid indirect wrap mode {}", wrapMode);
    return 0;
  }
}

void Tev::Indirect(unsigned int stageNum, s32 s, s32 t)
{
  const TevStageIndirect& indirect = bpmem.tevind[stageNum];
  const u8* indmap = IndirectTex[indirect.bt];

  s32 indcoord[3];

  // alpha bump select
  switch (indirect.bs)
  {
  case IndTexBumpAlpha::Off:
    AlphaBump = 0;
    break;
  case IndTexBumpAlpha::S:
    AlphaBump = indmap[TextureSampler::ALP_SMP];
    break;
  case IndTexBumpAlpha::T:
    AlphaBump = indmap[TextureSampler::BLU_SMP];
    break;
  case IndTexBumpAlpha::U:
    AlphaBump = indmap[TextureSampler::GRN_SMP];
    break;
  default:
    PanicAlertFmt("Invalid alpha bump {}", indirect.bs);
    return;
  }

  // bias select
  const s16 biasValue = indirect.fmt == IndTexFormat::ITF_8 ? -128 : 1;
  s16 bias[3];
  bias[0] = indirect.bias_s ? biasValue : 0;
  bias[1] = indirect.bias_t ? biasValue : 0;
  bias[2] = indirect.bias_u ? biasValue : 0;

  // format
  switch (indirect.fmt)
  {
  case IndTexFormat::ITF_8:
    indcoord[0] = indmap[TextureSampler::ALP_SMP] + bias[0];
    indcoord[1] = indmap[TextureSampler::BLU_SMP] + bias[1];
    indcoord[2] = indmap[TextureSampler::GRN_SMP] + bias[2];
    AlphaBump = AlphaBump & 0xf8;
    break;
  case IndTexFormat::ITF_5:
    indcoord[0] = (indmap[TextureSampler::ALP_SMP] >> 3) + bias[0];
    indcoord[1] = (indmap[TextureSampler::BLU_SMP] >> 3) + bias[1];
    indcoord[2] = (indmap[TextureSampler::GRN_SMP] >> 3) + bias[2];
    AlphaBump = AlphaBump << 5;
    break;
  case IndTexFormat::ITF_4:
    indcoord[0] = (indmap[TextureSampler::ALP_SMP] >> 4) + bias[0];
    indcoord[1] = (indmap[TextureSampler::BLU_SMP] >> 4) + bias[1];
    indcoord[2] = (indmap[TextureSampler::GRN_SMP] >> 4) + bias[2];
    AlphaBump = AlphaBump << 4;
    break;
  case IndTexFormat::ITF_3:
    indcoord[0] = (indmap[TextureSampler::ALP_SMP] >> 5) + bias[0];
    indcoord[1] = (indmap[TextureSampler::BLU_SMP] >> 5) + bias[1];
    indcoord[2] = (indmap[TextureSampler::GRN_SMP] >> 5) + bias[2];
    AlphaBump = AlphaBump << 3;
    break;
  default:
    PanicAlertFmt("Invalid indirect format {}", indirect.fmt);
    return;
  }

  s32 indtevtrans[2] = {0, 0};

  // matrix multiply - results might overflow, but we don't care since we only use the lower 24 bits
  // of the result.
  if (indirect.matrix_index != IndMtxIndex::Off)
  {
    const IND_MTX& indmtx = bpmem.indmtx[static_cast<u32>(indirect.matrix_index.Value()) - 1];

    const int shift = 17 - indmtx.GetScale();

    switch (indirect.matrix_id)
    {
    case IndMtxId::Indirect:
      // matrix values are S0.10, output format is S17.7, so divide by 8
      indtevtrans[0] = (indmtx.col0.ma * indcoord[0] + indmtx.col1.mc * indcoord[1] +
                        indmtx.col2.me * indcoord[2]) >>
                       3;
      indtevtrans[1] = (indmtx.col0.mb * indcoord[0] + indmtx.col1.md * indcoord[1] +
                        indmtx.col2.mf * indcoord[2]) >>
                       3;
      break;
    case IndMtxId::S:
      // s is S17.7, matrix elements are divided by 256, output is S17.7, so divide by 256. - TODO:
      // Maybe, since s is actually stored as S24, we should divide by 256*64?
      indtevtrans[0] = s * indcoord[0] / 256;
      indtevtrans[1] = t * indcoord[0] / 256;
      break;
    case IndMtxId::T:
      indtevtrans[0] = s * indcoord[1] / 256;
      indtevtrans[1] = t * indcoord[1] / 256;
      break;
    default:
      PanicAlertFmt("Invalid indirect matrix ID {}", indirect.matrix_id);
      return;
    }

    indtevtrans[0] = shift >= 0 ? indtevtrans[0] >> shift : indtevtrans[0] << -shift;
    indtevtrans[1] = shift >= 0 ? indtevtrans[1] >> shift : indtevtrans[1] << -shift;
  }
  else
  {
    // If matrix_index is Off (0), matrix_id should be Indirect (0)
    ASSERT(indirect.matrix_id == IndMtxId::Indirect);
  }

  if (indirect.fb_addprev)
  {
    TexCoord.s += (int)(WrapIndirectCoord(s, indirect.sw) + indtevtrans[0]);
    TexCoord.t += (int)(WrapIndirectCoord(t, indirect.tw) + indtevtrans[1]);
  }
  else
  {
    TexCoord.s = (int)(WrapIndirectCoord(s, indirect.sw) + indtevtrans[0]);
    TexCoord.t = (int)(WrapIndirectCoord(t, indirect.tw) + indtevtrans[1]);
  }
}

void Tev::Draw()
{
  ASSERT(Position[0] >= 0 && Position[0] < s32(EFB_WIDTH));
  ASSERT(Position[1] >= 0 && Position[1] < s32(EFB_HEIGHT));

  INCSTAT(g_stats.this_frame.tev_pixels_in);

  auto& system = Core::System::GetInstance();
  auto& pixel_shader_manager = system.GetPixelShaderManager();

  // initial color values
  for (int i = 0; i < 4; i++)
  {
    Reg[static_cast<TevOutput>(i)].r = pixel_shader_manager.constants.colors[i][0];
    Reg[static_cast<TevOutput>(i)].g = pixel_shader_manager.constants.colors[i][1];
    Reg[static_cast<TevOutput>(i)].b = pixel_shader_manager.constants.colors[i][2];
    Reg[static_cast<TevOutput>(i)].a = pixel_shader_manager.constants.colors[i][3];
  }

  for (unsigned int stageNum = 0; stageNum < bpmem.genMode.numindstages; stageNum++)
  {
    const int stageNum2 = stageNum >> 1;
    const int stageOdd = stageNum & 1;

    u32 texcoordSel = bpmem.tevindref.getTexCoord(stageNum);
    const u32 texmap = bpmem.tevindref.getTexMap(stageNum);

    // Quirk: when the tex coord is not less than the number of tex gens (i.e. the tex coord does
    // not exist), then tex coord 0 is used (though sometimes glitchy effects happen on console).
    // This affects the Mario portrait in Luigi's Mansion, where the developers forgot to set
    // the number of tex gens to 2 (bug 11462).
    if (texcoordSel >= bpmem.genMode.numtexgens)
      texcoordSel = 0;

    const TEXSCALE& texscale = bpmem.texscale[stageNum2];
    const s32 scaleS = stageOdd ? texscale.ss1 : texscale.ss0;
    const s32 scaleT = stageOdd ? texscale.ts1 : texscale.ts0;

    TextureSampler::Sample(Uv[texcoordSel].s >> scaleS, Uv[texcoordSel].t >> scaleT,
                           IndirectLod[stageNum], IndirectLinear[stageNum], texmap,
                           IndirectTex[stageNum]);
  }

  for (unsigned int stageNum = 0; stageNum <= bpmem.genMode.numtevstages; stageNum++)
  {
    const int stageNum2 = stageNum >> 1;
    const int stageOdd = stageNum & 1;
    const TwoTevStageOrders& order = bpmem.tevorders[stageNum2];

    // stage combiners
    const TevStageCombiner::ColorCombiner& cc = bpmem.combiners[stageNum].colorC;
    const TevStageCombiner::AlphaCombiner& ac = bpmem.combiners[stageNum].alphaC;

    u32 texcoordSel = order.getTexCoord(stageOdd);
    const u32 texmap = order.getTexMap(stageOdd);

    // Quirk: when the tex coord is not less than the number of tex gens (i.e. the tex coord does
    // not exist), then tex coord 0 is used (though sometimes glitchy effects happen on console).
    if (texcoordSel >= bpmem.genMode.numtexgens)
      texcoordSel = 0;

    Indirect(stageNum, Uv[texcoordSel].s, Uv[texcoordSel].t);

    // sample texture
    if (order.getEnable(stageOdd))
    {
      // RGBA
      u8 texel[4];

      if (bpmem.genMode.numtexgens > 0)
      {
        TextureSampler::Sample(TexCoord.s, TexCoord.t, TextureLod[stageNum],
                               TextureLinear[stageNum], texmap, texel);
      }
      else
      {
        // It seems like the result is always black when no tex coords are enabled, but further
        // hardware testing is needed.
        std::memset(texel, 0, 4);
      }

      const auto& swap = bpmem.tevksel.GetSwapTable(ac.tswap);
      TexColor.r = texel[u32(swap[ColorChannel::Red])];
      TexColor.g = texel[u32(swap[ColorChannel::Green])];
      TexColor.b = texel[u32(swap[ColorChannel::Blue])];
      TexColor.a = texel[u32(swap[ColorChannel::Alpha])];
    }

    // set konst for this stage
    const auto kc = bpmem.tevksel.GetKonstColor(stageNum);
    const auto ka = bpmem.tevksel.GetKonstAlpha(stageNum);
    StageKonst.r = m_KonstLUT[kc].r;
    StageKonst.g = m_KonstLUT[kc].g;
    StageKonst.b = m_KonstLUT[kc].b;
    StageKonst.a = m_KonstLUT[ka].a;

    // set color
    SetRasColor(order.getColorChan(stageOdd), ac.rswap);

    // combine inputs
    InputRegType inputs[4];
    inputs[BLU_C].a = m_ColorInputLUT[cc.a].b;
    inputs[BLU_C].b = m_ColorInputLUT[cc.b].b;
    inputs[BLU_C].c = m_ColorInputLUT[cc.c].b;
    inputs[BLU_C].d = m_ColorInputLUT[cc.d].b;
    inputs[GRN_C].a = m_ColorInputLUT[cc.a].g;
    inputs[GRN_C].b = m_ColorInputLUT[cc.b].g;
    inputs[GRN_C].c = m_ColorInputLUT[cc.c].g;
    inputs[GRN_C].d = m_ColorInputLUT[cc.d].g;
    inputs[RED_C].a = m_ColorInputLUT[cc.a].r;
    inputs[RED_C].b = m_ColorInputLUT[cc.b].r;
    inputs[RED_C].c = m_ColorInputLUT[cc.c].r;
    inputs[RED_C].d = m_ColorInputLUT[cc.d].r;
    inputs[ALP_C].a = m_AlphaInputLUT[ac.a].a;
    inputs[ALP_C].b = m_AlphaInputLUT[ac.b].a;
    inputs[ALP_C].c = m_AlphaInputLUT[ac.c].a;
    inputs[ALP_C].d = m_AlphaInputLUT[ac.d].a;

    if (cc.bias != TevBias::Compare)
      DrawColorRegular(cc, inputs);
    else
      DrawColorCompare(cc, inputs);

    if (cc.clamp)
    {
      Reg[cc.dest].r = Clamp255(Reg[cc.dest].r);
      Reg[cc.dest].g = Clamp255(Reg[cc.dest].g);
      Reg[cc.dest].b = Clamp255(Reg[cc.dest].b);
    }
    else
    {
      Reg[cc.dest].r = Clamp1024(Reg[cc.dest].r);
      Reg[cc.dest].g = Clamp1024(Reg[cc.dest].g);
      Reg[cc.dest].b = Clamp1024(Reg[cc.dest].b);
    }

    if (ac.bias != TevBias::Compare)
      DrawAlphaRegular(ac, inputs);
    else
      DrawAlphaCompare(ac, inputs);

    if (ac.clamp)
      Reg[ac.dest].a = Clamp255(Reg[ac.dest].a);
    else
      Reg[ac.dest].a = Clamp1024(Reg[ac.dest].a);
  }

  // convert to 8 bits per component
  // the results of the last tev stage are put onto the screen,
  // regardless of the used destination register - TODO: Verify!
  const auto& color_index = bpmem.combiners[bpmem.genMode.numtevstages].colorC.dest;
  const auto& alpha_index = bpmem.combiners[bpmem.genMode.numtevstages].alphaC.dest;
  u8 output[4] = {(u8)Reg[alpha_index].a, (u8)Reg[color_index].b, (u8)Reg[color_index].g,
                  (u8)Reg[color_index].r};

  if (!TevAlphaTest(output[ALP_C]))
    return;

  // z texture
  if (bpmem.ztex2.op != ZTexOp::Disabled)
  {
    u32 ztex = bpmem.ztex1.bias;
    switch (bpmem.ztex2.type)
    {
    case ZTexFormat::U8:
      ztex += TexColor[ALP_C];
      break;
    case ZTexFormat::U16:
      ztex += TexColor[ALP_C] << 8 | TexColor[RED_C];
      break;
    case ZTexFormat::U24:
      ztex += TexColor[RED_C] << 16 | TexColor[GRN_C] << 8 | TexColor[BLU_C];
      break;
    default:
      PanicAlertFmt("Invalid ztex format {}", bpmem.ztex2.type);
    }

    if (bpmem.ztex2.op == ZTexOp::Add)
      ztex += Position[2];

    Position[2] = ztex & 0x00ffffff;
  }

  // fog
  if (bpmem.fog.c_proj_fsel.fsel != FogType::Off)
  {
    float ze;

    if (bpmem.fog.c_proj_fsel.proj == FogProjection::Perspective)
    {
      // perspective
      // ze = A/(B - (Zs >> B_SHF))
      const s32 denom = bpmem.fog.b_magnitude - (Position[2] >> bpmem.fog.b_shift);
      // in addition downscale magnitude and zs to 0.24 bits
      ze = (bpmem.fog.GetA() * 16777215.0f) / static_cast<float>(denom);
    }
    else
    {
      // orthographic
      // ze = a*Zs
      // in addition downscale zs to 0.24 bits
      ze = bpmem.fog.GetA() * (static_cast<float>(Position[2]) / 16777215.0f);
    }

    if (bpmem.fogRange.Base.Enabled)
    {
      // TODO: This is untested and should definitely be checked against real hw.
      // - No idea if offset is really normalized against the viewport width or against the
      // projection matrix or yet something else
      // - scaling of the "k" coefficient isn't clear either.

      // First, calculate the offset from the viewport center (normalized to 0..1)
      const float offset =
          (Position[0] - (static_cast<s32>(bpmem.fogRange.Base.Center.Value()) - 342)) /
          static_cast<float>(xfmem.viewport.wd);

      // Based on that, choose the index such that points which are far away from the z-axis use the
      // 10th "k" value and such that central points use the first value.
      float floatindex = 9.f - std::abs(offset) * 9.f;
      floatindex = std::clamp(floatindex, 0.f, 9.f);  // TODO: This shouldn't be necessary!

      // Get the two closest integer indices, look up the corresponding samples
      const int indexlower = (int)floatindex;
      const int indexupper = indexlower + 1;
      // Look up coefficient... Seems like multiplying by 4 makes Fortune Street work properly (fog
      // is too strong without the factor)
      const float klower = bpmem.fogRange.K[indexlower / 2].GetValue(indexlower % 2) * 4.f;
      const float kupper = bpmem.fogRange.K[indexupper / 2].GetValue(indexupper % 2) * 4.f;

      // linearly interpolate the samples and multiple ze by the resulting adjustment factor
      const float factor = indexupper - floatindex;
      const float k = klower * factor + kupper * (1.f - factor);
      const float x_adjust = sqrt(offset * offset + k * k) / k;
      ze *= x_adjust;  // NOTE: This is basically dividing by a cosine (hidden behind
                       // GXInitFogAdjTable): 1/cos = c/b = sqrt(a^2+b^2)/b
    }

    ze -= bpmem.fog.GetC();

    // clamp 0 to 1
    float fog = std::clamp(ze, 0.f, 1.f);

    switch (bpmem.fog.c_proj_fsel.fsel)
    {
    case FogType::Exp:
      fog = 1.0f - pow(2.0f, -8.0f * fog);
      break;
    case FogType::ExpSq:
      fog = 1.0f - pow(2.0f, -8.0f * fog * fog);
      break;
    case FogType::BackwardsExp:
      fog = 1.0f - fog;
      fog = pow(2.0f, -8.0f * fog);
      break;
    case FogType::BackwardsExpSq:
      fog = 1.0f - fog;
      fog = pow(2.0f, -8.0f * fog * fog);
      break;
    default:
      break;
    }

    // lerp from output to fog color
    const u32 fogInt = (u32)(fog * 256);
    const u32 invFog = 256 - fogInt;

    output[RED_C] = (output[RED_C] * invFog + fogInt * bpmem.fog.color.r) >> 8;
    output[GRN_C] = (output[GRN_C] * invFog + fogInt * bpmem.fog.color.g) >> 8;
    output[BLU_C] = (output[BLU_C] * invFog + fogInt * bpmem.fog.color.b) >> 8;
  }

  if (bpmem.GetEmulatedZ() == EmulatedZ::Late)
  {
    // TODO: Check against hw if these values get incremented even if depth testing is disabled
    EfbInterface::IncPerfCounterQuadCount(PQ_ZCOMP_INPUT);

    if (!EfbInterface::ZCompare(Position[0], Position[1], Position[2]))
      return;

    EfbInterface::IncPerfCounterQuadCount(PQ_ZCOMP_OUTPUT);
  }

  // The GC/Wii GPU rasterizes in 2x2 pixel groups, so bounding box values will be rounded to the
  // extents of these groups, rather than the exact pixel.
  BBoxManager::Update(static_cast<u16>(Position[0] & ~1), static_cast<u16>(Position[0] | 1),
                      static_cast<u16>(Position[1] & ~1), static_cast<u16>(Position[1] | 1));

  INCSTAT(g_stats.this_frame.tev_pixels_out);
  EfbInterface::IncPerfCounterQuadCount(PQ_BLEND_INPUT);

  EfbInterface::BlendTev(Position[0], Position[1], output);
}

void Tev::SetKonstColors()
{
  auto& system = Core::System::GetInstance();
  auto& pixel_shader_manager = system.GetPixelShaderManager();

  for (int i = 0; i < 4; i++)
  {
    KonstantColors[i].r = pixel_shader_manager.constants.kcolors[i][0];
    KonstantColors[i].g = pixel_shader_manager.constants.kcolors[i][1];
    KonstantColors[i].b = pixel_shader_manager.constants.kcolors[i][2];
    KonstantColors[i].a = pixel_shader_manager.constants.kcolors[i][3];
  }
}
