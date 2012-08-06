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


//   old tev->pixelshader notes
//
//   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
//   konstant for this stage (alpha, color) is given by bpmem.tevksel
//   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current chan color
//   according to GXTevColorArg table above
//   output is given by .outreg
//   tevtemp is set according to swapmodetables and

template<class T, GenOutput type> static void WriteStage(char *&p, int n, API_TYPE ApiType);
template<class T, GenOutput type> static void SampleTexture(T& out, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType);
// static void WriteAlphaCompare(char *&p, int num, int comp);
template<class T, GenOutput type> static void WriteAlphaTest(char *&p, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode);
template<class T, GenOutput type> static void WriteFog(char *&p);

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
	"ERROR1", // 0x08
	"ERROR2", // 0x09
	"ERROR3", // 0x0a
	"ERROR4", // 0x0b
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
	"ERROR5", // 0x08
	"ERROR6", // 0x09
	"ERROR7", // 0x0a
	"ERROR8", // 0x0b
	"ERROR9", // 0x0c
	"ERROR10", // 0x0d
	"ERROR11", // 0x0e
	"ERROR12", // 0x0f
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
	"PADERROR1", "PADERROR2", "PADERROR3", "PADERROR4"
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
	"PADERROR5", "PADERROR6", "PADERROR7", "PADERROR8",
	"PADERROR9", "PADERROR10", "PADERROR11", "PADERROR12",
};

static const char *tevRasTable[] =
{
	"colors_0",
	"colors_1",
	"ERROR13", //2
	"ERROR14", //3
	"ERROR15", //4
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

static char swapModeTable[4][5];

static char text[16384];

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

template<class T, GenOutput type>
void GeneratePixelShader(T& out, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
#define SetUidField(name, value) if (type == GO_ShaderUid) {out.GetUidData().name = value; };
	if (type == GO_ShaderCode)
	{
		setlocale(LC_NUMERIC, "C"); // Reset locale for compilation
		out.SetBuffer(text);
	}
///	text[sizeof(text) - 1] = 0x7C;  // canary

	/// TODO: Uids!
	BuildSwapModeTable(); // Needed for WriteStage
	unsigned int numStages = bpmem.genMode.numtevstages + 1;
	unsigned int numTexgen = bpmem.genMode.numtexgens;

	char *p = text;
	out.Write("//Pixel Shader for TEV stages\n");
	out.Write("//%i TEV stages, %i texgens, XXX IND stages\n",
		numStages, numTexgen/*, bpmem.genMode.numindstages*/);

	SetUidField(components, components);
	SetUidField(dstAlphaMode, dstAlphaMode);

	SetUidField(genMode.numindstages, bpmem.genMode.numindstages);
	SetUidField(genMode.numtevstages, bpmem.genMode.numtevstages);
	SetUidField(genMode.numtexgens, bpmem.genMode.numtexgens);

	// Declare samplers
	if(ApiType != API_D3D11)
	{
		out.Write("uniform sampler2D ");
	}
	else
	{
		out.Write("sampler ");
	}

	bool bfirst = true;
	for (int i = 0; i < 8; ++i)
	{
		out.Write("%s samp%d : register(s%d)", bfirst?"":",", i, i);
		bfirst = false;
	}
	out.Write(";\n");
	if(ApiType == API_D3D11)
	{
		out.Write("Texture2D ");
		bfirst = true;
		for (int i = 0; i < 8; ++i)
		{
			out.Write("%s Tex%d : register(t%d)", bfirst?"":",", i, i);
			bfirst = false;
		}
		out.Write(";\n");
	}

	out.Write("\n");

	out.Write("uniform float4 " I_COLORS"[4] : register(c%d);\n", C_COLORS);
	out.Write("uniform float4 " I_KCOLORS"[4] : register(c%d);\n", C_KCOLORS);
	out.Write("uniform float4 " I_ALPHA"[1] : register(c%d);\n", C_ALPHA);
	out.Write("uniform float4 " I_TEXDIMS"[8] : register(c%d);\n", C_TEXDIMS);
	out.Write("uniform float4 " I_ZBIAS"[2] : register(c%d);\n", C_ZBIAS);
	out.Write("uniform float4 " I_INDTEXSCALE"[2] : register(c%d);\n", C_INDTEXSCALE);
	out.Write("uniform float4 " I_INDTEXMTX"[6] : register(c%d);\n", C_INDTEXMTX);
	out.Write("uniform float4 " I_FOG"[3] : register(c%d);\n", C_FOG);

	if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		out.Write("typedef struct { float4 col; float4 cosatt; float4 distatt; float4 pos; float4 dir; } Light;\n");
		out.Write("typedef struct { Light lights[8]; } s_" I_PLIGHTS";\n");
		out.Write("uniform s_" I_PLIGHTS" " I_PLIGHTS" : register(c%d);\n", C_PLIGHTS);
		out.Write("typedef struct { float4 C0, C1, C2, C3; } s_" I_PMATERIALS";\n");
		out.Write("uniform s_" I_PMATERIALS" " I_PMATERIALS" : register(c%d);\n", C_PMATERIALS);
	}

	out.Write("void main(\n");
	if(ApiType != API_D3D11)
	{
		out.Write("  out float4 ocol0 : COLOR0,%s%s\n  in float4 rawpos : %s,\n",
			dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : COLOR1," : "",
			"\n  out float depth : DEPTH,",
			ApiType & API_OPENGL ? "WPOS" : ApiType & API_D3D9_SM20 ? "POSITION" : "VPOS");
	}
	else
	{
		out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
			dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
			"\n  out float depth : SV_Depth,");
	}

	out.Write("  in float4 colors_0 : COLOR0,\n");
	out.Write("  in float4 colors_1 : COLOR1");

	// compute window position if needed because binding semantic WPOS is not widely supported
	if (numTexgen < 7)
	{
		for (unsigned int i = 0; i < numTexgen; ++i)
			out.Write(",\n  in float3 uv%d : TEXCOORD%d", i, i);
		out.Write(",\n  in float4 clipPos : TEXCOORD%d", numTexgen);
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			out.Write(",\n  in float4 Normal : TEXCOORD%d", numTexgen + 1);
	}
	else
	{
		// wpos is in w of first 4 texcoords
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
		{
			for (int i = 0; i < 8; ++i)
				out.Write(",\n  in float4 uv%d : TEXCOORD%d", i, i);
		}
		else
		{
			/// TODO: Set numTexGen used
			for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				out.Write(",\n  in float%d uv%d : TEXCOORD%d", i < 4 ? 4 : 3 , i, i);
		}
	}
	out.Write("        ) {\n");

	out.Write("  float4 c0 = " I_COLORS"[1], c1 = " I_COLORS"[2], c2 = " I_COLORS"[3], prev = float4(0.0f, 0.0f, 0.0f, 0.0f), textemp = float4(0.0f, 0.0f, 0.0f, 0.0f), rastemp = float4(0.0f, 0.0f, 0.0f, 0.0f), konsttemp = float4(0.0f, 0.0f, 0.0f, 0.0f);\n"
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
			out.Write("float3 _norm0 = normalize(Normal.xyz);\n\n");
			out.Write("float3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");
		}
		else
		{
			out.Write("  float3 _norm0 = normalize(float3(uv4.w,uv5.w,uv6.w));\n\n");
			out.Write("float3 pos = float3(uv0.w,uv1.w,uv7.w);\n");
		}


		out.Write("float4 mat, lacc;\n"
		"float3 ldir, h;\n"
		"float dist, dist2, attn;\n");

///		p = GenerateLightingShader(p, components, I_PMATERIALS, I_PLIGHTS, "colors_", "colors_");
	}

	if (numTexgen < 7)
		out.Write("clipPos = float4(rawpos.x, rawpos.y, clipPos.z, clipPos.w);\n");
	else
		out.Write("float4 clipPos = float4(rawpos.x, rawpos.y, uv2.w, uv3.w);\n");

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		out.Write("float3 uv0 = float3(0.0f, 0.0f, 0.0f);\n");
	}
	else
	{
		for (unsigned int i = 0; i < numTexgen; ++i)
		{
			// optional perspective divides
			SetUidField(texMtxInfo[i].projection, xfregs.texMtxInfo[i].projection);
			if (xfregs.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
			{
				out.Write("if (uv%d.z)", i);
				out.Write("	uv%d.xy = uv%d.xy / uv%d.z;\n", i, i, i);
			}

			out.Write("uv%d.xy = uv%d.xy * " I_TEXDIMS"[%d].zw;\n", i, i, i);
		}
	}

	// indirect texture map lookup
	int nIndirectStagesUsed = 0;
	if (bpmem.genMode.numindstages > 0)
	{
		for (unsigned int i = 0; i < numStages; ++i)
		{
			/// Ignoring this for now, handled in WriteStage.
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				nIndirectStagesUsed |= 1 << bpmem.tevind[i].bt;
		}
	}
	SetUidField(nIndirectStagesUsed, nIndirectStagesUsed);
	for(u32 i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (nIndirectStagesUsed & (1<<i))
		{
			unsigned int texcoord = bpmem.tevindref.getTexCoord(i);
			unsigned int texmap = bpmem.tevindref.getTexMap(i);

			if (i == 0)
			{
				SetUidField(tevindref.bc0, texcoord);
				SetUidField(tevindref.bi0, texmap);
			}
			else if (i == 1)
			{
				SetUidField(tevindref.bc1, texcoord);
				SetUidField(tevindref.bi1, texmap);
			}
			else if (i == 2)
			{
				SetUidField(tevindref.bc3, texcoord);
				SetUidField(tevindref.bi2, texmap);
			}
			else
			{
				SetUidField(tevindref.bc4, texcoord);
				SetUidField(tevindref.bi4, texmap);
			}
			if (texcoord < numTexgen)
				out.Write("tempcoord = uv%d.xy * " I_INDTEXSCALE"[%d].%s;\n", texcoord, i/2, (i&1)?"zw":"xy");
			else
				out.Write("tempcoord = float2(0.0f, 0.0f);\n");

			char buffer[32];
			sprintf(buffer, "float3 indtex%d", i);
			SampleTexture<T, type>(out, buffer, "tempcoord", "abg", texmap, ApiType);
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

	for (unsigned int i = 0; i < numStages; i++)
		WriteStage<T, type>(out, i, ApiType); //build the equation for this stage

	if(numStages)
	{
		// The results of the last texenv stage are put onto the screen,
		// regardless of the used destination register
		if(bpmem.combiners[numStages - 1].colorC.dest != 0)
		{
///			SetUidField(combiners[numStages-1].colorC.dest, bpmem.combiners[numStages-1].colorC.dest);
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].AuxStored;
			out.Write("prev.rgb = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevCOutputTable[bpmem.combiners[numStages - 1].colorC.dest]);
			RegisterStates[0].ColorNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl;
		}
		if(bpmem.combiners[numStages - 1].alphaC.dest != 0)
		{
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AuxStored;
			out.Write("prev.a = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevAOutputTable[bpmem.combiners[numStages - 1].alphaC.dest]);
			RegisterStates[0].AlphaNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl;
		}
	}
	// emulation of unsigned 8 overflow when casting if needed
	if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		out.Write("prev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");

	AlphaTest::TEST_RESULT Pretest = bpmem.alpha_test.TestResult();
	if (Pretest == AlphaTest::UNDETERMINED)
		WriteAlphaTest<T, type>(out, ApiType, dstAlphaMode);

	// the screen space depth value = far z + (clip z / clip w) * z range
	out.Write("float zCoord = " I_ZBIAS"[1].x + (clipPos.z / clipPos.w) * " I_ZBIAS"[1].y;\n");

	// Note: depth textures are disabled if early depth test is enabled
	if (bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.early_ztest && bpmem.zmode.testenable)
	{
		// use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
		out.Write("zCoord = dot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w %s;\n",
									(bpmem.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");

		// scale to make result from frac correct
		out.Write("zCoord = zCoord * (16777215.0f/16777216.0f);\n");
		out.Write("zCoord = frac(zCoord);\n");
		out.Write("zCoord = zCoord * (16777216.0f/16777215.0f);\n");
	}
	out.Write("depth = zCoord;\n");

	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
		out.Write("  ocol0 = float4(prev.rgb, " I_ALPHA"[0].a);\n");
	else
	{
		WriteFog<T, type>(out);
		out.Write("  ocol0 = prev;\n");
	}

	// On D3D11, use dual-source color blending to perform dst alpha in a
	// single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		// Colors will be blended against the alpha from ocol1...
		out.Write("  ocol1 = ocol0;\n");
		// ...and the alpha from ocol0 will be written to the framebuffer.
		out.Write("  ocol0.a = " I_ALPHA"[0].a;\n");
	}
	
	out.Write("}\n");
///	if (text[sizeof(text) - 1] != 0x7C)
///		PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");

	setlocale(LC_NUMERIC, ""); // restore locale
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


template<class T, GenOutput type>
static void WriteStage(T& out, int n, API_TYPE ApiType)
{
	int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
	bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
	bool bHasIndStage = bpmem.tevind[n].IsActive() && bpmem.tevind[n].bt < bpmem.genMode.numindstages;

	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;

	out.Write("// TEV stage %d\n", n);

	if (bHasIndStage)
	{
		out.Write("// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (bpmem.tevind[n].bs != ITBA_OFF)
		{
			out.Write("alphabump = indtex%d.%s %s;\n",
					bpmem.tevind[n].bt,
					tevIndAlphaSel[bpmem.tevind[n].bs],
					tevIndAlphaScale[bpmem.tevind[n].fmt]);
		}
		// format
		out.Write("float3 indtevcrd%d = indtex%d * %s;\n", n, bpmem.tevind[n].bt, tevIndFmtScale[bpmem.tevind[n].fmt]);

		// bias
		if (bpmem.tevind[n].bias != ITB_NONE )
			out.Write("indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);

		// multiply by offset matrix and scale
		if (bpmem.tevind[n].mid != 0)
		{
			if (bpmem.tevind[n].mid <= 3)
			{
				int mtxidx = 2*(bpmem.tevind[n].mid-1);
				out.Write("float2 indtevtrans%d = float2(dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d), dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d));\n",
					n, mtxidx, n, mtxidx+1, n);
			}
			else if (bpmem.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(bpmem.tevind[n].mid >= 5);
				int mtxidx = 2*(bpmem.tevind[n].mid-5);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.xx;\n", n, mtxidx, texcoord, n);
			}
			else if (bpmem.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(bpmem.tevind[n].mid >= 9);
				int mtxidx = 2*(bpmem.tevind[n].mid-9);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.yy;\n", n, mtxidx, texcoord, n);
			}
			else
				out.Write("float2 indtevtrans%d = 0;\n", n);
		}
		else
			out.Write("float2 indtevtrans%d = 0;\n", n);

		// ---------
		// Wrapping
		// ---------

		// wrap S
		if (bpmem.tevind[n].sw == ITW_OFF)
			out.Write("wrappedcoord.x = uv%d.x;\n", texcoord);
		else if (bpmem.tevind[n].sw == ITW_0)
			out.Write("wrappedcoord.x = 0.0f;\n");
		else
			out.Write("wrappedcoord.x = fmod( uv%d.x, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].sw]);

		// wrap T
		if (bpmem.tevind[n].tw == ITW_OFF)
			out.Write("wrappedcoord.y = uv%d.y;\n", texcoord);
		else if (bpmem.tevind[n].tw == ITW_0)
			out.Write("wrappedcoord.y = 0.0f;\n");
		else
			out.Write("wrappedcoord.y = fmod( uv%d.y, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].tw]);

		if (bpmem.tevind[n].fb_addprev) // add previous tevcoord
			out.Write("tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			out.Write("tevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);
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
		out.Write("rastemp = %s.%s;\n", tevRasTable[bpmem.tevorders[n / 2].getColorChan(n & 1)], rasswap);
		out.Write("crastemp = frac(rastemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
	}


	if (bpmem.tevorders[n/2].getEnable(n&1))
	{
		if(!bHasIndStage)
		{
			// calc tevcord
			if(bHasTexCoord)
				out.Write("tevcoord.xy = uv%d.xy;\n", texcoord);
			else
				out.Write("tevcoord.xy = float2(0.0f, 0.0f);\n");
		}

		char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];
		int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
		SampleTexture<T, type>(out, "textemp", "tevcoord", texswap, texmap, ApiType);
	}
	else
		out.Write("textemp = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");


	if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST
		|| ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST || ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
	{
		int kc = bpmem.tevksel[n / 2].getKC(n & 1);
		int ka = bpmem.tevksel[n / 2].getKA(n & 1);
		out.Write("konsttemp = float4(%s, %s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);
		if(kc > 7 || ka > 7)
		{
			out.Write("ckonsttemp = frac(konsttemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
		}
		else
		{
			out.Write("ckonsttemp = konsttemp;\n");
		}
	}

	if(cc.a == TEVCOLORARG_CPREV || cc.a == TEVCOLORARG_APREV
		|| cc.b == TEVCOLORARG_CPREV || cc.b == TEVCOLORARG_APREV
		|| cc.c == TEVCOLORARG_CPREV || cc.c == TEVCOLORARG_APREV
		|| ac.a == TEVALPHAARG_APREV || ac.b == TEVALPHAARG_APREV || ac.c == TEVALPHAARG_APREV)
	{
		if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		{
			out.Write("cprev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[0].AlphaNeedOverflowControl = false;
			RegisterStates[0].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cprev = prev;\n");
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
			out.Write("cc0 = frac(c0 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[1].AlphaNeedOverflowControl = false;
			RegisterStates[1].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc0 = c0;\n");
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
			out.Write("cc1 = frac(c1 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[2].AlphaNeedOverflowControl = false;
			RegisterStates[2].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc1 = c1;\n");
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
			out.Write("cc2 = frac(c2 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[3].AlphaNeedOverflowControl = false;
			RegisterStates[3].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc2 = c2;\n");
		}
		RegisterStates[3].AuxStored = true;
	}

	RegisterStates[cc.dest].ColorNeedOverflowControl = (cc.clamp == 0);
	RegisterStates[cc.dest].AuxStored = false;

	out.Write("// color combine\n");
	if (cc.clamp)
		out.Write("%s = saturate(", tevCOutputTable[cc.dest]);
	else
		out.Write("%s = ", tevCOutputTable[cc.dest]);

	// combine the color channel
	if (cc.bias != TevBias_COMPARE) // if not compare
	{
		//normal color combiner goes here
		if (cc.shift > TEVSCALE_1)
			out.Write("%s*(", tevScaleTable[cc.shift]);

		if(!(cc.d == TEVCOLORARG_ZERO && cc.op == TEVOP_ADD))
			out.Write("%s%s", tevCInputTable[cc.d], tevOpTable[cc.op]);

		if (cc.a == cc.b)
			out.Write("%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ZERO)
			out.Write("%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ONE)
			out.Write("%s", tevCInputTable[cc.b + 16]);
		else if (cc.a == TEVCOLORARG_ZERO)
			out.Write("%s*%s", tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);
		else if (cc.b == TEVCOLORARG_ZERO)
			out.Write("%s*(float3(1.0f, 1.0f, 1.0f)-%s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.c + 16]);
		else
			out.Write("lerp(%s, %s, %s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);

		out.Write("%s", tevBiasTable[cc.bias]);

		if (cc.shift > TEVSCALE_1)
			out.Write(")");
	}
	else
	{
		int cmp = (cc.shift<<1)|cc.op|8; // comparemode stored here
		out.Write(TEVCMPColorOPTable[cmp],//lookup the function from the op table
				tevCInputTable[cc.d],
				tevCInputTable[cc.a + 16],
				tevCInputTable[cc.b + 16],
				tevCInputTable[cc.c + 16]);
	}
	if (cc.clamp)
		out.Write(")");
	out.Write(";\n");

	RegisterStates[ac.dest].AlphaNeedOverflowControl = (ac.clamp == 0);
	RegisterStates[ac.dest].AuxStored = false;

	out.Write("// alpha combine\n");
	if (ac.clamp)
		out.Write("%s = saturate(", tevAOutputTable[ac.dest]);
	else
		out.Write("%s = ", tevAOutputTable[ac.dest]);

	if (ac.bias != TevBias_COMPARE) // if not compare
	{
		//normal alpha combiner goes here
		if (ac.shift > TEVSCALE_1)
			out.Write("%s*(", tevScaleTable[ac.shift]);

		if(!(ac.d == TEVALPHAARG_ZERO && ac.op == TEVOP_ADD))
			out.Write("%s.a%s", tevAInputTable[ac.d], tevOpTable[ac.op]);

		if (ac.a == ac.b)
			out.Write("%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.c == TEVALPHAARG_ZERO)
			out.Write("%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.a == TEVALPHAARG_ZERO)
			out.Write("%s.a*%s.a", tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);
		else if (ac.b == TEVALPHAARG_ZERO)
			out.Write("%s.a*(1.0f-%s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.c + 8]);
		else
			out.Write("lerp(%s.a, %s.a, %s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);

		out.Write("%s",tevBiasTable[ac.bias]);

		if (ac.shift>0)
			out.Write(")");

	}
	else
	{
		//compare alpha combiner goes here
		int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here
		out.Write(TEVCMPAlphaOPTable[cmp],
				tevAInputTable[ac.d],
				tevAInputTable[ac.a + 8],
				tevAInputTable[ac.b + 8],
				tevAInputTable[ac.c + 8]);
	}
	if (ac.clamp)
		out.Write(")");
	out.Write(";\n\n");
	out.Write("// TEV done\n");
}

template<class T, GenOutput type>
void SampleTexture(T& out, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType)
{
	if (ApiType == API_D3D11)
		out.Write("%s=Tex%d.Sample(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, texmap,texmap, texcoords, texmap, texswap);
	else
		out.Write("%s=tex2D(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, texmap, texcoords, texmap, texswap);
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",									//ALPHACMP_NEVER 0, TODO: Not safe?
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

template<class T, GenOutput type>
static void WriteAlphaTest(T& out, API_TYPE ApiType, DSTALPHA_MODE dstAlphaMode)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA"[0].r",
		I_ALPHA"[0].g"
	};	

	// using discard then return works the same in cg and dx9 but not in dx11
	out.Write("if(!( ");

	int compindex = bpmem.alpha_test.comp0;
	out.Write(tevAlphaFuncsTable[compindex],alphaRef[0]);//lookup the first component from the alpha function table

	out.Write("%s", tevAlphaFunclogicTable[bpmem.alpha_test.logic]);//lookup the logic op

	compindex = bpmem.alpha_test.comp1;
	out.Write(tevAlphaFuncsTable[compindex],alphaRef[1]);//lookup the second component from the alpha function table
	out.Write(")) {\n");

	out.Write("ocol0 = 0;\n");
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		out.Write("ocol1 = 0;\n");
	out.Write("depth = 1.f;\n");

	// HAXX: zcomploc (aka early_ztest) is a way to control whether depth test is done before
	// or after texturing and alpha test. PC GPUs have no way to support this
	// feature properly as of 2012: depth buffer and depth test are not
	// programmable and the depth test is always done after texturing.
	// Most importantly, PC GPUs do not allow writing to the z buffer without
	// writing a color value (unless color writing is disabled altogether).
	// We implement "depth test before texturing" by discarding the fragment
	// when the alpha test fail. This is not a correct implementation because
	// even if the depth test fails the fragment could be alpha blended, but
	// we don't have a choice.
	if (!(bpmem.zcontrol.early_ztest && bpmem.zmode.updateenable))
	{
		out.Write("discard;\n");
		if (ApiType != API_D3D11)
			out.Write("return;\n");
	}

	out.Write("}\n");
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

template<class T, GenOutput type>
static void WriteFog(T& out)
{
	if(bpmem.fog.c_proj_fsel.fsel == 0)
		return; //no Fog

	if (bpmem.fog.c_proj_fsel.proj == 0)
	{
		// perspective
		// ze = A/(B - (Zs >> B_SHF)
		out.Write("  float ze = " I_FOG"[1].x / (" I_FOG"[1].y - (zCoord / " I_FOG"[1].w));\n");
	}
	else
	{
		// orthographic
		// ze = a*Zs	(here, no B_SHF)
		out.Write("  float ze = " I_FOG"[1].x * zCoord;\n");
	}

	// x_adjust = sqrt((x-center)^2 + k^2)/k
	// ze *= x_adjust
	// this is completely theoretical as the real hardware seems to use a table intead of calculating the values.
	if(bpmem.fogRange.Base.Enabled)
	{
		out.Write("  float x_adjust = (2.0f * (clipPos.x / " I_FOG"[2].y)) - 1.0f - " I_FOG"[2].x;\n");
		out.Write("  x_adjust = sqrt(x_adjust * x_adjust + " I_FOG"[2].z * " I_FOG"[2].z) / " I_FOG"[2].z;\n");
		out.Write("  ze *= x_adjust;\n");
	}

	out.Write("float fog = saturate(ze - " I_FOG"[1].z);\n");

	if(bpmem.fog.c_proj_fsel.fsel > 3)
	{
		out.Write("%s", tevFogFuncsTable[bpmem.fog.c_proj_fsel.fsel]);
	}
	else
	{
		if(bpmem.fog.c_proj_fsel.fsel != 2)
			WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpmem.fog.c_proj_fsel.fsel);
	}

	out.Write("  prev.rgb = lerp(prev.rgb," I_FOG"[0].rgb,fog);\n");
}

void GetPixelShaderId(PixelShaderUid& object, DSTALPHA_MODE dst_alpha_mode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderUid, GO_ShaderUid>(object, dst_alpha_mode, ApiType, components);
}

void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dst_alpha_mode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderCode, GO_ShaderCode>(object, dst_alpha_mode, ApiType, components);
}
