// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "Tev.h"
#include "EfbInterface.h"
#include "TextureSampler.h"
#include "XFMemLoader.h"
#include "SWPixelEngine.h"
#include "SWStatistics.h"
#include "SWVideoConfig.h"
#include "DebugUtil.h"

#include <cmath>

#ifdef _DEBUG
#define ALLOW_TEV_DUMPS 1
#else
#define ALLOW_TEV_DUMPS 0
#endif

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

	m_ColorInputLUT[0][RED_INP] = &Reg[0][RED_C]; m_ColorInputLUT[0][GRN_INP] = &Reg[0][GRN_C]; m_ColorInputLUT[0][BLU_INP] = &Reg[0][BLU_C]; // prev.rgb
	m_ColorInputLUT[1][RED_INP] = &Reg[0][ALP_C]; m_ColorInputLUT[1][GRN_INP] = &Reg[0][ALP_C]; m_ColorInputLUT[1][BLU_INP] = &Reg[0][ALP_C]; // prev.aaa
	m_ColorInputLUT[2][RED_INP] = &Reg[1][RED_C]; m_ColorInputLUT[2][GRN_INP] = &Reg[1][GRN_C]; m_ColorInputLUT[2][BLU_INP] = &Reg[1][BLU_C]; // c0.rgb
	m_ColorInputLUT[3][RED_INP] = &Reg[1][ALP_C]; m_ColorInputLUT[3][GRN_INP] = &Reg[1][ALP_C]; m_ColorInputLUT[3][BLU_INP] = &Reg[1][ALP_C]; // c0.aaa
	m_ColorInputLUT[4][RED_INP] = &Reg[2][RED_C]; m_ColorInputLUT[4][GRN_INP] = &Reg[2][GRN_C]; m_ColorInputLUT[4][BLU_INP] = &Reg[2][BLU_C]; // c1.rgb
	m_ColorInputLUT[5][RED_INP] = &Reg[2][ALP_C]; m_ColorInputLUT[5][GRN_INP] = &Reg[2][ALP_C]; m_ColorInputLUT[5][BLU_INP] = &Reg[2][ALP_C]; // c1.aaa
	m_ColorInputLUT[6][RED_INP] = &Reg[3][RED_C]; m_ColorInputLUT[6][GRN_INP] = &Reg[3][GRN_C]; m_ColorInputLUT[6][BLU_INP] = &Reg[3][BLU_C]; // c2.rgb
	m_ColorInputLUT[7][RED_INP] = &Reg[3][ALP_C]; m_ColorInputLUT[7][GRN_INP] = &Reg[3][ALP_C]; m_ColorInputLUT[7][BLU_INP] = &Reg[3][ALP_C]; // c2.aaa
	m_ColorInputLUT[8][RED_INP] = &TexColor[RED_C]; m_ColorInputLUT[8][GRN_INP] = &TexColor[GRN_C]; m_ColorInputLUT[8][BLU_INP] = &TexColor[BLU_C]; // tex.rgb
	m_ColorInputLUT[9][RED_INP] = &TexColor[ALP_C]; m_ColorInputLUT[9][GRN_INP] = &TexColor[ALP_C]; m_ColorInputLUT[9][BLU_INP] = &TexColor[ALP_C]; // tex.aaa
	m_ColorInputLUT[10][RED_INP] = &RasColor[RED_C]; m_ColorInputLUT[10][GRN_INP] = &RasColor[GRN_C]; m_ColorInputLUT[10][BLU_INP] = &RasColor[BLU_C]; // ras.rgb
	m_ColorInputLUT[11][RED_INP] = &RasColor[ALP_C]; m_ColorInputLUT[11][GRN_INP] = &RasColor[ALP_C]; m_ColorInputLUT[11][BLU_INP] = &RasColor[ALP_C]; // ras.rgb
	m_ColorInputLUT[12][RED_INP] = &FixedConstants[8]; m_ColorInputLUT[12][GRN_INP] = &FixedConstants[8]; m_ColorInputLUT[12][BLU_INP] = &FixedConstants[8]; // one
	m_ColorInputLUT[13][RED_INP] = &FixedConstants[4]; m_ColorInputLUT[13][GRN_INP] = &FixedConstants[4]; m_ColorInputLUT[13][BLU_INP] = &FixedConstants[4]; // half
	m_ColorInputLUT[14][RED_INP] = &StageKonst[RED_C]; m_ColorInputLUT[14][GRN_INP] = &StageKonst[GRN_C]; m_ColorInputLUT[14][BLU_INP] = &StageKonst[BLU_C]; // konst
	m_ColorInputLUT[15][RED_INP] = &FixedConstants[0]; m_ColorInputLUT[15][GRN_INP] = &FixedConstants[0]; m_ColorInputLUT[15][BLU_INP] = &FixedConstants[0]; // zero

	m_AlphaInputLUT[0] = Reg[0]; // prev
	m_AlphaInputLUT[1] = Reg[1]; // c0
	m_AlphaInputLUT[2] = Reg[2]; // c1
	m_AlphaInputLUT[3] = Reg[3]; // c2
	m_AlphaInputLUT[4] = TexColor; // tex
	m_AlphaInputLUT[5] = RasColor; // ras
	m_AlphaInputLUT[6] = StageKonst; // konst
	m_AlphaInputLUT[7] = Zero16; // zero

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

void Tev::SetRasColor(int colorChan, int swaptable)
{
	switch(colorChan)
	{
	case 0: // Color0
		{
			u8 *color = Color[0];
			RasColor[RED_C] = color[bpmem.tevksel[swaptable].swap1];
			RasColor[GRN_C] = color[bpmem.tevksel[swaptable].swap2];
			swaptable++;
			RasColor[BLU_C] = color[bpmem.tevksel[swaptable].swap1];
			RasColor[ALP_C] = color[bpmem.tevksel[swaptable].swap2];
		}
		break;
	case 1: // Color1
		{
			u8 *color = Color[1];
			RasColor[RED_C] = color[bpmem.tevksel[swaptable].swap1];
			RasColor[GRN_C] = color[bpmem.tevksel[swaptable].swap2];
			swaptable++;
			RasColor[BLU_C] = color[bpmem.tevksel[swaptable].swap1];
			RasColor[ALP_C] = color[bpmem.tevksel[swaptable].swap2];
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

		Reg[cc.dest][BLU_C + i] = result;
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
			a = *m_ColorInputLUT[cc.a][RED_INP] & 0xff;
			b = *m_ColorInputLUT[cc.b][RED_INP] & 0xff;
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
			}
		}
		break;

	case TEVCMP_R8_EQ:
		{
			a = *m_ColorInputLUT[cc.a][RED_INP] & 0xff;
			b = *m_ColorInputLUT[cc.b][RED_INP] & 0xff;
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
			}
		}
		break;
	case TEVCMP_GR16_GT:
		{
			a = ((*m_ColorInputLUT[cc.a][GRN_INP] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_INP] & 0xff);
			b = ((*m_ColorInputLUT[cc.b][GRN_INP] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_INP] & 0xff);
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
			}
		}
		break;
	case TEVCMP_GR16_EQ:
		{
			a = ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_INP] & 0xff);
			b = ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_INP] & 0xff);
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
			}
		}
		break;
	case TEVCMP_BGR24_GT:
		{
			a = ((*m_ColorInputLUT[cc.a][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_INP] & 0xff);
			b = ((*m_ColorInputLUT[cc.b][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_INP] & 0xff);
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a > b) ? InputReg.c : 0);
			}
		}
		break;
	case TEVCMP_BGR24_EQ:
		{
			a = ((*m_ColorInputLUT[cc.a][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.a][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.a][RED_INP] & 0xff);
			b = ((*m_ColorInputLUT[cc.b][BLU_C] & 0xff) << 16) | ((*m_ColorInputLUT[cc.b][GRN_C] & 0xff) << 8) | (*m_ColorInputLUT[cc.b][RED_INP] & 0xff);
			for (int i = 0; i < 3; i++)
			{
				InputReg.c = *m_ColorInputLUT[cc.c][i];
				InputReg.d = *m_ColorInputLUT[cc.d][i];
				Reg[cc.dest][BLU_C + i] = InputReg.d + ((a == b) ? InputReg.c : 0);
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
			Reg[cc.dest][BLU_C + i] = InputReg.d + ((InputReg.a > InputReg.b) ? InputReg.c : 0);
		}
		break;
	case TEVCMP_RGB8_EQ: 
		for (int i = 0; i < 3; i++)
		{
			InputReg.a = *m_ColorInputLUT[cc.a][i];
			InputReg.b = *m_ColorInputLUT[cc.b][i];
			InputReg.c = *m_ColorInputLUT[cc.c][i];
			InputReg.d = *m_ColorInputLUT[cc.d][i];
			Reg[cc.dest][BLU_C + i] = InputReg.d + ((InputReg.a == InputReg.b) ? InputReg.c : 0);
		}
		break;
	}
}

void Tev::DrawAlphaRegular(TevStageCombiner::AlphaCombiner &ac)
{
	InputRegType InputReg;

	InputReg.a = m_AlphaInputLUT[ac.a][ALP_C];
	InputReg.b = m_AlphaInputLUT[ac.b][ALP_C];
	InputReg.c = m_AlphaInputLUT[ac.c][ALP_C];
	InputReg.d = m_AlphaInputLUT[ac.d][ALP_C];

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

static bool TevAlphaTest(int alpha)
{
	bool comp0 = AlphaCompare(alpha, bpmem.alpha_test.ref0, bpmem.alpha_test.comp0);
	bool comp1 = AlphaCompare(alpha, bpmem.alpha_test.ref1, bpmem.alpha_test.comp1);

	switch (bpmem.alpha_test.logic)
	{
	case 0: return comp0 && comp1;   // and
	case 1: return comp0 || comp1;   // or
	case 2: return comp0 ^ comp1;    // xor
	case 3: return !(comp0 ^ comp1); // xnor
	}
	return true;
}

inline s32 WrapIndirectCoord(s32 coord, int wrapMode)
{
	switch (wrapMode)
	{
		case ITW_OFF:
			return coord;
		case ITW_256:
			return (coord % (256 << 7));
		case ITW_128:
			return (coord % (128 << 7));
		case ITW_64:
			return (coord % (64 << 7));
		case ITW_32:
			return (coord % (32 << 7));
		case ITW_16:
			return (coord % (16 << 7));
		case ITW_0:
			return 0;
	}
	return 0;
}

void Tev::Indirect(unsigned int stageNum, s32 s, s32 t)
{
	TevStageIndirect &indirect = bpmem.tevind[stageNum];
	u8 *indmap = IndirectTex[indirect.bt];

	s32 indcoord[3];

	// alpha bump select
	switch (indirect.bs)
	{
		case ITBA_OFF:
			AlphaBump = 0;
			break;
			case ITBA_S:
			AlphaBump = indmap[TextureSampler::ALP_SMP];
			break;
		case ITBA_T:
			AlphaBump = indmap[TextureSampler::BLU_SMP];
			break;
		case ITBA_U:
			AlphaBump = indmap[TextureSampler::GRN_SMP];
			break;
	}

	// bias select
	s16 biasValue = indirect.fmt==ITF_8?-128:1;
	s16 bias[3];
	bias[0] = indirect.bias&1?biasValue:0;
	bias[1] = indirect.bias&2?biasValue:0;
	bias[2] = indirect.bias&4?biasValue:0;

	// format
	switch(indirect.fmt)
	{
		case ITF_8:
			indcoord[0] = indmap[TextureSampler::ALP_SMP] + bias[0];
			indcoord[1] = indmap[TextureSampler::BLU_SMP] + bias[1];
			indcoord[2] = indmap[TextureSampler::GRN_SMP] + bias[2];
			AlphaBump = AlphaBump & 0xf8;
			break;
		case ITF_5:
			indcoord[0] = (indmap[TextureSampler::ALP_SMP] & 0x1f) + bias[0];
			indcoord[1] = (indmap[TextureSampler::BLU_SMP] & 0x1f) + bias[1];
			indcoord[2] = (indmap[TextureSampler::GRN_SMP] & 0x1f) + bias[2];
			AlphaBump = AlphaBump & 0xe0;
			break;
		case ITF_4:
			indcoord[0] = (indmap[TextureSampler::ALP_SMP] & 0x0f) + bias[0];
			indcoord[1] = (indmap[TextureSampler::BLU_SMP] & 0x0f) + bias[1];
			indcoord[2] = (indmap[TextureSampler::GRN_SMP] & 0x0f) + bias[2];
			AlphaBump = AlphaBump & 0xf0;
			break;
		case ITF_3:
			indcoord[0] = (indmap[TextureSampler::ALP_SMP] & 0x07) + bias[0];
			indcoord[1] = (indmap[TextureSampler::BLU_SMP] & 0x07) + bias[1];
			indcoord[2] = (indmap[TextureSampler::GRN_SMP] & 0x07) + bias[2];
			AlphaBump = AlphaBump & 0xf8;
			break;
		default:
			PanicAlert("Tev::Indirect");
			return;
	}

	s64 indtevtrans[2] = { 0,0 };

	// matrix multiply
	int indmtxid = indirect.mid & 3;
	if (indmtxid)
	{
		IND_MTX &indmtx = bpmem.indmtx[indmtxid - 1];
		int scale = ((u32)indmtx.col0.s0 << 0) |
					((u32)indmtx.col1.s1 << 2) |
					((u32)indmtx.col2.s2 << 4);

		int shift;

		switch (indirect.mid & 12)
		{
			case 0:
				shift = 3 + (17 - scale);
				indtevtrans[0] = indmtx.col0.ma * indcoord[0] + indmtx.col1.mc * indcoord[1] + indmtx.col2.me * indcoord[2];
				indtevtrans[1] = indmtx.col0.mb * indcoord[0] + indmtx.col1.md * indcoord[1] + indmtx.col2.mf * indcoord[2];
				break;
			case 4: // s matrix
				shift = 8 + (17 - scale);
				indtevtrans[0] = s * indcoord[0];
				indtevtrans[1] = t * indcoord[0];
				break;
			case 8: // t matrix
				shift = 8 + (17 - scale);
				indtevtrans[0] = s * indcoord[1];
				indtevtrans[1] = t * indcoord[1];
				break;
			default:
				return;
		}

		indtevtrans[0] = shift >= 0 ? indtevtrans[0] >> shift : indtevtrans[0] << -shift;
		indtevtrans[1] = shift >= 0 ? indtevtrans[1] >> shift : indtevtrans[1] << -shift;
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
	_assert_(Position[0] >= 0 && Position[0] < EFB_WIDTH);
	_assert_(Position[1] >= 0 && Position[1] < EFB_HEIGHT);

	INCSTAT(swstats.thisFrame.tevPixelsIn);

	for (unsigned int stageNum = 0; stageNum < bpmem.genMode.numindstages; stageNum++)
	{
		int stageNum2 = stageNum >> 1;
		int stageOdd = stageNum&1;

		u32 texcoordSel = bpmem.tevindref.getTexCoord(stageNum);
		u32 texmap = bpmem.tevindref.getTexMap(stageNum);

		const TEXSCALE& texscale = bpmem.texscale[stageNum2];
		s32 scaleS = stageOdd ? texscale.ss1:texscale.ss0;
		s32 scaleT = stageOdd ? texscale.ts1:texscale.ts0;

		TextureSampler::Sample(Uv[texcoordSel].s >> scaleS, Uv[texcoordSel].t >> scaleT,
			IndirectLod[stageNum], IndirectLinear[stageNum], texmap, IndirectTex[stageNum]);

#if ALLOW_TEV_DUMPS
		if (g_SWVideoConfig.bDumpTevStages)
		{
			u8 stage[4] = { IndirectTex[stageNum][TextureSampler::ALP_SMP],
							IndirectTex[stageNum][TextureSampler::BLU_SMP],
							IndirectTex[stageNum][TextureSampler::GRN_SMP],
							255};
			DebugUtil::DrawTempBuffer(stage, INDIRECT + stageNum);
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

		Indirect(stageNum, Uv[texcoordSel].s, Uv[texcoordSel].t);

		// sample texture
		if (order.getEnable(stageOdd))
		{
			// RGBA
			u8 texel[4];

			TextureSampler::Sample(TexCoord.s, TexCoord.t, TextureLod[stageNum], TextureLinear[stageNum], texmap, texel);

#if ALLOW_TEV_DUMPS
			if (g_SWVideoConfig.bDumpTevTextureFetches)
				DebugUtil::DrawTempBuffer(texel, DIRECT_TFETCH + stageNum);
#endif

			int swaptable = ac.tswap * 2;

			TexColor[RED_C] = texel[bpmem.tevksel[swaptable].swap1];
			TexColor[GRN_C] = texel[bpmem.tevksel[swaptable].swap2];
			swaptable++;
			TexColor[BLU_C] = texel[bpmem.tevksel[swaptable].swap1];
			TexColor[ALP_C] = texel[bpmem.tevksel[swaptable].swap2];
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

#if ALLOW_TEV_DUMPS
		if (g_SWVideoConfig.bDumpTevStages)
		{
			u8 stage[4] = {(u8)Reg[0][RED_C], (u8)Reg[0][GRN_C], (u8)Reg[0][BLU_C], (u8)Reg[0][ALP_C]};
			DebugUtil::DrawTempBuffer(stage, DIRECT + stageNum);
		}
#endif
	}

	// convert to 8 bits per component
	// the results of the last tev stage are put onto the screen,
	// regardless of the used destination register - TODO: Verify!
	u32 color_index = bpmem.combiners[bpmem.genMode.numtevstages].colorC.dest;
	u32 alpha_index = bpmem.combiners[bpmem.genMode.numtevstages].alphaC.dest;
	u8 output[4] = {(u8)Reg[alpha_index][ALP_C], (u8)Reg[color_index][BLU_C], (u8)Reg[color_index][GRN_C], (u8)Reg[color_index][RED_C]};

	if (!TevAlphaTest(output[ALP_C]))
		return;

	// z texture
	if (bpmem.ztex2.op)
	{
		u32 ztex = bpmem.ztex1.bias;
		switch (bpmem.ztex2.type)
		{
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

	// fog
	if (bpmem.fog.c_proj_fsel.fsel)
	{
		float ze;

		if (bpmem.fog.c_proj_fsel.proj == 0)
		{
			// perspective
			// ze = A/(B - (Zs >> B_SHF))
			s32 denom = bpmem.fog.b_magnitude - (Position[2] >> bpmem.fog.b_shift);
			//in addition downscale magnitude and zs to 0.24 bits
			ze = (bpmem.fog.a.GetA() * 16777215.0f) / (float)denom;
		} 
		else 
		{
			// orthographic
			// ze = a*Zs
			//in addition downscale zs to 0.24 bits
			ze = bpmem.fog.a.GetA() * ((float)Position[2] / 16777215.0f);

		}

		if(bpmem.fogRange.Base.Enabled)
		{
			// TODO: This is untested and should definitely be checked against real hw.
			// - No idea if offset is really normalized against the viewport width or against the projection matrix or yet something else
			// - scaling of the "k" coefficient isn't clear either.

			// First, calculate the offset from the viewport center (normalized to 0..1)
			float offset = (Position[0] - (bpmem.fogRange.Base.Center - 342)) / (float)swxfregs.viewport.wd;
			// Based on that, choose the index such that points which are far away from the z-axis use the 10th "k" value and such that central points use the first value.
			int index = 9 - std::abs(offset) * 9.f;
			index = (index < 0) ? 0 : (index > 9) ? 9 : index; // TODO: Shouldn't be necessary!
			// Look up coefficient... Seems like multiplying by 4 makes Fortune Street work properly (fog is too strong without the factor)
			float k = bpmem.fogRange.K[index/2].GetValue(index%2) * 4.f;
			float x_adjust = sqrt(offset*offset + k*k)/k;
			ze *= x_adjust; // NOTE: This is basically dividing by a cosine (hidden behind GXInitFogAdjTable): 1/cos = c/b = sqrt(a^2+b^2)/b
		}

		ze -= bpmem.fog.c_proj_fsel.GetC();

		// clamp 0 to 1
		float fog = (ze<0.0f) ? 0.0f : ((ze>1.0f) ? 1.0f : ze);

		switch (bpmem.fog.c_proj_fsel.fsel)
		{
			case 4: // exp
				fog = 1.0f - pow(2.0f, -8.0f * fog);
				break;
			case 5: // exp2
				fog = 1.0f - pow(2.0f, -8.0f * fog * fog);
				break;
			case 6: // backward exp
				fog = 1.0f - fog;
				fog = pow(2.0f, -8.0f * fog);
				break;
			case 7: // backward exp2
				fog = 1.0f - fog;
				fog = pow(2.0f, -8.0f * fog * fog);
				break;
		}

		// lerp from output to fog color
		u32 fogInt = (u32)(fog * 256);
		u32 invFog = 256 - fogInt;

		output[RED_C] = (output[RED_C] * invFog + fogInt * bpmem.fog.color.r) >> 8;
		output[GRN_C] = (output[GRN_C] * invFog + fogInt * bpmem.fog.color.g) >> 8;
		output[BLU_C] = (output[BLU_C] * invFog + fogInt * bpmem.fog.color.b) >> 8;
	}

	bool late_ztest = !bpmem.zcontrol.early_ztest || !g_SWVideoConfig.bZComploc;
	if (late_ztest && bpmem.zmode.testenable)
	{
		// TODO: Check against hw if these values get incremented even if depth testing is disabled
		SWPixelEngine::pereg.IncZInputQuadCount(false);

		if (!EfbInterface::ZCompare(Position[0], Position[1], Position[2]))
			return;

		SWPixelEngine::pereg.IncZOutputQuadCount(false);
	}

#if ALLOW_TEV_DUMPS
	if (g_SWVideoConfig.bDumpTevStages)
	{
		for (u32 i = 0; i < bpmem.genMode.numindstages; ++i)
			DebugUtil::CopyTempBuffer(Position[0], Position[1], INDIRECT, i, "Indirect");
		for (u32 i = 0; i <= bpmem.genMode.numtevstages; ++i)
			DebugUtil::CopyTempBuffer(Position[0], Position[1], DIRECT, i, "Stage");
	}

	if (g_SWVideoConfig.bDumpTevTextureFetches)
	{
		for (u32 i = 0; i <= bpmem.genMode.numtevstages; ++i)
		{
			TwoTevStageOrders &order = bpmem.tevorders[i >> 1];
			if (order.getEnable(i & 1))
				DebugUtil::CopyTempBuffer(Position[0], Position[1], DIRECT_TFETCH, i, "TFetch");
		}
	}
#endif

	INCSTAT(swstats.thisFrame.tevPixelsOut);
	SWPixelEngine::pereg.IncBlendInputQuadCount();

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

void Tev::DoState(PointerWrap &p)
{
	p.DoArray(Reg, sizeof(Reg));
	
	p.DoArray(KonstantColors, sizeof(KonstantColors));
	p.DoArray(TexColor,4);
	p.DoArray(RasColor,4);
	p.DoArray(StageKonst,4);
	p.DoArray(Zero16,4);

	p.DoArray(FixedConstants,9);
	p.Do(AlphaBump);
	p.DoArray(IndirectTex, sizeof(IndirectTex));
	p.Do(TexCoord);

	p.DoArray(m_BiasLUT,4);
	p.DoArray(m_ScaleLShiftLUT,4);
	p.DoArray(m_ScaleRShiftLUT,4);

	p.DoArray(Position,3);
	p.DoArray(Color, sizeof(Color));
	p.DoArray(Uv, 8);
	p.DoArray(IndirectLod,4);
	p.DoArray(IndirectLinear,4);
	p.DoArray(TextureLod,16);
	p.DoArray(TextureLinear,16);
}
