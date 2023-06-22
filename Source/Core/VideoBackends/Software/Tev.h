// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/EnumMap.h"
#include "VideoCommon/BPMemory.h"

class Tev
{
  struct TevColor
  {
    constexpr TevColor() = default;
    constexpr explicit TevColor(s16 a_, s16 b_, s16 g_, s16 r_) : a(a_), b(b_), g(g_), r(r_) {}

    s16 a = 0;
    s16 b = 0;
    s16 g = 0;
    s16 r = 0;

    constexpr static TevColor All(s16 value) { return TevColor(value, value, value, value); }

    constexpr s16& operator[](int index)
    {
      switch (index)
      {
      case ALP_C:
        return a;
      case BLU_C:
        return b;
      case GRN_C:
        return g;
      case RED_C:
        return r;
      default:
        // invalid
        return a;
      }
    }
  };

  struct TevColorRef
  {
    constexpr explicit TevColorRef(const s16& r_, const s16& g_, const s16& b_)
        : r(r_), g(g_), b(b_)
    {
    }

    const s16& r;
    const s16& g;
    const s16& b;

    constexpr static TevColorRef Color(const TevColor& color)
    {
      return TevColorRef(color.r, color.g, color.b);
    }
    constexpr static TevColorRef All(const s16& value) { return TevColorRef(value, value, value); }
    constexpr static TevColorRef Alpha(const TevColor& color) { return All(color.a); }
  };

  struct TevAlphaRef
  {
    constexpr explicit TevAlphaRef(const TevColor& color) : a(color.a) {}
    constexpr explicit TevAlphaRef(const s16& a_) : a(a_) {}

    const s16& a;
  };

  struct TevKonstRef
  {
    constexpr explicit TevKonstRef(const s16& a_, const s16& r_, const s16& g_, const s16& b_)
        : a(a_), r(r_), g(g_), b(b_)
    {
    }

    const s16& a;
    const s16& r;
    const s16& g;
    const s16& b;

    constexpr static TevKonstRef Value(const s16& value)
    {
      return TevKonstRef(value, value, value, value);
    }
    constexpr static TevKonstRef Konst(const s16& alpha, const TevColor& color)
    {
      return TevKonstRef(alpha, color.r, color.g, color.b);
    }
  };

  struct InputRegType
  {
    unsigned a : 8;
    unsigned b : 8;
    unsigned c : 8;
    signed d : 11;
  };

  struct TextureCoordinateType
  {
    signed s : 24;
    signed t : 24;
  };

  // color order: ABGR
  Common::EnumMap<TevColor, TevOutput::Color2> Reg;
  std::array<TevColor, 4> KonstantColors;
  TevColor TexColor;
  TevColor RasColor;
  TevColor StageKonst;

  // Fixed constants, corresponding to KonstSel
  static constexpr s16 V0 = 0;
  static constexpr s16 V1_8 = 32;
  static constexpr s16 V1_4 = 64;
  static constexpr s16 V3_8 = 96;
  static constexpr s16 V1_2 = 128;
  static constexpr s16 V5_8 = 159;
  static constexpr s16 V3_4 = 191;
  static constexpr s16 V7_8 = 223;
  static constexpr s16 V1 = 255;

  u8 AlphaBump = 0;
  u8 IndirectTex[4][4]{};
  TextureCoordinateType TexCoord{};

  const Common::EnumMap<TevColorRef, TevColorArg::Zero> m_ColorInputLUT{
      TevColorRef::Color(Reg[TevOutput::Prev]),    // prev.rgb
      TevColorRef::Alpha(Reg[TevOutput::Prev]),    // prev.aaa
      TevColorRef::Color(Reg[TevOutput::Color0]),  // c0.rgb
      TevColorRef::Alpha(Reg[TevOutput::Color0]),  // c0.aaa
      TevColorRef::Color(Reg[TevOutput::Color1]),  // c1.rgb
      TevColorRef::Alpha(Reg[TevOutput::Color1]),  // c1.aaa
      TevColorRef::Color(Reg[TevOutput::Color2]),  // c2.rgb
      TevColorRef::Alpha(Reg[TevOutput::Color2]),  // c2.aaa
      TevColorRef::Color(TexColor),                // tex.rgb
      TevColorRef::Alpha(TexColor),                // tex.aaa
      TevColorRef::Color(RasColor),                // ras.rgb
      TevColorRef::Alpha(RasColor),                // ras.aaa
      TevColorRef::All(V1),                        // one
      TevColorRef::All(V1_2),                      // half
      TevColorRef::Color(StageKonst),              // konst
      TevColorRef::All(V0),                        // zero
  };
  const Common::EnumMap<TevAlphaRef, TevAlphaArg::Zero> m_AlphaInputLUT{
      TevAlphaRef(Reg[TevOutput::Prev]),    // prev
      TevAlphaRef(Reg[TevOutput::Color0]),  // c0
      TevAlphaRef(Reg[TevOutput::Color1]),  // c1
      TevAlphaRef(Reg[TevOutput::Color2]),  // c2
      TevAlphaRef(TexColor),                // tex
      TevAlphaRef(RasColor),                // ras
      TevAlphaRef(StageKonst),              // konst
      TevAlphaRef(V0),                      // zero
  };
  const Common::EnumMap<TevKonstRef, KonstSel::K3_A> m_KonstLUT{
      TevKonstRef::Value(V1),    // 1
      TevKonstRef::Value(V7_8),  // 7/8
      TevKonstRef::Value(V3_4),  // 3/4
      TevKonstRef::Value(V5_8),  // 5/8
      TevKonstRef::Value(V1_2),  // 1/2
      TevKonstRef::Value(V3_8),  // 3/8
      TevKonstRef::Value(V1_4),  // 1/4
      TevKonstRef::Value(V1_8),  // 1/8

      // These are "invalid" values, not meant to be used. On hardware,
      // they all output zero.
      TevKonstRef::Value(V0), TevKonstRef::Value(V0), TevKonstRef::Value(V0),
      TevKonstRef::Value(V0),

      // These values are valid for RGB only; they're invalid for alpha
      TevKonstRef::Konst(V0, KonstantColors[0]),  // Konst 0 RGB
      TevKonstRef::Konst(V0, KonstantColors[1]),  // Konst 1 RGB
      TevKonstRef::Konst(V0, KonstantColors[2]),  // Konst 2 RGB
      TevKonstRef::Konst(V0, KonstantColors[3]),  // Konst 3 RGB

      TevKonstRef::Value(KonstantColors[0].r),  // Konst 0 Red
      TevKonstRef::Value(KonstantColors[1].r),  // Konst 1 Red
      TevKonstRef::Value(KonstantColors[2].r),  // Konst 2 Red
      TevKonstRef::Value(KonstantColors[3].r),  // Konst 3 Red
      TevKonstRef::Value(KonstantColors[0].g),  // Konst 0 Green
      TevKonstRef::Value(KonstantColors[1].g),  // Konst 1 Green
      TevKonstRef::Value(KonstantColors[2].g),  // Konst 2 Green
      TevKonstRef::Value(KonstantColors[3].g),  // Konst 3 Green
      TevKonstRef::Value(KonstantColors[0].b),  // Konst 0 Blue
      TevKonstRef::Value(KonstantColors[1].b),  // Konst 1 Blue
      TevKonstRef::Value(KonstantColors[2].b),  // Konst 2 Blue
      TevKonstRef::Value(KonstantColors[3].b),  // Konst 3 Blue
      TevKonstRef::Value(KonstantColors[0].a),  // Konst 0 Alpha
      TevKonstRef::Value(KonstantColors[1].a),  // Konst 1 Alpha
      TevKonstRef::Value(KonstantColors[2].a),  // Konst 2 Alpha
      TevKonstRef::Value(KonstantColors[3].a),  // Konst 3 Alpha
  };
  static constexpr Common::EnumMap<s16, TevBias::Compare> s_BiasLUT{0, 128, -128, 0};
  static constexpr Common::EnumMap<u8, TevScale::Divide2> s_ScaleLShiftLUT{0, 1, 2, 0};
  static constexpr Common::EnumMap<u8, TevScale::Divide2> s_ScaleRShiftLUT{0, 0, 0, 1};

  enum BufferBase
  {
    DIRECT = 0,
    DIRECT_TFETCH = 16,
    INDIRECT = 32
  };

  void SetRasColor(RasColorChan colorChan, u32 swaptable);

  void DrawColorRegular(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4]);
  void DrawColorCompare(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4]);
  void DrawAlphaRegular(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4]);
  void DrawAlphaCompare(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4]);

  void Indirect(unsigned int stageNum, s32 s, s32 t);

public:
  s32 Position[3]{};
  u8 Color[2][4]{};  // must be RGBA for correct swap table ordering
  TextureCoordinateType Uv[8]{};
  s32 IndirectLod[4]{};
  bool IndirectLinear[4]{};
  s32 TextureLod[16]{};
  bool TextureLinear[16]{};

  enum
  {
    ALP_C,
    BLU_C,
    GRN_C,
    RED_C
  };

  void SetKonstColors();
  void Draw();
};
