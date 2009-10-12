// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _TEV_H_
#define _TEV_H_

#include "BPMemLoader.h"

class Tev
{
    // color order: RGBA
    s16 Reg[4][4];    
    s16 KonstantColors[4][4];
    s16 FixedConstants[9];
    s16 TexColor[4];
    s16 RasColor[4];
    s16 StageKonst[4];
    s16 Zero16[4];
    u8 AlphaBump;
    u8 IndirectTex[4][4];
    float TexCoord[2];

    s16 *m_ColorInputLUT[16][3];
    s16 *m_AlphaInputLUT[8];        // values must point to RGBA color
    s16 *m_KonstLUT[32][4];
    u8 *m_RasColorLUT[8];
    s16 m_BiasLUT[4];
    u8 m_ScaleLShiftLUT[4];
    u8 m_ScaleRShiftLUT[4];

    void SetRasColor(int colorChan, int swaptable);

    void DrawColorRegular(TevStageCombiner::ColorCombiner &cc);
    void DrawColorCompare(TevStageCombiner::ColorCombiner &cc);
    void DrawAlphaRegular(TevStageCombiner::AlphaCombiner &ac);
    void DrawAlphaCompare(TevStageCombiner::AlphaCombiner &ac);

    void Indirect(unsigned int stageNum, float s, float t);    

public:
    s32 Position[3];
    u8 Color[2][4];
    float Uv[8][2];
    float Lod[8];

    void Init();

    void Draw();

    void SetRegColor(int reg, int comp, bool konst, s16 color);

    enum { RED_C, GRN_C, BLU_C, ALP_C };
};

#endif