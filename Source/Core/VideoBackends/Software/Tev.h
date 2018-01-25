// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/BPMemory.h"

class Tev
{
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
  s16 Reg[4][4];
  s16 KonstantColors[4][4];
  s16 TexColor[4];
  s16 RasColor[4];
  s16 StageKonst[4];
  s16 Zero16[4];

  s16 FixedConstants[9];
  u8 AlphaBump;
  u8 IndirectTex[4][4];
  TextureCoordinateType TexCoord;

  s16* m_ColorInputLUT[16][3];
  s16* m_AlphaInputLUT[8];  // values must point to ABGR color
  s16* m_KonstLUT[32][4];
  s16 m_BiasLUT[4];
  u8 m_ScaleLShiftLUT[4];
  u8 m_ScaleRShiftLUT[4];

  // enumeration for color input LUT
  enum
  {
    BLU_INP,
    GRN_INP,
    RED_INP
  };

  enum BufferBase
  {
    DIRECT = 0,
    DIRECT_TFETCH = 16,
    INDIRECT = 32
  };

  void SetRasColor(int colorChan, int swaptable);

  void DrawColorRegular(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4]);
  void DrawColorCompare(const TevStageCombiner::ColorCombiner& cc, const InputRegType inputs[4]);
  void DrawAlphaRegular(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4]);
  void DrawAlphaCompare(const TevStageCombiner::AlphaCombiner& ac, const InputRegType inputs[4]);

  void Indirect(unsigned int stageNum, s32 s, s32 t);

public:
  s32 Position[3];
  u8 Color[2][4];  // must be RGBA for correct swap table ordering
  TextureCoordinateType Uv[8];
  s32 IndirectLod[4];
  bool IndirectLinear[4];
  s32 TextureLod[16];
  bool TextureLinear[16];

  enum
  {
    ALP_C,
    BLU_C,
    GRN_C,
    RED_C
  };

  void Init();

  void Draw();

  void SetRegColor(int reg, int comp, s16 color);
};
