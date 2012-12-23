// Copyright (C) 2003 Dolphin Project.

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

#include <stdio.h>
#include <cmath>
#include <assert.h>
#include <locale.h>

#include "LightingShaderGen.h"
#include "PixelShaderGen.h"
#include "XFMemory.h"  // for texture projection mode
#include "BPMemory.h"
#include "VideoConfig.h"
#include "NativeVertexFormat.h"

static int AlphaPreTest();

static void StageHash(int stage, u32* out)
{
	out[0] |= bpmem.combiners[stage].colorC.hex & 0xFFFFFF; // 24
	u32 alphaC = bpmem.combiners[stage].alphaC.hex & 0xFFFFF0; // 24, strip out tswap and rswap for now
	out[0] |= (alphaC&0xF0) << 24; // 8
	out[1] |= alphaC >> 8; // 16

	// reserve 3 bits for bpmem.tevorders[stage/2].getTexMap
	out[1] |= bpmem.tevorders[stage/2].getTexCoord(stage&1) << 19; // 3
	out[1] |= bpmem.tevorders[stage/2].getEnable(stage&1) << 22; // 1
	// reserve 3 bits for bpmem.tevorders[stage/2].getColorChan

	bool bHasIndStage = bpmem.tevind[stage].IsActive() && bpmem.tevind[stage].bt < bpmem.genMode.numindstages;
	out[2] |= bHasIndStage << 2; // 1

	bool needstexcoord = false;

	if (bHasIndStage)
	{
		out[2] |= (bpmem.tevind[stage].hex & 0x17FFFF) << 3; // 21, TODO: needs an explanation
		needstexcoord = true;
	}


	TevStageCombiner::ColorCombiner& cc = bpmem.combiners[stage].colorC;
	TevStageCombiner::AlphaCombiner& ac = bpmem.combiners[stage].alphaC;

	if(cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC
		|| cc.b == TEVCOLORARG_RASA || cc.b == TEVCOLORARG_RASC
		|| cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC
		|| cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC
		|| ac.a == TEVALPHAARG_RASA || ac.b == TEVALPHAARG_RASA
		|| ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
	{
		out[0] |= bpmem.combiners[stage].alphaC.rswap;
		out[2] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.rswap*2].swap1 << 24; // 2
		out[2] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.rswap*2].swap2 << 26; // 2
		out[2] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.rswap*2+1].swap1 << 28; // 2
		out[2] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.rswap*2+1].swap2 << 30; // 2
		out[1] |= (bpmem.tevorders[stage/2].getColorChan(stage&1)&1) << 23;
		out[2] |= (bpmem.tevorders[stage/2].getColorChan(stage&1)&0x6) >> 1;
	}

	out[3] |= bpmem.tevorders[stage/2].getEnable(stage&1);
	if (bpmem.tevorders[stage/2].getEnable(stage&1))
	{
		if (bHasIndStage) needstexcoord = true;

		out[0] |= bpmem.combiners[stage].alphaC.tswap;
		out[3] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.tswap*2].swap1 << 1; // 2
		out[3] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.tswap*2].swap2 << 3; // 2
		out[3] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.tswap*2+1].swap1 << 5; // 2
		out[3] |= bpmem.tevksel[bpmem.combiners[stage].alphaC.tswap*2+1].swap2 << 7; // 2
		out[1] |= bpmem.tevorders[stage/2].getTexMap(stage&1) << 16;
	}

	if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST
		|| ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST || ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
	{
		out[3] |= bpmem.tevksel[stage/2].getKC(stage&1) << 9; // 5
		out[3] |= bpmem.tevksel[stage/2].getKA(stage&1) << 14; // 5
	}

	if (needstexcoord)
	{
		out[1] |= bpmem.tevorders[stage/2].getTexCoord(stage&1) << 16;
	}
}

// Mash together all the inputs that contribute to the code of a generated pixel shader into
// a unique identifier, basically containing all the bits. Yup, it's a lot ....
// It would likely be a lot more efficient to build this incrementally as the attributes
// are set...
void GetPixelShaderId(PIXELSHADERUID *uid, DSTALPHA_MODE dstAlphaMode, u32 components)
{
	memset(uid->values, 0, sizeof(uid->values));
	uid->values[0] |= bpmem.genMode.numtevstages; // 4
	uid->values[0] |= bpmem.genMode.numtexgens << 4; // 4
	uid->values[0] |= dstAlphaMode << 8; // 2

	bool DepthTextureEnable = (bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.zcomploc && bpmem.zmode.testenable && bpmem.zmode.updateenable) || g_ActiveConfig.bEnablePerPixelDepth;

	uid->values[0] |= DepthTextureEnable << 10; // 1

	bool enablePL = g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting;
	uid->values[0] |= enablePL << 11; // 1

	if (!enablePL) uid->values[0] |= xfregs.numTexGen.numTexGens << 12; // 4
	u32 alphaPreTest = AlphaPreTest()+1;

	uid->values[0] |= alphaPreTest << 16; // 2

	if (alphaPreTest == 1 || (alphaPreTest && !DepthTextureEnable && dstAlphaMode == DSTALPHA_ALPHA_PASS))
	{
		// Courtesy of PreAlphaTest, we're done already ;)
		// NOTE: The comment header of generated shaders depends on the value of bpmem.genmode.numindstages.. shouldnt really bother about that though.
		uid->num_values = 1;
		return;
	}

	// numtexgens should be <= 8
	for (unsigned int i = 0; i < bpmem.genMode.numtexgens; ++i)
		uid->values[0] |= xfregs.texMtxInfo[i].projection << (18+i); // 1

	uid->values[1] = bpmem.genMode.numindstages; // 3
	u32 indirectStagesUsed = 0;
	for (unsigned int i = 0; i < bpmem.genMode.numindstages; ++i)
		if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
			indirectStagesUsed |= (1 << bpmem.tevind[i].bt);

	assert(indirectStagesUsed == (indirectStagesUsed & 0xF));

	uid->values[1] |= indirectStagesUsed << 3; // 4;

	for (unsigned int i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (indirectStagesUsed & (1 << i))
		{
			uid->values[1] |= (bpmem.tevindref.getTexCoord(i) < bpmem.genMode.numtexgens) << (7 + 3*i); // 1
			if (bpmem.tevindref.getTexCoord(i) < bpmem.genMode.numtexgens)
				uid->values[1] |= bpmem.tevindref.getTexCoord(i) << (8 + 3*i); // 2
		}
	}

	u32* ptr = &uid->values[2];
	for (int i = 0; i < bpmem.genMode.numtevstages+1; ++i)
	{
		StageHash(i, ptr);
		ptr += 4; // max: ptr = &uid->values[66]
	}

	ptr[0] |= bpmem.alphaFunc.comp0; // 3
	ptr[0] |= bpmem.alphaFunc.comp1 << 3; // 3
	ptr[0] |= bpmem.alphaFunc.logic << 6; // 2

	if (alphaPreTest == 0 || alphaPreTest == 2)
	{
		ptr[0] |= bpmem.fog.c_proj_fsel.fsel << 8; // 3
		if (DepthTextureEnable)
		{
			ptr[0] |= bpmem.ztex2.op << 11; // 2
			ptr[0] |= bpmem.zcontrol.zcomploc << 13; // 1
			ptr[0] |= bpmem.zmode.testenable << 14; // 1
			ptr[0] |= bpmem.zmode.updateenable << 15; // 1
		}
	}

	if (dstAlphaMode != DSTALPHA_ALPHA_PASS)
	{
		if (bpmem.fog.c_proj_fsel.fsel != 0)
		{
			ptr[0] |= bpmem.fog.c_proj_fsel.proj << 16; // 1
			ptr[0] |= bpmem.fogRange.Base.Enabled << 17; // 1
		}
	}

	++ptr;
	if (enablePL)
	{
		ptr += GetLightingShaderId(ptr);
		*ptr++ = components;
	}

	uid->num_values = ptr - uid->values;
}

void GetSafePixelShaderId(PIXELSHADERUIDSAFE *uid, DSTALPHA_MODE dstAlphaMode, u32 components)
{
	memset(uid->values, 0, sizeof(uid->values));
	u32* ptr = uid->values;
	*ptr++ = dstAlphaMode; // 0
	*ptr++ = bpmem.genMode.hex; // 1
	*ptr++ = bpmem.ztex2.hex; // 2
	*ptr++ = bpmem.zcontrol.hex; // 3
	*ptr++ = bpmem.zmode.hex; // 4
	*ptr++ = g_ActiveConfig.bEnablePerPixelDepth; // 5
	*ptr++ = g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting; // 6
	*ptr++ = xfregs.numTexGen.hex; // 7

	if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		*ptr++ = xfregs.color[0].hex;
		*ptr++ = xfregs.alpha[0].hex;
		*ptr++ = xfregs.color[1].hex;
		*ptr++ = xfregs.alpha[1].hex;
		*ptr++ = components;
	}

	for (unsigned int i = 0; i < 8; ++i)
		*ptr++ = xfregs.texMtxInfo[i].hex; // 8-15

	for (unsigned int i = 0; i < 16; ++i)
		*ptr++ = bpmem.tevind[i].hex; // 16-31

	*ptr++ = bpmem.tevindref.hex; // 32

	for (int i = 0; i < bpmem.genMode.numtevstages+1; ++i) // up to 16 times
	{
		*ptr++ = bpmem.combiners[i].colorC.hex; // 33+5*i
		*ptr++ = bpmem.combiners[i].alphaC.hex; // 34+5*i
		*ptr++ = bpmem.tevind[i].hex; // 35+5*i
		*ptr++ = bpmem.tevksel[i/2].hex; // 36+5*i
		*ptr++ = bpmem.tevorders[i/2].hex; // 37+5*i
	}

	ptr = &uid->values[113];

	*ptr++ = bpmem.alphaFunc.hex; // 113

	*ptr++ = bpmem.fog.c_proj_fsel.hex; // 114
	*ptr++ = bpmem.fogRange.Base.hex; // 115

	_assert_((ptr - uid->values) == uid->GetNumValues());
}

void ValidatePixelShaderIDs(API_TYPE api, PIXELSHADERUIDSAFE old_id, const std::string& old_code, DSTALPHA_MODE dstAlphaMode, u32 components)
{
	if (!g_ActiveConfig.bEnableShaderDebugging)
		return;

	PIXELSHADERUIDSAFE new_id;
	GetSafePixelShaderId(&new_id, dstAlphaMode, components);

	if (!(old_id == new_id))
	{
		std::string new_code(GeneratePixelShaderCode(dstAlphaMode, api, components));
		if (old_code != new_code)
		{
			_assert_(old_id.GetNumValues() == new_id.GetNumValues());

			char msg[8192];
			char* ptr = msg;
			ptr += sprintf(ptr, "Pixel shader IDs matched but unique IDs did not!\nUnique IDs (old <-> new):\n");
			const int N = new_id.GetNumValues();
			for (int i = 0; i < N/2; ++i)
				ptr += sprintf(ptr, "%02d, %08X  %08X  |  %08X  %08X\n", 2*i, old_id.values[2*i], old_id.values[2*i+1],
																			new_id.values[2*i], new_id.values[2*i+1]);
			if (N % 2)
				ptr += sprintf(ptr, "%02d, %08X  |  %08X\n", N-1, old_id.values[N-1], new_id.values[N-1]);
	
			static int num_failures = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%spsuid_mismatch_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file(szTemp);
			file << msg;
			file << "\n\nOld shader code:\n" << old_code;
			file << "\n\nNew shader code:\n" << new_code;
			file.close();

			PanicAlert("Unique pixel shader ID mismatch!\n\nReport this to the devs, along with the contents of %s.", szTemp);
		}
	}
}

//   old tev->pixelshader notes
//
//   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
//   konstant for this stage (alpha, color) is given by bpmem.tevksel
//   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current chan color
//   according to GXTevColorArg table above
//   output is given by .outreg
//   tevtemp is set according to swapmodetables and

static void WriteStage(char *&p, int n, API_TYPE ApiType);
static void SampleTexture(char *&p, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType);
// static void WriteAlphaCompare(char *&p, int num, int comp);
static void WriteAlphaTest(char *&p, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode);
static void WriteFog(char *&p);

static const char *tevKSelTableC[] = // KCSEL
{
	"1.0f,1.0f,1.0f",    // 1   = 0x00
	"0.875f,0.875f,0.875f", // 7_8 = 0x01
	"0.75f,0.75f,0.75f",    // 3_4 = 0x02
	"0.625f,0.625f,0.625f", // 5_8 = 0x03
	"0.5f,0.5f,0.5f",       // 1_2 = 0x04
	"0.375f,0.375f,0.375f", // 3_8 = 0x05
	"0.25f,0.25f,0.25f",    // 1_4 = 0x06
	"0.125f,0.125f,0.125f", // 1_8 = 0x07
	"ERROR", // 0x08
	"ERROR", // 0x09
	"ERROR", // 0x0a
	"ERROR", // 0x0b
	I_KCOLORS"[0].rgb", // K0 = 0x0C
	I_KCOLORS"[1].rgb", // K1 = 0x0D
	I_KCOLORS"[2].rgb", // K2 = 0x0E
	I_KCOLORS"[3].rgb", // K3 = 0x0F
	I_KCOLORS"[0].rrr", // K0_R = 0x10
	I_KCOLORS"[1].rrr", // K1_R = 0x11
	I_KCOLORS"[2].rrr", // K2_R = 0x12
	I_KCOLORS"[3].rrr", // K3_R = 0x13
	I_KCOLORS"[0].ggg", // K0_G = 0x14
	I_KCOLORS"[1].ggg", // K1_G = 0x15
	I_KCOLORS"[2].ggg", // K2_G = 0x16
	I_KCOLORS"[3].ggg", // K3_G = 0x17
	I_KCOLORS"[0].bbb", // K0_B = 0x18
	I_KCOLORS"[1].bbb", // K1_B = 0x19
	I_KCOLORS"[2].bbb", // K2_B = 0x1A
	I_KCOLORS"[3].bbb", // K3_B = 0x1B
	I_KCOLORS"[0].aaa", // K0_A = 0x1C
	I_KCOLORS"[1].aaa", // K1_A = 0x1D
	I_KCOLORS"[2].aaa", // K2_A = 0x1E
	I_KCOLORS"[3].aaa", // K3_A = 0x1F
};

static const char *tevKSelTableA[] = // KASEL
{
	"1.0f",  // 1   = 0x00
	"0.875f",// 7_8 = 0x01
	"0.75f", // 3_4 = 0x02
	"0.625f",// 5_8 = 0x03
	"0.5f",  // 1_2 = 0x04
	"0.375f",// 3_8 = 0x05
	"0.25f", // 1_4 = 0x06
	"0.125f",// 1_8 = 0x07
	"ERROR", // 0x08
	"ERROR", // 0x09
	"ERROR", // 0x0a
	"ERROR", // 0x0b
	"ERROR", // 0x0c
	"ERROR", // 0x0d
	"ERROR", // 0x0e
	"ERROR", // 0x0f
	I_KCOLORS"[0].r", // K0_R = 0x10
	I_KCOLORS"[1].r", // K1_R = 0x11
	I_KCOLORS"[2].r", // K2_R = 0x12
	I_KCOLORS"[3].r", // K3_R = 0x13
	I_KCOLORS"[0].g", // K0_G = 0x14
	I_KCOLORS"[1].g", // K1_G = 0x15
	I_KCOLORS"[2].g", // K2_G = 0x16
	I_KCOLORS"[3].g", // K3_G = 0x17
	I_KCOLORS"[0].b", // K0_B = 0x18
	I_KCOLORS"[1].b", // K1_B = 0x19
	I_KCOLORS"[2].b", // K2_B = 0x1A
	I_KCOLORS"[3].b", // K3_B = 0x1B
	I_KCOLORS"[0].a", // K0_A = 0x1C
	I_KCOLORS"[1].a", // K1_A = 0x1D
	I_KCOLORS"[2].a", // K2_A = 0x1E
	I_KCOLORS"[3].a", // K3_A = 0x1F
};

static const char *tevScaleTable[] = // CS
{
	"1.0f",  // SCALE_1
	"2.0f",  // SCALE_2
	"4.0f",  // SCALE_4
	"0.5f",  // DIVIDE_2
};

static const char *tevBiasTable[] = // TB
{
	"",       // ZERO,
	"+0.5f",  // ADDHALF,
	"-0.5f",  // SUBHALF,
	"",
};

static const char *tevOpTable[] = { // TEV
	"+",      // TEVOP_ADD = 0,
	"-",      // TEVOP_SUB = 1,
};

static const char *tevCInputTable[] = // CC
{
	"(prev.rgb)",               // CPREV,
	"(prev.aaa)",         // APREV,
	"(c0.rgb)",                 // C0,
	"(c0.aaa)",           // A0,
	"(c1.rgb)",                 // C1,
	"(c1.aaa)",           // A1,
	"(c2.rgb)",                 // C2,
	"(c2.aaa)",           // A2,
	"(textemp.rgb)",            // TEXC,
	"(textemp.aaa)",      // TEXA,
	"(rastemp.rgb)",            // RASC,
	"(rastemp.aaa)",      // RASA,
	"float3(1.0f, 1.0f, 1.0f)",              // ONE
	"float3(0.5f, 0.5f, 0.5f)",                 // HALF
	"(konsttemp.rgb)", //"konsttemp.rgb",        // KONST
	"float3(0.0f, 0.0f, 0.0f)",              // ZERO
	///aded extra values to map clamped values
	"(cprev.rgb)",               // CPREV,
	"(cprev.aaa)",         // APREV,
	"(cc0.rgb)",                 // C0,
	"(cc0.aaa)",           // A0,
	"(cc1.rgb)",                 // C1,
	"(cc1.aaa)",           // A1,
	"(cc2.rgb)",                 // C2,
	"(cc2.aaa)",           // A2,
	"(textemp.rgb)",            // TEXC,
	"(textemp.aaa)",      // TEXA,
	"(crastemp.rgb)",            // RASC,
	"(crastemp.aaa)",      // RASA,
	"float3(1.0f, 1.0f, 1.0f)",              // ONE
	"float3(0.5f, 0.5f, 0.5f)",                 // HALF
	"(ckonsttemp.rgb)", //"konsttemp.rgb",        // KONST
	"float3(0.0f, 0.0f, 0.0f)",              // ZERO
	"PADERROR", "PADERROR", "PADERROR", "PADERROR"
};

static const char *tevAInputTable[] = // CA
{
	"prev",            // APREV,
	"c0",              // A0,
	"c1",              // A1,
	"c2",              // A2,
	"textemp",         // TEXA,
	"rastemp",         // RASA,
	"konsttemp",       // KONST,  (hw1 had quarter)
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // ZERO
	///aded extra values to map clamped values
	"cprev",            // APREV,
	"cc0",              // A0,
	"cc1",              // A1,
	"cc2",              // A2,
	"textemp",         // TEXA,
	"crastemp",         // RASA,
	"ckonsttemp",       // KONST,  (hw1 had quarter)
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // ZERO
	"PADERROR", "PADERROR", "PADERROR", "PADERROR",
	"PADERROR", "PADERROR", "PADERROR", "PADERROR",
};

static const char *tevRasTable[] =
{
	"colors_0",
	"colors_1",
	"ERROR", //2
	"ERROR", //3
	"ERROR", //4
	"alphabump", // use bump alpha
	"(alphabump*(255.0f/248.0f))", //normalized
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // zero
};

//static const char *tevTexFunc[] = { "tex2D", "texRECT" };

static const char *tevCOutputTable[]  = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
static const char *tevAOutputTable[]  = { "prev.a", "c0.a", "c1.a", "c2.a" };
static const char *tevIndAlphaSel[]   = {"", "x", "y", "z"};
//static const char *tevIndAlphaScale[] = {"", "*32", "*16", "*8"};
static const char *tevIndAlphaScale[] = {"*(248.0f/255.0f)", "*(224.0f/255.0f)", "*(240.0f/255.0f)", "*(248.0f/255.0f)"};
static const char *tevIndBiasField[]  = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"}; // indexed by bias
static const char *tevIndBiasAdd[]    = {"-128.0f", "1.0f", "1.0f", "1.0f" }; // indexed by fmt
static const char *tevIndWrapStart[]  = {"0.0f", "256.0f", "128.0f", "64.0f", "32.0f", "16.0f", "0.001f" };
static const char *tevIndFmtScale[]   = {"255.0f", "31.0f", "15.0f", "7.0f" };

#define WRITE p+=sprintf

static char swapModeTable[4][5];

static char text[16384];
static bool DepthTextureEnable;

struct RegisterState
{
	bool ColorNeedOverflowControl;
	bool AlphaNeedOverflowControl;
	bool AuxStored;
};

static RegisterState RegisterStates[4];

static void BuildSwapModeTable()
{
	static const char *swapColors = "rgba";
	for (int i = 0; i < 4; i++)
	{
		swapModeTable[i][0] = swapColors[bpmem.tevksel[i*2].swap1];
		swapModeTable[i][1] = swapColors[bpmem.tevksel[i*2].swap2];
		swapModeTable[i][2] = swapColors[bpmem.tevksel[i*2+1].swap1];
		swapModeTable[i][3] = swapColors[bpmem.tevksel[i*2+1].swap2];
		swapModeTable[i][4] = 0;
	}
}

const char *GeneratePixelShaderCode(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	setlocale(LC_NUMERIC, "C"); // Reset locale for compilation
	text[sizeof(text) - 1] = 0x7C;  // canary

	BuildSwapModeTable(); // Needed for WriteStage
	int numStages = bpmem.genMode.numtevstages + 1;
	int numTexgen = bpmem.genMode.numtexgens;

	char *p = text;
	WRITE(p, "//Pixel Shader for TEV stages\n");
	WRITE(p, "//%i TEV stages, %i texgens, XXX IND stages\n",
		numStages, numTexgen/*, bpmem.genMode.numindstages*/);

	int nIndirectStagesUsed = 0;
	if (bpmem.genMode.numindstages > 0)
	{
		for (int i = 0; i < numStages; ++i)
		{
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				nIndirectStagesUsed |= 1 << bpmem.tevind[i].bt;
		}
	}
	DepthTextureEnable = (bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.zcomploc && bpmem.zmode.testenable && bpmem.zmode.updateenable) || g_ActiveConfig.bEnablePerPixelDepth ;
	// Declare samplers

	if(ApiType != API_D3D11)
	{
		WRITE(p, "uniform sampler2D ");
	}
	else
	{
		WRITE(p, "sampler ");
	}

	bool bfirst = true;
	for (int i = 0; i < 8; ++i)
	{
		WRITE(p, "%s samp%d : register(s%d)", bfirst?"":",", i, i);
		bfirst = false;
	}
	WRITE(p, ";\n");
	if(ApiType == API_D3D11)
	{
		WRITE(p, "Texture2D ");
		bfirst = true;
		for (int i = 0; i < 8; ++i)
		{
			WRITE(p, "%s Tex%d : register(t%d)", bfirst?"":",", i, i);
			bfirst = false;
		}
		WRITE(p, ";\n");
	}

	WRITE(p, "\n");

	WRITE(p, "uniform float4 " I_COLORS"[4] : register(c%d);\n", C_COLORS);
	WRITE(p, "uniform float4 " I_KCOLORS"[4] : register(c%d);\n", C_KCOLORS);
	WRITE(p, "uniform float4 " I_ALPHA"[1] : register(c%d);\n", C_ALPHA);
	WRITE(p, "uniform float4 " I_TEXDIMS"[8] : register(c%d);\n", C_TEXDIMS);
	WRITE(p, "uniform float4 " I_ZBIAS"[2] : register(c%d);\n", C_ZBIAS);
	WRITE(p, "uniform float4 " I_INDTEXSCALE"[2] : register(c%d);\n", C_INDTEXSCALE);
	WRITE(p, "uniform float4 " I_INDTEXMTX"[6] : register(c%d);\n", C_INDTEXMTX);
	WRITE(p, "uniform float4 " I_FOG"[3] : register(c%d);\n", C_FOG);

	if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		WRITE(p,"typedef struct { float4 col; float4 cosatt; float4 distatt; float4 pos; float4 dir; } Light;\n");
		WRITE(p,"typedef struct { Light lights[8]; } s_" I_PLIGHTS";\n");
		WRITE(p, "uniform s_" I_PLIGHTS" " I_PLIGHTS" : register(c%d);\n", C_PLIGHTS);
		WRITE(p, "typedef struct { float4 C0, C1, C2, C3; } s_" I_PMATERIALS";\n");
		WRITE(p, "uniform s_" I_PMATERIALS" " I_PMATERIALS" : register(c%d);\n", C_PMATERIALS);
	}

	WRITE(p, "void main(\n");
	if(ApiType != API_D3D11)
	{
		WRITE(p, "  out float4 ocol0 : COLOR0,%s%s\n  in float4 rawpos : %s,\n",
			dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : COLOR1," : "",
			DepthTextureEnable ? "\n  out float depth : DEPTH," : "",
			ApiType & API_OPENGL ? "WPOS" : ApiType & API_D3D9_SM20 ? "POSITION" : "VPOS");
	}
	else
	{
		WRITE(p, "  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
			dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
			DepthTextureEnable ? "\n  out float depth : SV_Depth," : "");
	}

	WRITE(p, "  in float4 colors_0 : COLOR0,\n");
	WRITE(p, "  in float4 colors_1 : COLOR1");

	// compute window position if needed because binding semantic WPOS is not widely supported
	if (numTexgen < 7)
	{
		for (int i = 0; i < numTexgen; ++i)
			WRITE(p, ",\n  in float3 uv%d : TEXCOORD%d", i, i);
		WRITE(p, ",\n  in float4 clipPos : TEXCOORD%d", numTexgen);
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			WRITE(p, ",\n  in float4 Normal : TEXCOORD%d", numTexgen + 1);
	}
	else
	{
		// wpos is in w of first 4 texcoords
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
		{
			for (int i = 0; i < 8; ++i)
				WRITE(p, ",\n  in float4 uv%d : TEXCOORD%d", i, i);
		}
		else
		{
			for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				WRITE(p, ",\n  in float%d uv%d : TEXCOORD%d", i < 4 ? 4 : 3 , i, i);
		}
	}
	WRITE(p, "        ) {\n");

	int Pretest = AlphaPreTest();
	if(Pretest >= 0 && !DepthTextureEnable)
	{
		if (!Pretest)
		{
			// alpha test will always fail, so restart the shader and just make it an empty function		
			WRITE(p, "ocol0 = 0;\n");
			if(DepthTextureEnable)
				WRITE(p, "depth = 1.f;\n");
			if(dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
					WRITE(p, "ocol1 = 0;\n");
			WRITE(p, "discard;\n");
			if(ApiType != API_D3D11)
				WRITE(p, "return;\n");
		}
		else if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
		{
			WRITE(p, "  ocol0 = " I_ALPHA"[0].aaaa;\n");
		}
		if(!Pretest || dstAlphaMode == DSTALPHA_ALPHA_PASS)
		{
			WRITE(p, "}\n");
			return text;
		}
	}

	WRITE(p, "  float4 c0 = " I_COLORS"[1], c1 = " I_COLORS"[2], c2 = " I_COLORS"[3], prev = float4(0.0f, 0.0f, 0.0f, 0.0f), textemp = float4(0.0f, 0.0f, 0.0f, 0.0f), rastemp = float4(0.0f, 0.0f, 0.0f, 0.0f), konsttemp = float4(0.0f, 0.0f, 0.0f, 0.0f);\n"
			"  float3 comp16 = float3(1.0f, 255.0f, 0.0f), comp24 = float3(1.0f, 255.0f, 255.0f*255.0f);\n"
			"  float4 alphabump=float4(0.0f,0.0f,0.0f,0.0f);\n"
			"  float3 tevcoord=float3(0.0f, 0.0f, 0.0f);\n"
			"  float2 wrappedcoord=float2(0.0f,0.0f), tempcoord=float2(0.0f,0.0f);\n"
			"  float4 cc0=float4(0.0f,0.0f,0.0f,0.0f), cc1=float4(0.0f,0.0f,0.0f,0.0f);\n"
			"  float4 cc2=float4(0.0f,0.0f,0.0f,0.0f), cprev=float4(0.0f,0.0f,0.0f,0.0f);\n"
			"  float4 crastemp=float4(0.0f,0.0f,0.0f,0.0f),ckonsttemp=float4(0.0f,0.0f,0.0f,0.0f);\n\n");

	if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		if (xfregs.numTexGen.numTexGens < 7)
		{
			WRITE(p,"float3 _norm0 = normalize(Normal.xyz);\n\n");
			WRITE(p,"float3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");
		}
		else
		{
			WRITE(p,"  float3 _norm0 = normalize(float3(uv4.w,uv5.w,uv6.w));\n\n");
			WRITE(p,"float3 pos = float3(uv0.w,uv1.w,uv7.w);\n");
		}


		WRITE(p, "float4 mat, lacc;\n"
		"float3 ldir, h;\n"
		"float dist, dist2, attn;\n");

		p = GenerateLightingShader(p, components, I_PMATERIALS, I_PLIGHTS, "colors_", "colors_");
	}

	if (numTexgen < 7)
		WRITE(p, "clipPos = float4(rawpos.x, rawpos.y, clipPos.z, clipPos.w);\n");
	else
		WRITE(p, "float4 clipPos = float4(rawpos.x, rawpos.y, uv2.w, uv3.w);\n");

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		WRITE(p, "float3 uv0 = float3(0.0f, 0.0f, 0.0f);\n");
	}
	else
	{
		for (int i = 0; i < numTexgen; ++i)
		{
			// optional perspective divides
			if (xfregs.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
			{
				WRITE(p, "if (uv%d.z)", i);
				WRITE(p, "	uv%d.xy = uv%d.xy / uv%d.z;\n", i, i, i);
			}

			WRITE(p, "uv%d.xy = uv%d.xy * " I_TEXDIMS"[%d].zw;\n", i, i, i);
		}
	}

	// indirect texture map lookup
	for(u32 i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (nIndirectStagesUsed & (1<<i))
		{
			int texcoord = bpmem.tevindref.getTexCoord(i);

			if (texcoord < numTexgen)
				WRITE(p, "tempcoord = uv%d.xy * " I_INDTEXSCALE"[%d].%s;\n", texcoord, i/2, (i&1)?"zw":"xy");
			else
				WRITE(p, "tempcoord = float2(0.0f, 0.0f);\n");

			char buffer[32];
			sprintf(buffer, "float3 indtex%d", i);
			SampleTexture(p, buffer, "tempcoord", "abg", bpmem.tevindref.getTexMap(i), ApiType);
		}
	}

	RegisterStates[0].AlphaNeedOverflowControl = false;
	RegisterStates[0].ColorNeedOverflowControl = false;
	RegisterStates[0].AuxStored = false;
	for(int i = 1; i < 4; i++)
	{
		RegisterStates[i].AlphaNeedOverflowControl = true;
		RegisterStates[i].ColorNeedOverflowControl = true;
		RegisterStates[i].AuxStored = false;
	}

	for (int i = 0; i < numStages; i++)
		WriteStage(p, i, ApiType); //build the equation for this stage

	if(numStages)
	{
		// The results of the last texenv stage are put onto the screen,
		// regardless of the used destination register
		if(bpmem.combiners[numStages - 1].colorC.dest != 0)
		{
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].AuxStored;
			WRITE(p, "prev.rgb = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevCOutputTable[bpmem.combiners[numStages - 1].colorC.dest]);
			RegisterStates[0].ColorNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl;
		}
		if(bpmem.combiners[numStages - 1].alphaC.dest != 0)
		{
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AuxStored;
			WRITE(p, "prev.a = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevAOutputTable[bpmem.combiners[numStages - 1].alphaC.dest]);
			RegisterStates[0].AlphaNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl;
		}
	}
	// emulation of unsigned 8 overflow when casting if needed
	if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		WRITE(p, "prev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");

	if(Pretest ==  -1)
	{
		WriteAlphaTest(p, ApiType, dstAlphaMode);
	}

	if((bpmem.fog.c_proj_fsel.fsel != 0) || DepthTextureEnable)
	{
		// the screen space depth value = far z + (clip z / clip w) * z range
		WRITE(p, "float zCoord = " I_ZBIAS"[1].x + (clipPos.z / clipPos.w) * " I_ZBIAS"[1].y;\n");
	}

	if (DepthTextureEnable)
	{
		// use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
		if (bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.zcomploc && bpmem.zmode.testenable && bpmem.zmode.updateenable)
		{
			if (bpmem.ztex2.op == ZTEXTURE_ADD)
				WRITE(p, "zCoord = dot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w + zCoord;\n");
			else
				WRITE(p, "zCoord = dot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w;\n");

			// scale to make result from frac correct
			WRITE(p, "zCoord = zCoord * (16777215.0f/16777216.0f);\n");
			WRITE(p, "zCoord = frac(zCoord);\n");
			WRITE(p, "zCoord = zCoord * (16777216.0f/16777215.0f);\n");
		}
		WRITE(p, "depth = zCoord;\n");
	}

	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
		WRITE(p, "  ocol0 = float4(prev.rgb, " I_ALPHA"[0].a);\n");
	else
	{
		WriteFog(p);
		WRITE(p, "  ocol0 = prev;\n");
	}

	// On D3D11, use dual-source color blending to perform dst alpha in a
	// single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		// Colors will be blended against the alpha from ocol1...
		WRITE(p, "  ocol1 = ocol0;\n");
		// ...and the alpha from ocol0 will be written to the framebuffer.
		WRITE(p, "  ocol0.a = " I_ALPHA"[0].a;\n");
	}
	
	WRITE(p, "}\n");
	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");

	setlocale(LC_NUMERIC, ""); // restore locale
	return text;
}



//table with the color compare operations
static const char *TEVCMPColorOPTable[16] =
{
	"float3(0.0f, 0.0f, 0.0f)",//0
	"float3(0.0f, 0.0f, 0.0f)",//1
	"float3(0.0f, 0.0f, 0.0f)",//2
	"float3(0.0f, 0.0f, 0.0f)",//3
	"float3(0.0f, 0.0f, 0.0f)",//4
	"float3(0.0f, 0.0f, 0.0f)",//5
	"float3(0.0f, 0.0f, 0.0f)",//6
	"float3(0.0f, 0.0f, 0.0f)",//7
	"   %s + ((%s.r >= %s.r + (0.25f/255.0f)) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_GT 8
	"   %s + ((abs(%s.r - %s.r) < (0.5f/255.0f)) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_EQ 9
	"   %s + (( dot(%s.rgb, comp16) >= (dot(%s.rgb, comp16) + (0.25f/255.0f))) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_GT 10
	"   %s + (abs(dot(%s.rgb, comp16) - dot(%s.rgb, comp16)) < (0.5f/255.0f) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_EQ 11
	"   %s + (( dot(%s.rgb, comp24) >= (dot(%s.rgb, comp24) + (0.25f/255.0f))) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_GT 12
	"   %s + (abs(dot(%s.rgb, comp24) - dot(%s.rgb, comp24)) < (0.5f/255.0f) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_EQ 13
	"   %s + (max(sign(%s.rgb - %s.rgb - (0.25f/255.0f)), float3(0.0f, 0.0f, 0.0f)) * %s)",//#define TEVCMP_RGB8_GT  14
	"   %s + ((float3(1.0f, 1.0f, 1.0f) - max(sign(abs(%s.rgb - %s.rgb) - (0.5f/255.0f)), float3(0.0f, 0.0f, 0.0f))) * %s)"//#define TEVCMP_RGB8_EQ  15
};

//table with the alpha compare operations
static const char *TEVCMPAlphaOPTable[16] =
{
	"0.0f",//0
	"0.0f",//1
	"0.0f",//2
	"0.0f",//3
	"0.0f",//4
	"0.0f",//5
	"0.0f",//6
	"0.0f",//7
	"   %s.a + ((%s.r >= (%s.r + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_R8_GT 8
	"   %s.a + (abs(%s.r - %s.r) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_R8_EQ 9
	"   %s.a + ((dot(%s.rgb, comp16) >= (dot(%s.rgb, comp16) + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_GR16_GT 10
	"   %s.a + (abs(dot(%s.rgb, comp16) - dot(%s.rgb, comp16)) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_GR16_EQ 11
	"   %s.a + ((dot(%s.rgb, comp24) >= (dot(%s.rgb, comp24) + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_GT 12
	"   %s.a + (abs(dot(%s.rgb, comp24) - dot(%s.rgb, comp24)) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_EQ 13
	"   %s.a + ((%s.a >= (%s.a + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_A8_GT 14
	"   %s.a + (abs(%s.a - %s.a) < (0.5f/255.0f) ? %s.a : 0.0f)"//#define TEVCMP_A8_EQ 15

};


static void WriteStage(char *&p, int n, API_TYPE ApiType)
{
	int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
	bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
	bool bHasIndStage = bpmem.tevind[n].IsActive() && bpmem.tevind[n].bt < bpmem.genMode.numindstages;

	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;

	WRITE(p, "// TEV stage %d\n", n);

	if (bHasIndStage)
	{
		WRITE(p, "// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (bpmem.tevind[n].bs != ITBA_OFF)
		{
			WRITE(p, "alphabump = indtex%d.%s %s;\n",
					bpmem.tevind[n].bt,
					tevIndAlphaSel[bpmem.tevind[n].bs],
					tevIndAlphaScale[bpmem.tevind[n].fmt]);
		}
		// format
		WRITE(p, "float3 indtevcrd%d = indtex%d * %s;\n", n, bpmem.tevind[n].bt, tevIndFmtScale[bpmem.tevind[n].fmt]);

		// bias
		if (bpmem.tevind[n].bias != ITB_NONE )
			WRITE(p, "indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);

		// multiply by offset matrix and scale
		if (bpmem.tevind[n].mid != 0)
		{
			if (bpmem.tevind[n].mid <= 3)
			{
				int mtxidx = 2*(bpmem.tevind[n].mid-1);
				WRITE(p, "float2 indtevtrans%d = float2(dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d), dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d));\n",
					n, mtxidx, n, mtxidx+1, n);
			}
			else if (bpmem.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(bpmem.tevind[n].mid >= 5);
				int mtxidx = 2*(bpmem.tevind[n].mid-5);
				WRITE(p, "float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.xx;\n", n, mtxidx, texcoord, n);
			}
			else if (bpmem.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(bpmem.tevind[n].mid >= 9);
				int mtxidx = 2*(bpmem.tevind[n].mid-9);
				WRITE(p, "float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.yy;\n", n, mtxidx, texcoord, n);
			}
			else
				WRITE(p, "float2 indtevtrans%d = 0;\n", n);
		}
		else
			WRITE(p, "float2 indtevtrans%d = 0;\n", n);

		// ---------
		// Wrapping
		// ---------

		// wrap S
		if (bpmem.tevind[n].sw == ITW_OFF)
			WRITE(p, "wrappedcoord.x = uv%d.x;\n", texcoord);
		else if (bpmem.tevind[n].sw == ITW_0)
			WRITE(p, "wrappedcoord.x = 0.0f;\n");
		else
			WRITE(p, "wrappedcoord.x = fmod( uv%d.x, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].sw]);

		// wrap T
		if (bpmem.tevind[n].tw == ITW_OFF)
			WRITE(p, "wrappedcoord.y = uv%d.y;\n", texcoord);
		else if (bpmem.tevind[n].tw == ITW_0)
			WRITE(p, "wrappedcoord.y = 0.0f;\n");
		else
			WRITE(p, "wrappedcoord.y = fmod( uv%d.y, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].tw]);

		if (bpmem.tevind[n].fb_addprev) // add previous tevcoord
			WRITE(p, "tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			WRITE(p, "tevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);
	}

	TevStageCombiner::ColorCombiner &cc = bpmem.combiners[n].colorC;
	TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[n].alphaC;

	if(cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC
		|| cc.b == TEVCOLORARG_RASA || cc.b == TEVCOLORARG_RASC
		|| cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC
		|| cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC
		|| ac.a == TEVALPHAARG_RASA || ac.b == TEVALPHAARG_RASA
		|| ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
	{
		char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
		WRITE(p, "rastemp = %s.%s;\n", tevRasTable[bpmem.tevorders[n / 2].getColorChan(n & 1)], rasswap);
		WRITE(p, "crastemp = frac(rastemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
	}


	if (bpmem.tevorders[n/2].getEnable(n&1))
	{
		if(!bHasIndStage)
		{
			// calc tevcord
			if(bHasTexCoord)
				WRITE(p, "tevcoord.xy = uv%d.xy;\n", texcoord);
			else
				WRITE(p, "tevcoord.xy = float2(0.0f, 0.0f);\n");
		}

		char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];
		int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
		SampleTexture(p, "textemp", "tevcoord", texswap, texmap, ApiType);
	}
	else
		WRITE(p, "textemp = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");


	if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST
		|| ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST || ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
	{
		int kc = bpmem.tevksel[n / 2].getKC(n & 1);
		int ka = bpmem.tevksel[n / 2].getKA(n & 1);
		WRITE(p, "konsttemp = float4(%s, %s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);
		if(kc > 7 || ka > 7)
		{
			WRITE(p, "ckonsttemp = frac(konsttemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
		}
		else
		{
			WRITE(p, "ckonsttemp = konsttemp;\n");
		}
	}

	if(cc.a == TEVCOLORARG_CPREV || cc.a == TEVCOLORARG_APREV
		|| cc.b == TEVCOLORARG_CPREV || cc.b == TEVCOLORARG_APREV
		|| cc.c == TEVCOLORARG_CPREV || cc.c == TEVCOLORARG_APREV
		|| ac.a == TEVALPHAARG_APREV || ac.b == TEVALPHAARG_APREV || ac.c == TEVALPHAARG_APREV)
	{
		if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		{
			WRITE(p, "cprev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[0].AlphaNeedOverflowControl = false;
			RegisterStates[0].ColorNeedOverflowControl = false;
		}
		else
		{
			WRITE(p, "cprev = prev;\n");
		}
		RegisterStates[0].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C0 || cc.a == TEVCOLORARG_A0
	|| cc.b == TEVCOLORARG_C0 || cc.b == TEVCOLORARG_A0
	|| cc.c == TEVCOLORARG_C0 || cc.c == TEVCOLORARG_A0
	|| ac.a == TEVALPHAARG_A0 || ac.b == TEVALPHAARG_A0 || ac.c == TEVALPHAARG_A0)
	{
		if(RegisterStates[1].AlphaNeedOverflowControl || RegisterStates[1].ColorNeedOverflowControl)
		{
			WRITE(p, "cc0 = frac(c0 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[1].AlphaNeedOverflowControl = false;
			RegisterStates[1].ColorNeedOverflowControl = false;
		}
		else
		{
			WRITE(p, "cc0 = c0;\n");
		}
		RegisterStates[1].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C1 || cc.a == TEVCOLORARG_A1
	|| cc.b == TEVCOLORARG_C1 || cc.b == TEVCOLORARG_A1
	|| cc.c == TEVCOLORARG_C1 || cc.c == TEVCOLORARG_A1
	|| ac.a == TEVALPHAARG_A1 || ac.b == TEVALPHAARG_A1 || ac.c == TEVALPHAARG_A1)
	{
		if(RegisterStates[2].AlphaNeedOverflowControl || RegisterStates[2].ColorNeedOverflowControl)
		{
			WRITE(p, "cc1 = frac(c1 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[2].AlphaNeedOverflowControl = false;
			RegisterStates[2].ColorNeedOverflowControl = false;
		}
		else
		{
			WRITE(p, "cc1 = c1;\n");
		}
		RegisterStates[2].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C2 || cc.a == TEVCOLORARG_A2
	|| cc.b == TEVCOLORARG_C2 || cc.b == TEVCOLORARG_A2
	|| cc.c == TEVCOLORARG_C2 || cc.c == TEVCOLORARG_A2
	|| ac.a == TEVALPHAARG_A2 || ac.b == TEVALPHAARG_A2 || ac.c == TEVALPHAARG_A2)
	{
		if(RegisterStates[3].AlphaNeedOverflowControl || RegisterStates[3].ColorNeedOverflowControl)
		{
			WRITE(p, "cc2 = frac(c2 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[3].AlphaNeedOverflowControl = false;
			RegisterStates[3].ColorNeedOverflowControl = false;
		}
		else
		{
			WRITE(p, "cc2 = c2;\n");
		}
		RegisterStates[3].AuxStored = true;
	}

	RegisterStates[cc.dest].ColorNeedOverflowControl = (cc.clamp == 0);
	RegisterStates[cc.dest].AuxStored = false;

	// combine the color channel
	WRITE(p, "// color combine\n");
	if (cc.clamp)
		WRITE(p, "%s = saturate(", tevCOutputTable[cc.dest]);
	else
		WRITE(p, "%s = ", tevCOutputTable[cc.dest]);

	// combine the color channel
	if (cc.bias != TevBias_COMPARE) // if not compare
	{
		//normal color combiner goes here
		if (cc.shift > TEVSCALE_1)
			WRITE(p, "%s*(", tevScaleTable[cc.shift]);

		if(!(cc.d == TEVCOLORARG_ZERO && cc.op == TEVOP_ADD))
			WRITE(p, "%s%s", tevCInputTable[cc.d], tevOpTable[cc.op]);

		if (cc.a == cc.b)
			WRITE(p, "%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ZERO)
			WRITE(p, "%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ONE)
			WRITE(p, "%s", tevCInputTable[cc.b + 16]);
		else if (cc.a == TEVCOLORARG_ZERO)
			WRITE(p, "%s*%s", tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);
		else if (cc.b == TEVCOLORARG_ZERO)
			WRITE(p, "%s*(float3(1.0f, 1.0f, 1.0f)-%s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.c + 16]);
		else
			WRITE(p, "lerp(%s, %s, %s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);

		WRITE(p, "%s", tevBiasTable[cc.bias]);

		if (cc.shift > TEVSCALE_1)
			WRITE(p, ")");
	}
	else
	{
		int cmp = (cc.shift<<1)|cc.op|8; // comparemode stored here
		WRITE(p, TEVCMPColorOPTable[cmp],//lookup the function from the op table
				tevCInputTable[cc.d],
				tevCInputTable[cc.a + 16],
				tevCInputTable[cc.b + 16],
				tevCInputTable[cc.c + 16]);
	}
	if (cc.clamp)
		WRITE(p, ")");
	WRITE(p,";\n");

	RegisterStates[ac.dest].AlphaNeedOverflowControl = (ac.clamp == 0);
	RegisterStates[ac.dest].AuxStored = false;

	// combine the alpha channel
	WRITE(p, "// alpha combine\n");
	if (ac.clamp)
		WRITE(p, "%s = saturate(", tevAOutputTable[ac.dest]);
	else
		WRITE(p, "%s = ", tevAOutputTable[ac.dest]);

	if (ac.bias != TevBias_COMPARE) // if not compare
	{
		//normal alpha combiner goes here
		if (ac.shift > TEVSCALE_1)
			WRITE(p, "%s*(", tevScaleTable[ac.shift]);

		if(!(ac.d == TEVALPHAARG_ZERO && ac.op == TEVOP_ADD))
			WRITE(p, "%s.a%s", tevAInputTable[ac.d], tevOpTable[ac.op]);

		if (ac.a == ac.b)
			WRITE(p, "%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.c == TEVALPHAARG_ZERO)
			WRITE(p, "%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.a == TEVALPHAARG_ZERO)
			WRITE(p, "%s.a*%s.a", tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);
		else if (ac.b == TEVALPHAARG_ZERO)
			WRITE(p, "%s.a*(1.0f-%s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.c + 8]);
		else
			WRITE(p, "lerp(%s.a, %s.a, %s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);

		WRITE(p, "%s",tevBiasTable[ac.bias]);

		if (ac.shift>0)
			WRITE(p, ")");

	}
	else
	{
		//compare alpha combiner goes here
		int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here
		WRITE(p, TEVCMPAlphaOPTable[cmp],
				tevAInputTable[ac.d],
				tevAInputTable[ac.a + 8],
				tevAInputTable[ac.b + 8],
				tevAInputTable[ac.c + 8]);
	}
	if (ac.clamp)
		WRITE(p, ")");
	WRITE(p, ";\n\n");
	WRITE(p, "// TEV done\n");
}

void SampleTexture(char *&p, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType)
{
	if (ApiType == API_D3D11)
		WRITE(p, "%s=Tex%d.Sample(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, texmap,texmap, texcoords, texmap, texswap);
	else
		WRITE(p, "%s=tex2D(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, texmap, texcoords, texmap, texswap);
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",									//ALPHACMP_NEVER 0
	"(prev.a <= %s - (0.25f/255.0f))",			//ALPHACMP_LESS 1
	"(abs( prev.a - %s ) < (0.5f/255.0f))",		//ALPHACMP_EQUAL 2
	"(prev.a < %s + (0.25f/255.0f))",			//ALPHACMP_LEQUAL 3
	"(prev.a >= %s + (0.25f/255.0f))",			//ALPHACMP_GREATER 4
	"(abs( prev.a - %s ) >= (0.5f/255.0f))",	//ALPHACMP_NEQUAL 5
	"(prev.a > %s - (0.25f/255.0f))",			//ALPHACMP_GEQUAL 6
	"(true)"									//ALPHACMP_ALWAYS 7
};

static const char *tevAlphaFunclogicTable[] =
{
	" && ", // and
	" || ", // or
	" != ", // xor
	" == "  // xnor
};
static int AlphaPreTest()
{
	u32 op = bpmem.alphaFunc.logic;
	u32 comp[2] = {bpmem.alphaFunc.comp0, bpmem.alphaFunc.comp1};

	// First kill all the simple cases
	switch(op)
	{
	case 0: // AND
		if (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) return true;
		if (comp[0] == ALPHACMP_NEVER || comp[1] == ALPHACMP_NEVER) return false;
		break;
	case 1: // OR
		if (comp[0] == ALPHACMP_ALWAYS || comp[1] == ALPHACMP_ALWAYS) return true;
		if (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER)return false;
		break;
	case 2: // XOR
		if ((comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_NEVER) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_ALWAYS))
			return true;
		if ((comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER))
			return false;
		break;
	case 3: // XNOR
		if ((comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_NEVER) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_ALWAYS))
			return false;
		if ((comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER))
			return true;
		break;
	default: PanicAlert("bad logic for alpha test? %08x", op);
	}
	return -1;
}


static void WriteAlphaTest(char *&p, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA"[0].r",
		I_ALPHA"[0].g"
	};	

	// using discard then return works the same in cg and dx9 but not in dx11
	WRITE(p, "if(!( ");

	int compindex = bpmem.alphaFunc.comp0 % 8;
	WRITE(p, tevAlphaFuncsTable[compindex],alphaRef[0]);//lookup the first component from the alpha function table

	WRITE(p, "%s", tevAlphaFunclogicTable[bpmem.alphaFunc.logic % 4]);//lookup the logic op

	compindex = bpmem.alphaFunc.comp1 % 8;
	WRITE(p, tevAlphaFuncsTable[compindex],alphaRef[1]);//lookup the second component from the alpha function table
	WRITE(p, ")) {\n");

	WRITE(p, "ocol0 = 0;\n");
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		WRITE(p, "ocol1 = 0;\n");
	if (DepthTextureEnable)
		WRITE(p, "depth = 1.f;\n");

	// HAXX: zcomploc is a way to control whether depth test is done before
	// or after texturing and alpha test. PC GPU does depth test before texturing ONLY if depth value is
	// not updated during shader execution.
	// We implement "depth test before texturing" by discarding the fragment
	// when the alpha test fail. This is not a correct implementation because
	// even if the depth test fails the fragment could be alpha blended.
	// this implemnetation is a trick to  keep speed.
	// the correct, but slow, way to implement a correct zComploc is :
	// 1 - if zcomplock is enebled make a first pass, with color channel write disabled updating only 
	// depth channel.
	// 2 - in the next pass disable depth chanel update, but proccess the color data normally
	// this way is the only CORRECT way to emulate perfectly the zcomplock behaviour
	if (!(bpmem.zcontrol.zcomploc && bpmem.zmode.updateenable))
	{
		WRITE(p, "discard;\n");
		if (ApiType != API_D3D11)
			WRITE(p, "return;\n");
	}

	WRITE(p, "}\n");
	
}

static const char *tevFogFuncsTable[] =
{
	"",																//No Fog
	"",																//?
	"",																//Linear
	"",																//?
	"  fog = 1.0f - pow(2.0f, -8.0f * fog);\n",						//exp
	"  fog = 1.0f - pow(2.0f, -8.0f * fog * fog);\n",					//exp2
	"  fog = pow(2.0f, -8.0f * (1.0f - fog));\n",						//backward exp
	"  fog = 1.0f - fog;\n   fog = pow(2.0f, -8.0f * fog * fog);\n"	//backward exp2
};

static void WriteFog(char *&p)
{
	if(bpmem.fog.c_proj_fsel.fsel == 0)return;//no Fog

	if (bpmem.fog.c_proj_fsel.proj == 0)
	{
		// perspective
		// ze = A/(B - (Zs >> B_SHF)
		WRITE (p, "  float ze = " I_FOG"[1].x / (" I_FOG"[1].y - (zCoord / " I_FOG"[1].w));\n");
	}
	else
	{
		// orthographic
		// ze = a*Zs	(here, no B_SHF)
		WRITE (p, "  float ze = " I_FOG"[1].x * zCoord;\n");
	}

	// x_adjust = sqrt((x-center)^2 + k^2)/k
	// ze *= x_adjust
	//this is complitly teorical as the real hard seems to use a table intead of calculate the values.
	if(bpmem.fogRange.Base.Enabled)
	{
		WRITE (p, "  float x_adjust = (2.0f * (clipPos.x / " I_FOG"[2].y)) - 1.0f - " I_FOG"[2].x;\n");
		WRITE (p, "  x_adjust = sqrt(x_adjust * x_adjust + " I_FOG"[2].z * " I_FOG"[2].z) / " I_FOG"[2].z;\n");
		WRITE (p, "  ze *= x_adjust;\n");
	}

	WRITE (p, "  float fog = saturate(ze - " I_FOG"[1].z);\n");

	if(bpmem.fog.c_proj_fsel.fsel > 3)
	{
		WRITE(p, "%s", tevFogFuncsTable[bpmem.fog.c_proj_fsel.fsel]);
	}
	else
	{
		if(bpmem.fog.c_proj_fsel.fsel != 2)
			WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpmem.fog.c_proj_fsel.fsel);
	}

	WRITE(p, "  prev.rgb = lerp(prev.rgb," I_FOG"[0].rgb,fog);\n");


}
