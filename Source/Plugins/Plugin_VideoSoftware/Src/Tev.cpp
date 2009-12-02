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

#include "Common.h"

#include "Tev.h"
#include "EfbInterface.h"
#include "TextureSampler.h"
#include "Statistics.h"
#include "VideoConfig.h"
#include "DebugUtil.h"

#include <math.h>

void Tev::Init()
{
    FixedConstants[0] = 0;
    FixedConstants[1] = 31;
    FixedConstants[2] = 63;
    FixedConstants[3] = 95;
    FixedConstants[4] = 127;
    FixedConstants[5] = 159;
    FixedConstants[6] = 191;
    FixedConstants[7] = 223;
    FixedConstants[8] = 255;

    for (int i = 0; i < 4; i++)
        Zero16[i] = 0;

    m_ColorInputLUT[0][0] = &Reg[0][RED_C]; m_ColorInputLUT[0][1] = &Reg[0][GRN_C]; m_ColorInputLUT[0][2] = &Reg[0][BLU_C]; // prev.rgb
    m_ColorInputLUT[1][0] = &Reg[0][ALP_C]; m_ColorInputLUT[1][1] = &Reg[0][ALP_C]; m_ColorInputLUT[1][2] = &Reg[0][ALP_C]; // prev.aaa
    m_ColorInputLUT[2][0] = &Reg[1][RED_C]; m_ColorInputLUT[2][1] = &Reg[1][GRN_C]; m_ColorInputLUT[2][2] = &Reg[1][BLU_C]; // c0.rgb
    m_ColorInputLUT[3][0] = &Reg[1][ALP_C]; m_ColorInputLUT[3][1] = &Reg[1][ALP_C]; m_ColorInputLUT[3][2] = &Reg[1][ALP_C]; // c0.aaa
    m_ColorInputLUT[4][0] = &Reg[2][RED_C]; m_ColorInputLUT[4][1] = &Reg[2][GRN_C]; m_ColorInputLUT[4][2] = &Reg[2][BLU_C]; // c1.rgb
    m_ColorInputLUT[5][0] = &Reg[2][ALP_C]; m_ColorInputLUT[5][1] = &Reg[2][ALP_C]; m_ColorInputLUT[5][2] = &Reg[2][ALP_C]; // c1.aaa
    m_ColorInputLUT[6][0] = &Reg[3][RED_C]; m_ColorInputLUT[6][1] = &Reg[3][GRN_C]; m_ColorInputLUT[6][2] = &Reg[3][BLU_C]; // c2.rgb
    m_ColorInputLUT[7][0] = &Reg[3][ALP_C]; m_ColorInputLUT[7][1] = &Reg[3][ALP_C]; m_ColorInputLUT[7][2] = &Reg[3][ALP_C]; // c2.aaa
    m_ColorInputLUT[8][0] = &TexColor[RED_C]; m_ColorInputLUT[8][1] = &TexColor[GRN_C]; m_ColorInputLUT[8][2] = &TexColor[BLU_C]; // tex.rgb
    m_ColorInputLUT[9][0] = &TexColor[ALP_C]; m_ColorInputLUT[9][1] = &TexColor[ALP_C]; m_ColorInputLUT[9][2] = &TexColor[ALP_C]; // tex.aaa
    m_ColorInputLUT[10][0] = &RasColor[RED_C]; m_ColorInputLUT[10][1] = &RasColor[GRN_C]; m_ColorInputLUT[10][2] = &RasColor[BLU_C]; // ras.rgb
    m_ColorInputLUT[11][0] = &RasColor[ALP_C]; m_ColorInputLUT[11][1] = &RasColor[ALP_C]; m_ColorInputLUT[11][2] = &RasColor[ALP_C]; // ras.rgb
    m_ColorInputLUT[12][0] = &FixedConstants[8]; m_ColorInputLUT[12][1] = &FixedConstants[8]; m_ColorInputLUT[12][2] = &FixedConstants[8]; // one
    m_ColorInputLUT[13][0] = &FixedConstants[4]; m_ColorInputLUT[13][1] = &FixedConstants[4]; m_ColorInputLUT[13][2] = &FixedConstants[4]; // half
    m_ColorInputLUT[14][0] = &StageKonst[0]; m_ColorInputLUT[14][1] = &StageKonst[1]; m_ColorInputLUT[14][2] = &StageKonst[2]; // konst
    m_ColorInputLUT[15][0] = &FixedConstants[0]; m_ColorInputLUT[15][1] = &FixedConstants[0]; m_ColorInputLUT[15][2] = &FixedConstants[0]; // zero

    m_AlphaInputLUT[0] = &Reg[0][ALP_C]; // prev.a
    m_AlphaInputLUT[1] = &Reg[1][ALP_C]; // c0.a
    m_AlphaInputLUT[2] = &Reg[2][ALP_C]; // c1.a
    m_AlphaInputLUT[3] = &Reg[3][ALP_C]; // c2.a
    m_AlphaInputLUT[4] = &TexColor[ALP_C]; // tex.a
    m_AlphaInputLUT[5] = &RasColor[ALP_C]; // ras.a
    m_AlphaInputLUT[6] = &StageKonst[ALP_C]; // konst.a
    m_AlphaInputLUT[7] = &Zero16[ALP_C]; // zero

    for (int comp = 0; comp < 4; comp++)
    {
        m_KonstLUT[0][comp] = &FixedConstants[8];
        m_KonstLUT[1][comp] = &FixedConstants[7];
        m_KonstLUT[2][comp] = &FixedConstants[6];
        m_KonstLUT[3][comp] = &FixedConstants[5];
        m_KonstLUT[4][comp] = &FixedConstants[4];
        m_KonstLUT[5][comp] = &FixedConstants[3];
        m_KonstLUT[6][comp] = &FixedConstants[2];
        m_KonstLUT[7][comp] = &FixedConstants[1];

        m_KonstLUT[12][comp] = &KonstantColors[0][comp];
        m_KonstLUT[13][comp] = &KonstantColors[1][comp];
        m_KonstLUT[14][comp] = &KonstantColors[2][comp];
        m_KonstLUT[15][comp] = &KonstantColors[3][comp];

        m_KonstLUT[16][comp] = &KonstantColors[0][RED_C];
        m_KonstLUT[17][comp] = &KonstantColors[1][RED_C];
        m_KonstLUT[18][comp] = &KonstantColors[2][RED_C];
        m_KonstLUT[19][comp] = &KonstantColors[3][RED_C];
        m_KonstLUT[20][comp] = &KonstantColors[0][GRN_C];
        m_KonstLUT[21][comp] = &KonstantColors[1][GRN_C];
        m_KonstLUT[22][comp] = &KonstantColors[2][GRN_C];
        m_KonstLUT[23][comp] = &KonstantColors[3][GRN_C];
        m_KonstLUT[24][comp] = &KonstantColors[0][BLU_C];
        m_KonstLUT[25][comp] = &KonstantColors[1][BLU_C];
        m_KonstLUT[26][comp] = &KonstantColors[2][BLU_C];
        m_KonstLUT[27][comp] = &KonstantColors[3][BLU_C];
        m_KonstLUT[28][comp] = &KonstantColors[0][ALP_C];
        m_KonstLUT[29][comp] = &KonstantColors[1][ALP_C];
        m_KonstLUT[30][comp] = &KonstantColors[2][ALP_C];
        m_KonstLUT[31][comp] = &KonstantColors[3][ALP_C];
    }

    m_BiasLUT[0] = 0;
    m_BiasLUT[1] = 128;
    m_BiasLUT[2] = -128;
    m_BiasLUT[3] = 0;

    m_ScaleLShiftLUT[0] = 0;
    m_ScaleLShiftLUT[1] = 1;
    m_ScaleLShiftLUT[2] = 2;
    m_ScaleLShiftLUT[3] = 0;

    m_ScaleRShiftLUT[0] = 0;
    m_ScaleRShiftLUT[1] = 0;
    m_ScaleRShiftLUT[2] = 0;
    m_ScaleRShiftLUT[3] = 1;
}

inline s16 Clamp255(s16 in)
{
    return in>255?255:(in<0?0:in);
}

inline s16 Clamp1024(s16 in)
{
     return in>1023?1023:(in<-1024?-1024:in);
}

inline void Tev::SetRasColor(int colorChan, int swaptable)
{
    switch(colorChan)
    {
    case 0: // Color0
        {
            u8 *color = Color[0];
            RasColor[0] = color[bpmem.tevksel[swaptable].swap1];
            RasColor[1] = color[bpmem.tevksel[swaptable].swap2];
            swaptable++;
            RasColor[2] = color[bpmem.tevksel[swaptable].swap1];
            RasColor[3] = color[bpmem.tevksel[swaptable].swap2];
        }
        break;
    case 1: // Color1
        {
            u8 *color = Color[1];
            RasColor[0] = color[bpmem.tevksel[swaptable].swap1];
            RasColor[1] = color[bpmem.tevksel[swaptable].swap2];
            swaptable++;
            RasColor[2] = color[bpmem.tevksel[swaptable].swap1];
            RasColor[3] = color[bpmem.tevksel[swaptable].swap2];
        }
        break;
     case 5: // alpha bump
        {
            for(int i = 0; i < 4; i++)
                RasColor[i] = AlphaBump;
        }
        break;
    case 6: // alpha bump normalized
        {
            u8 normalized = AlphaBump | AlphaBump >> 5;
            for(int i = 0; i < 4; i++)
                RasColor[i] = normalized;
        }
        break;    
    default: // zero
        {
            for(int i = 0; i < 4; i++)
                RasColor[i] = 0;
        }
        break;
    }
}

void Tev::DrawColorRegular(TevStageCombiner::ColorCombiner &cc)
{
    InputRegType InputReg;

    for (int i = 0; i < 3; i++)
    {
        InputReg.a = *m_ColorInputLUT[cc.a][i];
        InputReg.b = *m_ColorInputLUT[cc.b][i];
        InputReg.c = *m_ColorInputLUT[cc.c][i];
        InputReg.d = *m_ColorInputLUT[cc.d][i];

        u16 c = InputReg.c + (InputReg.c >> 7); 

        s32 temp = InputReg.a * (256 - c) + (InputReg.b * c);
        temp = cc.op?(-temp >> 8):(temp >> 8);

        s32 result = InputReg.d + temp + m_BiasLUT[cc.bias];
        result = result << m_ScaleLShiftLUT[cc.shift];
        result = result >> m_ScaleRShiftLUT[cc.shift];        

        Reg[cc.dest][RED_C + i] = result;
    }
}

void Tev::DrawColorCompare(TevStageCombiner::ColorCombiner &cc)
{
    int cmp = (cc.shift<<1)|cc.op|8; // comparemode stored here

    u32 a;
    u32 b;

    InputRegType InputReg;

    switch(cmp) {
    case TEVCMP_R8_GT:        
        {
            a = *m_ColorInputLUT[cc.a][RED_C] & 0xff;
            b = *m_ColorInputLUT[cc.b][RED_C] & 0xff;
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
            }
        }
        break;

    case TEVCMP_R8_EQ:
        {
            a = *m_ColorInputLUT[cc.a][RED_C] & 0xff;
            b = *m_ColorInputLUT[cc.b][RED_C] & 0xff;
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
            }
        }
        break;
    case TEVCMP_GR16_GT:
        {
            a = ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_C] & 0xff);
            b = ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_C] & 0xff);
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
            }
        }
        break;
    case TEVCMP_GR16_EQ:
        {
            a = ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_C] & 0xff);
            b = ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_C] & 0xff);
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
            }
        }
        break;
    case TEVCMP_BGR24_GT:
        {
            a = ((*m_ColorInputLUT[cc.a][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_C] & 0xff);
            b = ((*m_ColorInputLUT[cc.b][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_C] & 0xff);
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
            }
        }
        break;
    case TEVCMP_BGR24_EQ:
        {
            a = ((*m_ColorInputLUT[cc.a][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_C] & 0xff);
            b = ((*m_ColorInputLUT[cc.b][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_C] & 0xff);
            for (int i = 0; i < 3; i++)
            {
                InputReg.c = *m_ColorInputLUT[cc.c][i];
                InputReg.d = *m_ColorInputLUT[cc.d][i];
                Reg[cc.dest][RED_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
            }
        }
        break;
    case TEVCMP_RGB8_GT:
        for (int i = 0; i < 3; i++)
        {
            InputReg.a = *m_ColorInputLUT[cc.a][i];
            InputReg.b = *m_ColorInputLUT[cc.b][i];
            InputReg.c = *m_ColorInputLUT[cc.c][i];
            InputReg.d = *m_ColorInputLUT[cc.d][i];
            Reg[cc.dest][RED_C + i] = InputReg.d + ((InputReg.a > InputReg.b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_RGB8_EQ: 
        for (int i = 0; i < 3; i++)
        {
            InputReg.a = *m_ColorInputLUT[cc.a][i];
            InputReg.b = *m_ColorInputLUT[cc.b][i];
            InputReg.c = *m_ColorInputLUT[cc.c][i];
            InputReg.d = *m_ColorInputLUT[cc.d][i];
            Reg[cc.dest][RED_C + i] = InputReg.d + ((InputReg.a == InputReg.b) ? InputReg.c : 0);
        }
        break;
    }          
}

void Tev::DrawAlphaRegular(TevStageCombiner::AlphaCombiner &ac)
{
    InputRegType InputReg;

    InputReg.a = *m_AlphaInputLUT[ac.a];
    InputReg.b = *m_AlphaInputLUT[ac.b];
    InputReg.c = *m_AlphaInputLUT[ac.c];
    InputReg.d = *m_AlphaInputLUT[ac.d];

    u16 c = InputReg.c + (InputReg.c >> 7); 

    s32 temp = InputReg.a * (256 - c) + (InputReg.b * c);
    temp = ac.op?(-temp >> 8):(temp >> 8);

    s32 result = InputReg.d + temp + m_BiasLUT[ac.bias];
    result = result << m_ScaleLShiftLUT[ac.shift];
    result = result >> m_ScaleRShiftLUT[ac.shift];

    Reg[ac.dest][ALP_C] = result;
}

void Tev::DrawAlphaCompare(TevStageCombiner::AlphaCombiner &ac)
{
    int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here

    u32 a;
    u32 b;
    
    InputRegType InputReg;

    switch(cmp) {
    case TEVCMP_R8_GT:        
        {
            a = m_AlphaInputLUT[ac.a][RED_C] & 0xff;
            b = m_AlphaInputLUT[ac.b][RED_C] & 0xff;
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a > b) ? InputReg.c : 0);
        }
        break;

    case TEVCMP_R8_EQ:
        {
            a = m_AlphaInputLUT[ac.a][RED_C] & 0xff;
            b = m_AlphaInputLUT[ac.b][RED_C] & 0xff;
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a == b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_GR16_GT:
        {
            a = ((m_AlphaInputLUT[ac.a][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.a][RED_C] & 0xff);
            b = ((m_AlphaInputLUT[ac.b][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.b][RED_C] & 0xff);
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a > b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_GR16_EQ:
        {
            a = ((m_AlphaInputLUT[ac.a][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.a][RED_C] & 0xff);
            b = ((m_AlphaInputLUT[ac.b][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.b][RED_C] & 0xff);
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a == b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_BGR24_GT:
        {
            a = ((m_AlphaInputLUT[ac.a][BLU_C] & 0xff) << 16) | ((m_AlphaInputLUT[ac.a][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.a][RED_C] & 0xff);
            b = ((m_AlphaInputLUT[ac.b][BLU_C] & 0xff) << 16) | ((m_AlphaInputLUT[ac.b][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.b][RED_C] & 0xff);
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a > b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_BGR24_EQ:
        {
            a = ((m_AlphaInputLUT[ac.a][BLU_C] & 0xff) << 16) | ((m_AlphaInputLUT[ac.a][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.a][RED_C] & 0xff);
            b = ((m_AlphaInputLUT[ac.b][BLU_C] & 0xff) << 16) | ((m_AlphaInputLUT[ac.b][GRN_C] & 0xff) << 8) | (m_AlphaInputLUT[ac.b][RED_C] & 0xff);
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((a == b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_A8_GT:
        {
            InputReg.a = m_AlphaInputLUT[ac.a][ALP_C];
            InputReg.b = m_AlphaInputLUT[ac.b][ALP_C];
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((InputReg.a > InputReg.b) ? InputReg.c : 0);
        }
        break;
    case TEVCMP_A8_EQ:
        {
            InputReg.a = m_AlphaInputLUT[ac.a][ALP_C];
            InputReg.b = m_AlphaInputLUT[ac.b][ALP_C];
            InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
            InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];
            Reg[ac.dest][ALP_C] = InputReg.d + ((InputReg.a == InputReg.b) ? InputReg.c : 0);
        }
        break;
    }
}

static bool AlphaCompare(int alpha, int ref, int comp)
{
    switch(comp) {
    case ALPHACMP_ALWAYS:  return true;
    case ALPHACMP_NEVER:   return false;
    case ALPHACMP_LEQUAL:  return alpha <= ref;
    case ALPHACMP_LESS:    return alpha < ref;
    case ALPHACMP_GEQUAL:  return alpha >= ref;
    case ALPHACMP_GREATER: return alpha > ref;
    case ALPHACMP_EQUAL:   return alpha == ref;
    case ALPHACMP_NEQUAL:  return alpha != ref;
    }
    return true;
}

static bool AlphaTest(int alpha)
{
    bool comp0 = AlphaCompare(alpha, bpmem.alphaFunc.ref0, bpmem.alphaFunc.comp0);
    bool comp1 = AlphaCompare(alpha, bpmem.alphaFunc.ref1, bpmem.alphaFunc.comp1);

    switch (bpmem.alphaFunc.logic) {
    case 0: return comp0 && comp1; // and
    case 1: return comp0 || comp1; // or
    case 2: return comp0 ^ comp1; // xor
    case 3: return !(comp0 ^ comp1); // xnor
    }
    return true;
}

inline float WrapIndirectCoord(float coord, int wrapMode)
{
    switch (wrapMode) {
        case ITW_OFF:
            return coord;
        case ITW_256:
            return fmod(coord, 256);
         case ITW_128:
            return fmod(coord, 128);
        case ITW_64:
            return fmod(coord, 64);
        case ITW_32:
            return fmod(coord, 32);
        case ITW_16:
            return fmod(coord, 16);
        case ITW_0:
            return 0;
    }
    return 0;
}

void Tev::Indirect(unsigned int stageNum, float s, float t)
{
    TevStageIndirect &indirect = bpmem.tevind[stageNum];
    u8 *indmap = IndirectTex[indirect.bt];
    

    float indcoord[3];

    // alpha bump select
    switch (indirect.bs) {
        case ITBA_OFF:
            AlphaBump = 0;
            break;
         case ITBA_S:
            AlphaBump = indmap[ALP_C];
            break;
        case ITBA_T:
            AlphaBump = indmap[BLU_C];
            break;
        case ITBA_U:
            AlphaBump = indmap[GRN_C];
            break;
    }

    // bias select
    s16 biasValue = indirect.fmt==ITF_8?-128:1;
    s16 bias[3];
    bias[0] = indirect.bias&1?biasValue:0;
    bias[1] = indirect.bias&2?biasValue:0;
    bias[2] = indirect.bias&4?biasValue:0;

    // format
    switch(indirect.fmt) {
        case ITF_8:
            indcoord[0] = (float)indmap[ALP_C] + bias[0];
            indcoord[1] = (float)indmap[BLU_C] + bias[1];
            indcoord[2] = (float)indmap[GRN_C] + bias[2];
            AlphaBump = AlphaBump & 0xf8;
            break;
        case ITF_5:
            indcoord[0] = (float)(indmap[ALP_C] & 0x1f) + bias[0];
            indcoord[1] = (float)(indmap[BLU_C] & 0x1f) + bias[1];
            indcoord[2] = (float)(indmap[GRN_C] & 0x1f) + bias[2];
            AlphaBump = AlphaBump & 0xe0;
            break;
        case ITF_4:
            indcoord[0] = (float)(indmap[ALP_C] & 0x0f) + bias[0];
            indcoord[1] = (float)(indmap[BLU_C] & 0x0f) + bias[1];
            indcoord[2] = (float)(indmap[GRN_C] & 0x0f) + bias[2];
            AlphaBump = AlphaBump & 0xf0;
            break;
        case ITF_3:
            indcoord[0] = (float)(indmap[ALP_C] & 0x07) + bias[0];
            indcoord[1] = (float)(indmap[BLU_C] & 0x07) + bias[1];
            indcoord[2] = (float)(indmap[GRN_C] & 0x07) + bias[2];
            AlphaBump = AlphaBump & 0xf8;
            break;
    }

    float indtevtrans[2] = { 0,0 };

    // matrix multiply
    int indmtxid = indirect.mid & 3;
    if (indmtxid)
    {
        IND_MTX &indmtx = bpmem.indmtx[indmtxid - 1];
        int scale = ((u32)indmtx.col0.s0 << 0) |
	                ((u32)indmtx.col1.s1 << 2) |
	                ((u32)indmtx.col2.s2 << 4);
        float fscale = 0.0f;

        switch (indirect.mid & 12) {
            case 0:
                fscale = powf(2.0f, (float)(scale - 17)) / 1024.0f;
                indtevtrans[0] = indmtx.col0.ma * indcoord[0] + indmtx.col1.mc * indcoord[1] + indmtx.col2.me * indcoord[2];
                indtevtrans[1] = indmtx.col0.mb * indcoord[0] + indmtx.col1.md * indcoord[1] + indmtx.col2.mf * indcoord[2];
                break;
            case 4: // s matrix
                fscale = powf(2.0f, (float)(scale - 17)) / 256;
                indtevtrans[0] = s * indcoord[0];
                indtevtrans[1] = t * indcoord[0];
                break;
            case 8: // t matrix
                fscale = powf(2.0f, (float)(scale - 17)) / 256;
                indtevtrans[0] = s * indcoord[1];
                indtevtrans[1] = t * indcoord[1];
                break;
        }

        indtevtrans[0] *= fscale;
        indtevtrans[1] *= fscale;
    }

    if (indirect.fb_addprev)
    {
        TexCoord[0] += WrapIndirectCoord(s, indirect.sw) + indtevtrans[0];
        TexCoord[1] += WrapIndirectCoord(t, indirect.tw) + indtevtrans[1];
    }
    else
    {
        TexCoord[0] = WrapIndirectCoord(s, indirect.sw) + indtevtrans[0];
        TexCoord[1] = WrapIndirectCoord(t, indirect.tw) + indtevtrans[1];
    }
}

void Tev::Draw()
{
    _assert_(Position[0] >= 0 && Position[0] < EFB_WIDTH);
    _assert_(Position[1] >= 0 && Position[1] < EFB_HEIGHT);

    INCSTAT(stats.thisFrame.tevPixelsIn);

    for (unsigned int stageNum = 0; stageNum < bpmem.genMode.numindstages; stageNum++)
    {
        int stageNum2 = stageNum >> 1;
        int stageOdd = stageNum&1;

        u32 texcoordSel = bpmem.tevindref.getTexCoord(stageNum);
        u32 texmap = bpmem.tevindref.getTexMap(stageNum);

        float scaleS = bpmem.texscale[stageNum2].getScaleS(stageOdd);
        float scaleT = bpmem.texscale[stageNum2].getScaleT(stageOdd);

        TextureSampler::Sample(Uv[texcoordSel][0] * scaleS, Uv[texcoordSel][1] * scaleT, Lod[texcoordSel], texmap, IndirectTex[stageNum]);

#ifdef _DEBUG
        if (g_Config.bDumpTevStages)
        {
            u8 stage[4] = {(u8)IndirectTex[stageNum][3], (u8)IndirectTex[stageNum][2], (u8)IndirectTex[stageNum][1], 255};
            DebugUtil::DrawObjectBuffer(Position[0], Position[1], stage, 16+stageNum, "Ind");
        }
#endif
    }

    for (unsigned int stageNum = 0; stageNum <= bpmem.genMode.numtevstages; stageNum++)
    {
        int stageNum2 = stageNum >> 1;
        int stageOdd = stageNum&1;
        TwoTevStageOrders &order = bpmem.tevorders[stageNum2];
        TevKSel &kSel = bpmem.tevksel[stageNum2];        

        // stage combiners
        TevStageCombiner::ColorCombiner &cc = bpmem.combiners[stageNum].colorC;
        TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[stageNum].alphaC;

        int texcoordSel = order.getTexCoord(stageOdd);
        int texmap = order.getTexMap(stageOdd);

        Indirect(stageNum, Uv[texcoordSel][0], Uv[texcoordSel][1]);

        // sample texture
        if (order.getEnable(stageOdd))
        {
            u8 texel[4];
    
            TextureSampler::Sample(TexCoord[0], TexCoord[1], Lod[texcoordSel], texmap, texel);

            int swaptable = ac.tswap * 2;            

            TexColor[0] = texel[bpmem.tevksel[swaptable].swap1];
            TexColor[1] = texel[bpmem.tevksel[swaptable].swap2];
            swaptable++;
            TexColor[2] = texel[bpmem.tevksel[swaptable].swap1];
            TexColor[3] = texel[bpmem.tevksel[swaptable].swap2];
        }

        // set konst for this stage
        int kc = kSel.getKC(stageOdd);
        int ka = kSel.getKA(stageOdd);
        StageKonst[RED_C] = *(m_KonstLUT[kc][RED_C]);
        StageKonst[GRN_C] = *(m_KonstLUT[kc][GRN_C]);
        StageKonst[BLU_C] = *(m_KonstLUT[kc][BLU_C]);
        StageKonst[ALP_C] = *(m_KonstLUT[ka][ALP_C]);

        // set color        
        SetRasColor(order.getColorChan(stageOdd), ac.rswap * 2);

        // combine inputs
        if (cc.bias != 3)
            DrawColorRegular(cc);
        else
            DrawColorCompare(cc);

        if (cc.clamp)
        {
            Reg[cc.dest][RED_C] = Clamp255(Reg[cc.dest][RED_C]);
            Reg[cc.dest][GRN_C] = Clamp255(Reg[cc.dest][GRN_C]);
            Reg[cc.dest][BLU_C] = Clamp255(Reg[cc.dest][BLU_C]);
        }
        else
        {
            Reg[cc.dest][RED_C] = Clamp1024(Reg[cc.dest][RED_C]);
            Reg[cc.dest][GRN_C] = Clamp1024(Reg[cc.dest][GRN_C]);
            Reg[cc.dest][BLU_C] = Clamp1024(Reg[cc.dest][BLU_C]);
        }

        if (ac.bias != 3)
            DrawAlphaRegular(ac);
        else
            DrawAlphaCompare(ac);

        if (ac.clamp)
            Reg[ac.dest][ALP_C] = Clamp255(Reg[ac.dest][ALP_C]);
        else
            Reg[ac.dest][ALP_C] = Clamp1024(Reg[ac.dest][ALP_C]);

#ifdef _DEBUG
        if (g_Config.bDumpTevStages)
        {
            u8 stage[4] = {(u8)Reg[0][0], (u8)Reg[0][1], (u8)Reg[0][2], (u8)Reg[0][3]};
            DebugUtil::DrawObjectBuffer(Position[0], Position[1], stage, stageNum, "Stage");
        }
#endif
    }
    
    // convert to 8 bits per component
    u8 output[4] = {(u8)Reg[0][0], (u8)Reg[0][1], (u8)Reg[0][2], (u8)Reg[0][3]};

    if (!AlphaTest(output[ALP_C]))
        return;

    // z texture
    if (bpmem.ztex2.op)
    {
        u32 ztex = bpmem.ztex1.bias;
        switch (bpmem.ztex2.type) {
            case 0: // 8 bit
                ztex += TexColor[ALP_C];
                break;
            case 1: // 16 bit
                ztex += TexColor[ALP_C] << 8 | TexColor[RED_C];
                break;
            case 2: // 24 bit
                ztex += TexColor[RED_C] << 16 | TexColor[GRN_C] << 8 | TexColor[BLU_C];
                break;
        }

        if (bpmem.ztex2.op == ZTEXTURE_ADD)
            ztex += Position[2];

        Position[2] = ztex & 0x00ffffff;
    }

    if (!bpmem.zcontrol.zcomploc && bpmem.zmode.testenable)
    {
        if (!EfbInterface::ZCompare(Position[0], Position[1], Position[2]))
            return;
    }

    INCSTAT(stats.thisFrame.tevPixelsOut);

    EfbInterface::BlendTev(Position[0], Position[1], output);
}

void Tev::SetRegColor(int reg, int comp, bool konst, s16 color)
{
    if (konst)
    {
        KonstantColors[reg][comp] = color;
    }
    else
    {
        Reg[reg][comp] = color;
    }
}
