// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <stdio.h>
#include <cmath>
#include <assert.h>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "LightingShaderGen.h"
#include "PixelShaderGen.h"
#include "XFMemory.h"  // for texture projection mode
#include "BPMemory.h"
#include "VideoConfig.h"
#include "NativeVertexFormat.h"


#define CONST_31_BY_255 "(31.0f/255.0f)"
#define CONST_63_BY_255 "(63.0f/255.0f)"
#define CONST_95_BY_255 "(95.0f/255.0f)"
#define CONST_127_BY_255 "(127.0f/255.0f)"
#define CONST_128_BY_255 "(128.0f/255.0f)"
#define CONST_159_BY_255 "(159.0f/255.0f)"
#define CONST_191_BY_255 "(191.0f/255.0f)"
#define CONST_223_BY_255 "(223.0f/255.0f)"

#define FLOAT3_CONST(const) const"," const"," const

static const char *tevKSelTableC[] = // KCSEL
{
	"1.0f,1.0f,1.0f",       // 1   = 0x00
	FLOAT3_CONST(CONST_223_BY_255), // 7_8 = 0x01
	FLOAT3_CONST(CONST_191_BY_255),    // 3_4 = 0x02
	FLOAT3_CONST(CONST_159_BY_255), // 5_8 = 0x03
	FLOAT3_CONST(CONST_127_BY_255),       // 1_2 = 0x04
	FLOAT3_CONST(CONST_95_BY_255), // 3_8 = 0x05
	FLOAT3_CONST(CONST_63_BY_255),    // 1_4 = 0x06
	FLOAT3_CONST(CONST_31_BY_255), // 1_8 = 0x07
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
	CONST_223_BY_255,// 7_8 = 0x01
	CONST_191_BY_255, // 3_4 = 0x02
	CONST_159_BY_255,// 5_8 = 0x03
	CONST_127_BY_255,  // 1_2 = 0x04
	CONST_95_BY_255,// 3_8 = 0x05
	CONST_63_BY_255, // 1_4 = 0x06
	CONST_31_BY_255,// 1_8 = 0x07
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
	"+" CONST_128_BY_255,  // ADDHALF
	"-" CONST_128_BY_255,  // SUBHALF
	"",
};

static const char *tevOpTable[] = { // TEV
	"+",      // TEVOP_ADD = 0,
	"-",      // TEVOP_SUB = 1,
};

static const char *tevCInputTable[16] = // CC
{
	"(prev.rgb)",         // CPREV,
	"(prev.aaa)",         // APREV,
	"(c0.rgb)",           // C0,
	"(c0.aaa)",           // A0,
	"(c1.rgb)",           // C1,
	"(c1.aaa)",           // A1,
	"(c2.rgb)",           // C2,
	"(c2.aaa)",           // A2,
	"(textemp.rgb)",      // TEXC,
	"(textemp.aaa)",      // TEXA,
	"(rastemp.rgb)",      // RASC,
	"(rastemp.aaa)",      // RASA,
	"float3(1.0f, 1.0f, 1.0f)",              // ONE
	"float3(0.5f, 0.5f, 0.5f)",          // HALF, TODO:Using CONST_127_BY_255 here breaks Simpsons...
	"(konsttemp.rgb)", //"konsttemp.rgb",    // KONST
	"float3(0.0f, 0.0f, 0.0f)",              // ZERO
};

static const char *tevAInputTable[8] = // CA
{
	"prev",            // APREV,
	"c0",              // A0,
	"c1",              // A1,
	"c2",              // A2,
	"textemp",         // TEXA,
	"rastemp",         // RASA,
	"konsttemp",       // KONST,  (hw1 had quarter)
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // ZERO
};

static const char *tevRasTable[] =
{
	"colors_0",
	"colors_1",
	"ERROR13", //2
	"ERROR14", //3
	"ERROR15", //4
	"float4(alphabump,alphabump,alphabump,alphabump)", // use bump alpha
	"(FIX_PRECISION_U8(float4(alphabump,alphabump,alphabump,alphabump)*(255.0f/248.0f)))", // normalized, TODO: Verify!
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // zero
};

static const char *tevCOutputTable[]  = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
static const char *tevAOutputTable[]  = { "prev.a", "c0.a", "c1.a", "c2.a" };

static char swapModeTable[4][5];

static char text[16384];

static void BuildSwapModeTable()
{
	static const char *swapColors = "rgba";
	for (int i = 0; i < 4; i++)
	{
		swapModeTable[i][0] = swapColors[bpmem.tevksel[i*2].swap1];
		swapModeTable[i][1] = swapColors[bpmem.tevksel[i*2].swap2];
		swapModeTable[i][2] = swapColors[bpmem.tevksel[i*2+1].swap1];
		swapModeTable[i][3] = swapColors[bpmem.tevksel[i*2+1].swap2];
		swapModeTable[i][4] = '\0';
	}
}

template<class T> static void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, API_TYPE ApiType);
template<class T> static void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType);
template<class T> static void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth);
template<class T> static void WriteFog(T& out, pixel_shader_uid_data& uid_data);

template<class T>
static void GeneratePixelShader(T& out, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	pixel_shader_uid_data dummy_data;
	pixel_shader_uid_data& uid_data = (&out.template GetUidData<pixel_shader_uid_data>() != NULL)
										? out.template GetUidData<pixel_shader_uid_data>() : dummy_data;

	out.SetBuffer(text);
#ifndef ANDROID
	locale_t locale;
	locale_t old_locale;
	if (out.GetBuffer() != NULL)
	{
		locale = newlocale(LC_NUMERIC_MASK, "C", NULL); // New locale for compilation
		old_locale = uselocale(locale); // Apply the locale for this thread
	}
#endif

	text[sizeof(text) - 1] = 0x7C;  // canary

	unsigned int numStages = bpmem.genMode.numtevstages + 1;
	unsigned int numTexgen = bpmem.genMode.numtexgens;

	const bool forced_early_z = g_ActiveConfig.backend_info.bSupportsEarlyZ && bpmem.zcontrol.early_ztest && (g_ActiveConfig.bFastDepthCalc || bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED);
	const bool per_pixel_depth = (bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.early_ztest && bpmem.zmode.testenable) || (!g_ActiveConfig.bFastDepthCalc && !forced_early_z);

	out.Write("//Pixel Shader for TEV stages\n");
	out.Write("//%i TEV stages, %i texgens, %i IND stages\n",
		numStages, numTexgen, bpmem.genMode.numindstages);

	uid_data.dstAlphaMode = dstAlphaMode;
	uid_data.genMode_numindstages = bpmem.genMode.numindstages;
	uid_data.genMode_numtevstages = bpmem.genMode.numtevstages;
	uid_data.genMode_numtexgens = bpmem.genMode.numtexgens;

	if (ApiType == API_OPENGL)
	{
		// Fmod implementation gleaned from Nvidia
		// At http://http.developer.nvidia.com/Cg/fmod.html
		out.Write("float fmod( float x, float y )\n");
		out.Write("{\n");
		out.Write("\tfloat z = fract( abs( x / y) ) * abs( y );\n");
		out.Write("\treturn (x < 0.0) ? -z : z;\n");
		out.Write("}\n");

		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("uniform sampler2D samp%d;\n", i);
	}
	else
	{
		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("%s samp%d : register(s%d);\n", (ApiType == API_D3D11) ? "sampler" : "uniform sampler2D", i, i);

		if (ApiType == API_D3D11)
		{
			out.Write("\n");
			for (int i = 0; i < 8; ++i)
			{
				out.Write("Texture2D Tex%d : register(t%d);\n", i, i);
			}
		}
	}
	out.Write("\n");

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		out.Write("layout(std140) uniform PSBlock {\n");

	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_COLORS, "float4", I_COLORS"[4]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_KCOLORS, "float4", I_KCOLORS"[4]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_ALPHA, "float4", I_ALPHA"[1]");  // TODO: Why is this an array...-.-
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_TEXDIMS, "float4", I_TEXDIMS"[8]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_ZBIAS, "float4", I_ZBIAS"[2]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_INDTEXSCALE, "float4", I_INDTEXSCALE"[2]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_INDTEXMTX, "float4", I_INDTEXMTX"[6]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_FOG, "float4", I_FOG"[3]");

	// For pixel lighting - TODO: Should only be defined when per pixel lighting is enabled!
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_PLIGHTS, "float4", I_PLIGHTS"[40]");
	DeclareUniform(out, ApiType, g_ActiveConfig.backend_info.bSupportsGLSLUBO, C_PMATERIALS, "float4", I_PMATERIALS"[4]");

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		out.Write("};\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("COLOROUT(ocol0)\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("COLOROUT(ocol1)\n");

		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");

		out.Write("VARYIN float4 colors_02;\n");
		out.Write("VARYIN float4 colors_12;\n");
		
		// compute window position if needed because binding semantic WPOS is not widely supported
		// Let's set up attributes
		if (xfregs.numTexGen.numTexGens < 7)
		{
			for (int i = 0; i < 8; ++i)
			{
				out.Write("VARYIN float3 uv%d_2;\n", i);
			}
			out.Write("VARYIN float4 clipPos_2;\n");
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				out.Write("VARYIN float4 Normal_2;\n");
			}
		}
		else
		{
			// wpos is in w of first 4 texcoords
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
				{
					out.Write("VARYIN float4 uv%d_2;\n", i);
				}
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				{
					out.Write("VARYIN float%d uv%d_2;\n", i < 4 ? 4 : 3 , i);
				}
			}
			out.Write("float4 clipPos;\n");
		}
		
		if (forced_early_z)
		{
			// HACK: This doesn't force the driver to write to depth buffer if alpha test fails.
			// It just allows it, but it seems that all drivers do.
			out.Write("layout(early_fragment_tests) in;\n");
		}
		
		out.Write("void main()\n{\n");
	}
	else
	{
		out.Write("void main(\n");
		if(ApiType != API_D3D11)
		{
			out.Write("  out float4 ocol0 : COLOR0,%s%s\n  in float4 rawpos : %s,\n",
				dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : COLOR1," : "",
				per_pixel_depth ? "\n  out float depth : DEPTH," : "",
				ApiType & API_D3D9_SM20 ? "POSITION" : "VPOS");
		}
		else
		{
			out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
				dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
				per_pixel_depth ? "\n  out float depth : SV_Depth," : "");
		}

		// "centroid" attribute is only supported by D3D11
		const char* optCentroid = (ApiType == API_D3D11 ? "centroid" : "");

		out.Write("  in %s float4 colors_0 : COLOR0,\n", optCentroid);
		out.Write("  in %s float4 colors_1 : COLOR1", optCentroid);

		// compute window position if needed because binding semantic WPOS is not widely supported
		if (numTexgen < 7)
		{
			for (unsigned int i = 0; i < numTexgen; ++i)
				out.Write(",\n  in %s float3 uv%d : TEXCOORD%d", optCentroid, i, i);
			out.Write(",\n  in %s float4 clipPos : TEXCOORD%d", optCentroid, numTexgen);
			if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
				out.Write(",\n  in %s float4 Normal : TEXCOORD%d", optCentroid, numTexgen + 1);
			out.Write("        ) {\n");
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
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
					out.Write(",\n  in float%d uv%d : TEXCOORD%d", i < 4 ? 4 : 3 , i, i);
			}
			out.Write("        ) {\n");
			out.Write("\tfloat4 clipPos = float4(0.0f, 0.0f, 0.0f, 0.0f);");
		}
	}

	out.Write("  float4 c0 = " I_COLORS"[1], c1 = " I_COLORS"[2], c2 = " I_COLORS"[3], prev = float4(0.0f, 0.0f, 0.0f, 0.0f), textemp = float4(0.0f, 0.0f, 0.0f, 0.0f);\n"
			"  float alphabump=0.0f;\n"
			"  float3 tevcoord=float3(0.0f, 0.0f, 0.0f);\n"
			"  float2 wrappedcoord=float2(0.0f,0.0f), tempcoord=float2(0.0f,0.0f);\n\n");

	if (ApiType == API_OPENGL)
	{
		// On Mali, global variables must be initialized as constants.
		// This is why we initialize these variables locally instead.
		out.Write("float4 rawpos = gl_FragCoord;\n");
		out.Write("float4 colors_0 = colors_02;\n");
		out.Write("float4 colors_1 = colors_12;\n");
		// compute window position if needed because binding semantic WPOS is not widely supported
		// Let's set up attributes
		if (xfregs.numTexGen.numTexGens < 7)
		{
			if(numTexgen)
			{
			for (int i = 0; i < 8; ++i)
			{
				out.Write("float3 uv%d = uv%d_2;\n", i, i);
			}
			}
			out.Write("float4 clipPos = clipPos_2;\n");
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				out.Write("float4 Normal = Normal_2;\n");
			}
		}
		else
		{
			// wpos is in w of first 4 texcoords
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
				{
					out.Write("float4 uv%d = uv%d_2;\n", i, i);
				}
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				{
					out.Write("float%d uv%d = uv%d_2;\n", i < 4 ? 4 : 3 , i, i);
				}
			}
		}
	}

	if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		uid_data.xfregs_numTexGen_numTexGens = xfregs.numTexGen.numTexGens;
		if (xfregs.numTexGen.numTexGens < 7)
		{
			out.Write("\tfloat3 _norm0 = normalize(Normal.xyz);\n\n");
			out.Write("\tfloat3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");
		}
		else
		{
			out.Write("\tfloat3 _norm0 = normalize(float3(uv4.w,uv5.w,uv6.w));\n\n");
			out.Write("\tfloat3 pos = float3(uv0.w,uv1.w,uv7.w);\n");
		}

		out.Write("\tfloat4 mat, lacc;\n"
				"\tfloat3 ldir, h;\n"
				"\tfloat dist, dist2, attn;\n");

		out.SetConstantsUsed(C_PLIGHTS, C_PLIGHTS+39); // TODO: Can be optimized further
		out.SetConstantsUsed(C_PMATERIALS, C_PMATERIALS+3);
		uid_data.components = components;
		GenerateLightingShader<T>(out, uid_data.lighting, components, I_PMATERIALS, I_PLIGHTS, "colors_", "colors_");
	}

	if (numTexgen < 7)
		out.Write("\tclipPos = float4(rawpos.x, rawpos.y, clipPos.z, clipPos.w);\n");
	else
		out.Write("\tclipPos = float4(rawpos.x, rawpos.y, uv2.w, uv3.w);\n");

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		out.Write("\tfloat3 uv0 = float3(0.0f, 0.0f, 0.0f);\n");
	}
	else
	{
		out.SetConstantsUsed(C_TEXDIMS, C_TEXDIMS+numTexgen-1);
		for (unsigned int i = 0; i < numTexgen; ++i)
		{
			// optional perspective divides
			uid_data.texMtxInfo_n_projection |= xfregs.texMtxInfo[i].projection << i;
			if (xfregs.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
			{
				out.Write("\tif (uv%d.z != 0.0f)", i);
				out.Write("\t\tuv%d.xy = uv%d.xy / uv%d.z;\n", i, i, i);
			}

			out.Write("uv%d.xy = uv%d.xy * " I_TEXDIMS"[%d].zw;\n", i, i, i);
		}
	}

	// helper macros for emulating tev register bitness
	out.Write("#define FIX_PRECISION_U8(x) (round((x) * 255.0f) / 255.0f)\n");
	out.Write("#define CHECK_OVERFLOW_U8(x) (frac((x) * (255.0f/256.0f)) * (256.0f/255.0f))\n");
	out.Write("#define AS_UNORM8(x) FIX_PRECISION_U8(CHECK_OVERFLOW_U8(x))\n");
	out.Write("#define MASK_U8(x, mask) FIX_PRECISION_U8(x*mask/255.0f)\n");
	out.Write("#define FIX_PRECISION_U16(x) (round((x) * 65535.0f) / 65535.0f)\n");
	out.Write("#define CHECK_OVERFLOW_U16(x) (frac((x) * (65535.0f/65536.0f)) * (65536.0f/65535.0f))\n");
	out.Write("#define AS_UNORM16(x) FIX_PRECISION_U16(CHECK_OVERFLOW_U16(x))\n");
	out.Write("#define FIX_PRECISION_U24(x) (round((x) * 16777215.0f) / 16777215.0f)\n");
	out.Write("#define CHECK_OVERFLOW_U24(x) (frac((x) * (16777215.0f/16777216.0f)) * (16777216.0f/16777215.0f))\n");
	out.Write("#define AS_UNORM24(x) FIX_PRECISION_U24(CHECK_OVERFLOW_U24(x))\n");

	// indirect texture map lookup
	int nIndirectStagesUsed = 0;
	if (bpmem.genMode.numindstages > 0)
	{
		for (unsigned int i = 0; i < numStages; ++i)
		{
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				nIndirectStagesUsed |= 1 << bpmem.tevind[i].bt;
		}
	}

	uid_data.nIndirectStagesUsed = nIndirectStagesUsed;
	for (u32 i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (nIndirectStagesUsed & (1 << i))
		{
			unsigned int texcoord = bpmem.tevindref.getTexCoord(i);
			unsigned int texmap = bpmem.tevindref.getTexMap(i);

			uid_data.SetTevindrefValues(i, texcoord, texmap);
			if (texcoord < numTexgen)
			{
				out.SetConstantsUsed(C_INDTEXSCALE+i/2,C_INDTEXSCALE+i/2);
				out.Write("\ttempcoord = uv%d.xy * " I_INDTEXSCALE"[%d].%s;\n", texcoord, i/2, (i&1)?"zw":"xy");
			}
			else
				out.Write("\ttempcoord = float2(0.0f, 0.0f);\n");

			out.Write("float3 indtex%d = ", i);
			SampleTexture<T>(out, "tempcoord", "abg", texmap, ApiType);
		}
	}

	// Uid fields for BuildSwapModeTable are set in WriteStage
	BuildSwapModeTable();
	for (unsigned int i = 0; i < numStages; i++)
		WriteStage<T>(out, uid_data, i, ApiType); // build the equation for this stage


#define MY_STRUCT_OFFSET(str,elem) ((u32)((u64)&(str).elem-(u64)&(str)))
	bool enable_pl = g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting;
	uid_data.num_values = (enable_pl) ? sizeof(uid_data) : MY_STRUCT_OFFSET(uid_data,stagehash[numStages]);


	// The results of the last texenv stage are put onto the screen,
	// regardless of the used destination register
	if(bpmem.combiners[numStages - 1].colorC.dest != 0)
	{
		out.Write("\tprev.rgb = %s;\n", tevCOutputTable[bpmem.combiners[numStages - 1].colorC.dest]);
	}
	if(bpmem.combiners[numStages - 1].alphaC.dest != 0)
	{
		out.Write("\tprev.a = %s;\n", tevAOutputTable[bpmem.combiners[numStages - 1].alphaC.dest]);
	}
	// Final tev output is U8
	out.Write("\tprev = AS_UNORM8(prev);\n");

	AlphaTest::TEST_RESULT Pretest = bpmem.alpha_test.TestResult();
	uid_data.Pretest = Pretest;
	if (Pretest == AlphaTest::UNDETERMINED)
		WriteAlphaTest<T>(out, uid_data, ApiType, dstAlphaMode, per_pixel_depth);


	// D3D9 doesn't support readback of depth in pixel shader, so we always have to calculate it again.
	// This shouldn't be a performance issue as the written depth is usually still from perspective division
	// but this isn't true for z-textures, so there will be depth issues between enabled and disabled z-textures fragments
	if ((ApiType == API_OPENGL || ApiType == API_D3D11) && g_ActiveConfig.bFastDepthCalc)
		out.Write("float zCoord = rawpos.z;\n");
	else
	{
		out.SetConstantsUsed(C_ZBIAS+1, C_ZBIAS+1);
		// the screen space depth value = far z + (clip z / clip w) * z range
		out.Write("float zCoord = " I_ZBIAS"[1].x + (clipPos.z / clipPos.w) * " I_ZBIAS"[1].y;\n");
	}

	// depth texture can safely be ignored if the result won't be written to the depth buffer (early_ztest) and isn't used for fog either
	const bool skip_ztexture = !per_pixel_depth && !bpmem.fog.c_proj_fsel.fsel;

	uid_data.ztex_op = bpmem.ztex2.op;
	uid_data.per_pixel_depth = per_pixel_depth;
	uid_data.forced_early_z = forced_early_z;
	uid_data.fast_depth_calc = g_ActiveConfig.bFastDepthCalc;
	uid_data.early_ztest = bpmem.zcontrol.early_ztest;
	uid_data.fog_fsel = bpmem.fog.c_proj_fsel.fsel;

	// Note: z-textures are not written to depth buffer if early depth test is used
	if (per_pixel_depth && bpmem.zcontrol.early_ztest)
		out.Write("depth = zCoord;\n");

	// Note: depth texture output is only written to depth buffer if late depth test is used
	// theoretical final depth value is used for fog calculation, though, so we have to emulate ztextures anyway
	if (bpmem.ztex2.op != ZTEXTURE_DISABLE && !skip_ztexture)
	{
		// use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
		out.SetConstantsUsed(C_ZBIAS, C_ZBIAS+1);
		out.Write("zCoord = dot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w %s;\n",
									(bpmem.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");

		// U24 overflow emulation
		out.Write("zCoord = AS_UNORM24(zCoord);\n");
	}

	if (per_pixel_depth && !bpmem.zcontrol.early_ztest)
		out.Write("depth = zCoord;\n");

	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);
		out.Write("\tocol0 = float4(prev.rgb, " I_ALPHA"[0].a);\n");
	}
	else
	{
		WriteFog<T>(out, uid_data);
		out.Write("\tocol0 = prev;\n");
	}

	// Emulate limited color resolution
	uid_data.pixel_format = bpmem.zcontrol.pixel_format;
	if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		out.Write("ocol0 = round(ocol0 * 63.0f) / 63.0f;\n");
	else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		out.Write("ocol0 = round(ocol0 * float4(31.0f,63.0f,31.0f,0.0f)) / float4(31.0f,63.0f,31.0f,1.0f);\n");

	// Use dual-source color blending to perform dst alpha in a single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);
		if(ApiType & API_D3D9)
		{
			// alpha component must be 0 or the shader will not compile (Direct3D 9Ex restriction)
			// Colors will be blended against the color from ocol1 in D3D 9...
			out.Write("\tocol1 = float4(prev.a, prev.a, prev.a, 0.0f);\n");
		}
		else
		{
			// Colors will be blended against the alpha from ocol1...
			out.Write("\tocol1 = prev;\n");
		}
		// ...and the alpha from ocol0 will be written to the framebuffer.
		out.Write("\tocol0.a = " I_ALPHA"[0].a;\n");
	}

	out.Write("}\n");

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");

#ifndef ANDROID
	if (out.GetBuffer() != NULL)
	{
		uselocale(old_locale); // restore locale
		freelocale(locale);
	}
#endif
}



//table with the color compare operations
#define COMP16 "float3(1.0f, 256.0f, 0.0f)"
#define COMP24 "float3(1.0f, 256.0f, 256.0f*256.0f)"
static const char *TEVCMPColorOPTable[8] =
{
	"   %s + ((%s.r > %s.r) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_GT 8
	"   %s + ((%s.r == %s.r) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_EQ 9
	"   %s + ((dot(%s.rgb - %s.rgb, " COMP16") > 0) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_GT 10
	"   %s + ((dot(%s.rgb - %s.rgb, " COMP16") == 0) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_EQ 11
	"   %s + ((dot(%s.rgb - %s.rgb, " COMP24") > 0) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_GT 12
	"   %s + ((dot(%s.rgb - %s.rgb, " COMP24") == 0) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_EQ 13
	"   %s + (max(sign(%s.rgb - %s.rgb), float3(0.0f, 0.0f, 0.0f)) * %s)",//#define TEVCMP_RGB8_GT  14
	"   %s + ((float3(1.0f, 1.0f, 1.0f) - max(sign(abs(%s.rgb - %s.rgb)), float3(0.0f, 0.0f, 0.0f))) * %s)"//#define TEVCMP_RGB8_EQ  15
};

//table with the alpha compare operations
static const char *TEVCMPAlphaOPTable[8] =
{
	"   %s.a + ((%s.r > %s.r) ? %s.a : 0.0f)",//#define TEVCMP_R8_GT 8
	"   %s.a + ((%s.r == %s.r) ? %s.a : 0.0f)",//#define TEVCMP_R8_EQ 9
	"   %s.a + ((dot(%s.rgb - %s.rgb, " COMP16") > 0) ? %s.a : 0.0f)",//#define TEVCMP_GR16_GT 10
	"   %s.a + ((dot(%s.rgb - %s.rgb, " COMP16") == 0) ? %s.a : 0.0f)",//#define TEVCMP_GR16_EQ 11
	"   %s.a + ((dot(%s.rgb - %s.rgb, " COMP24") > 0) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_GT 12
	"   %s.a + ((dot(%s.rgb - %s.rgb, " COMP24") == 0) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_EQ 13
	"   %s.a + ((%s.a > %s.a) ? %s.a : 0.0f)",//#define TEVCMP_A8_GT 14
	"   %s.a + ((%s.a == %s.a) ? %s.a : 0.0f)"//#define TEVCMP_A8_EQ 15
};

// Emulates U8 integer overflow when source value might be bigger than that.
// In this case a temporary variable with the name temp_name will be declared.
// The returned string is the name of the variable that holds the loaded register value.
template<class T>
const char* LoadTevColorInput(T& out, u32 input, const char* temp_name)
{
	// rastemp, textemp and konsttemp are guaranteed to be bitness correct already
	if (input < 8)
		out.Write("float3 %s = AS_UNORM8(%s);\n", temp_name, tevCInputTable[input]);

	return (input < 8) ? temp_name : tevCInputTable[input];
}

template<class T>
const char* LoadTevAlphaInput(T& out, u32 input, const char* temp_name)
{
	if (input < 4)
		out.Write("float4 %s = AS_UNORM8(%s);\n", temp_name, tevAInputTable[input]);

	return (input < 4) ? temp_name : tevAInputTable[input];
}

template<class T>
static void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, API_TYPE ApiType)
{
	int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
	bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
	bool bHasIndStage = bpmem.tevind[n].IsActive() && bpmem.tevind[n].bt < bpmem.genMode.numindstages;
	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;

	out.Write("// TEV stage %d\n", n);
	out.Write("{\n");

	uid_data.stagehash[n].hasindstage = bHasIndStage;
	uid_data.stagehash[n].tevorders_texcoord = texcoord;
	if (bHasIndStage)
	{
		uid_data.stagehash[n].tevind = bpmem.tevind[n].hex & 0x7FFFFF;

		out.Write("// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (bpmem.tevind[n].bs != ITBA_OFF)
		{
			const char *tevIndAlphaSel[] = { "", "x", "y", "z" };
			const char *tevIndAlphaBumpMask[] = { "248.0f", "224.0f", "240.0f", "248.0f" };
			out.Write("alphabump = MASK_U8(indtex%d.%s, %s);\n",
					bpmem.tevind[n].bt,
					tevIndAlphaSel[bpmem.tevind[n].bs],
					tevIndAlphaBumpMask[bpmem.tevind[n].fmt]);
		}
		// format
		const char *tevIndFmtScale[] = { "255.0f", "31.0f", "15.0f", "7.0f" };
		out.Write("float3 indtevcrd%d = indtex%d * %s;\n", n, bpmem.tevind[n].bt, tevIndFmtScale[bpmem.tevind[n].fmt]);

		// bias
		if (bpmem.tevind[n].bias != ITB_NONE )
		{
			const char *tevIndBiasAdd[] = { "-128.0f", "1.0f", "1.0f", "1.0f" }; // indexed by fmt
			const char *tevIndBiasField[]  = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"}; // indexed by bias
			out.Write("indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);
		}

		// multiply by offset matrix and scale
		if (bpmem.tevind[n].mid & 3)
		{
			int mtxidx = 2*((bpmem.tevind[n].mid&0x3)-1);
			if (bpmem.tevind[n].mid <= 3)
			{
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = float2(dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d), dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d));\n",
							n, mtxidx, n, mtxidx+1, n);
			}
			else if (bpmem.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.xx;\n", n, mtxidx, texcoord, n);
			}
			else if (bpmem.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.yy;\n", n, mtxidx, texcoord, n);
			}
			else
			{
				out.Write("float2 indtevtrans%d = float2(0.0f, 0.0f);\n", n);
			}
		}
		else
		{
			out.Write("float2 indtevtrans%d = float2(0.0f, 0.0f);\n", n);
		}

		// ---------
		// Wrapping
		// ---------
		const char *tevIndWrapStart[]  = { "0.0f", "256.0f", "128.0f", "64.0f", "32.0f", "16.0f", "0.0f" };

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

	uid_data.stagehash[n].cc = cc.hex & 0xFFFFFF;
	uid_data.stagehash[n].ac = ac.hex & 0xFFFFF0; // Storing rswap and tswap later

	if(cc.InputUsed(TEVCOLORARG_RASC) || cc.InputUsed(TEVCOLORARG_RASA) || ac.InputUsed(TEVALPHAARG_RASA))
	{
		const int i = bpmem.combiners[n].alphaC.rswap;
		uid_data.stagehash[n].ac |= bpmem.combiners[n].alphaC.rswap;
		uid_data.stagehash[n].tevksel_swap1a = bpmem.tevksel[i*2].swap1;
		uid_data.stagehash[n].tevksel_swap2a = bpmem.tevksel[i*2].swap2;
		uid_data.stagehash[n].tevksel_swap1b = bpmem.tevksel[i*2+1].swap1;
		uid_data.stagehash[n].tevksel_swap2b = bpmem.tevksel[i*2+1].swap2;
		uid_data.stagehash[n].tevorders_colorchan = bpmem.tevorders[n / 2].getColorChan(n & 1);

		char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
		out.Write("float4 rastemp = %s.%s;\n", tevRasTable[bpmem.tevorders[n / 2].getColorChan(n & 1)], rasswap);
	}

	uid_data.stagehash[n].tevorders_enable = bpmem.tevorders[n / 2].getEnable(n & 1);
	if (bpmem.tevorders[n/2].getEnable(n&1))
	{
		if (!bHasIndStage)
		{
			// calc tevcord
			if(bHasTexCoord)
				out.Write("tevcoord.xy = uv%d.xy;\n", texcoord);
			else
				out.Write("tevcoord.xy = float2(0.0f, 0.0f);\n");
		}

		const int i = bpmem.combiners[n].alphaC.tswap;
		uid_data.stagehash[n].ac |= bpmem.combiners[n].alphaC.tswap << 2;
		uid_data.stagehash[n].tevksel_swap1c = bpmem.tevksel[i*2].swap1;
		uid_data.stagehash[n].tevksel_swap2c = bpmem.tevksel[i*2].swap2;
		uid_data.stagehash[n].tevksel_swap1d = bpmem.tevksel[i*2+1].swap1;
		uid_data.stagehash[n].tevksel_swap2d = bpmem.tevksel[i*2+1].swap2;

		uid_data.stagehash[n].tevorders_texmap= bpmem.tevorders[n/2].getTexMap(n&1);

		char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];
		int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
		uid_data.SetTevindrefTexmap(i, texmap);

		out.Write("textemp = ");
		SampleTexture<T>(out, "tevcoord", texswap, texmap, ApiType);
	}
	else
	{
		out.Write("textemp = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");
	}


	if(cc.InputUsed(TEVCOLORARG_KONST) || ac.InputUsed(TEVALPHAARG_KONST))
	{
		int kc = bpmem.tevksel[n / 2].getKC(n & 1);
		int ka = bpmem.tevksel[n / 2].getKA(n & 1);
		uid_data.stagehash[n].tevksel_kc = kc;
		uid_data.stagehash[n].tevksel_ka = ka;
		out.Write("float4 konsttemp = float4(%s, %s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);
		if (kc > 7)
			out.SetConstantsUsed(C_KCOLORS+((kc-0xc)%4),C_KCOLORS+((kc-0xc)%4));
		if (ka > 7)
			out.SetConstantsUsed(C_KCOLORS+((ka-0xc)%4),C_KCOLORS+((ka-0xc)%4));
	}

	if(cc.InputUsed(TEVCOLORARG_C0) || cc.InputUsed(TEVCOLORARG_A0) || ac.InputUsed(TEVALPHAARG_A0))
		out.SetConstantsUsed(C_COLORS+1,C_COLORS+1);

	if(cc.InputUsed(TEVCOLORARG_C1) || cc.InputUsed(TEVCOLORARG_A1) || ac.InputUsed(TEVALPHAARG_A1))
		out.SetConstantsUsed(C_COLORS+2,C_COLORS+2);

	if(cc.InputUsed(TEVCOLORARG_C2) || cc.InputUsed(TEVCOLORARG_A2) || ac.InputUsed(TEVALPHAARG_A2))
		out.SetConstantsUsed(C_COLORS+3,C_COLORS+3);

	if (cc.dest >= GX_TEVREG0 && cc.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+cc.dest, C_COLORS+cc.dest);

	if (ac.dest >= GX_TEVREG0 && ac.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+ac.dest, C_COLORS+ac.dest);

	// Loading prev or C0/C1/C2 into the 8 bit tev registers (A,B and C) requires integer overflow emulation
	const char* input_ca = LoadTevColorInput(out, cc.a, "input_cc_a");
	const char* input_cb = LoadTevColorInput(out, cc.b, "input_cc_b");
	const char* input_cc = LoadTevColorInput(out, cc.c, "input_cc_c");

	// TODO: Do we need to delay initialization until color combiner has been processed?
	const char* input_aa = LoadTevAlphaInput(out, ac.a, "input_ac_a");
	const char* input_ab = LoadTevAlphaInput(out, ac.b, "input_ac_b");
	const char* input_ac = LoadTevAlphaInput(out, ac.c, "input_ac_c");

	out.Write("// color combine\n");
	out.Write("%s = ", tevCOutputTable[cc.dest]);
	out.Write("clamp(");

	// combine the color channel
	if (cc.bias != TevBias_COMPARE) // if not compare
	{
		// normal color combiner goes here
		out.Write("FIX_PRECISION_U8(%s * (%s %s FIX_PRECISION_U8(lerp(%s, %s, %s) %s)))",
					tevScaleTable[cc.shift], tevCInputTable[cc.d], tevOpTable[cc.op],
					input_ca, input_cb, input_cc, tevBiasTable[cc.bias]);
	}
	else
	{
		// compare color combiner goes here
		out.Write(TEVCMPColorOPTable[(cc.shift<<1) | cc.op],
				tevCInputTable[cc.d], input_ca, input_cb, input_cc);
	}
	if (cc.clamp)
		out.Write(", 0.0f, 1.0f)"); // clamp to U8 range (0..255)
	else
		out.Write(", -1024.0f/255.0f, 1023.0f/255.0f)"); // clamp to S11 range (-1024..1023)
	out.Write(";\n");

	out.Write("// alpha combine\n");
	out.Write("%s = ", tevAOutputTable[ac.dest]);
	out.Write("clamp(");

	if (ac.bias != TevBias_COMPARE) // if not compare
	{
		// normal alpha combiner goes here
		out.Write("FIX_PRECISION_U8(%s * (%s.a %s FIX_PRECISION_U8(lerp(%s.a, %s.a, %s.a) %s)))",
					tevScaleTable[ac.shift], tevAInputTable[ac.d], tevOpTable[ac.op],
					input_aa, input_ab, input_ac, tevBiasTable[ac.bias]);
	}
	else
	{
		// compare alpha combiner goes here
		out.Write(TEVCMPAlphaOPTable[(ac.shift<<1) | ac.op],
				tevAInputTable[ac.d], input_aa, input_ab, input_ac);
	}
	if (ac.clamp)
		out.Write(", 0.0f, 1.0f)"); // clamp to U8 range (0..255)
	else
		out.Write(", -1024.0f/255.0f, 1023.0f/255.0f)"); // clamp to S11 range (-1024..1023)
	out.Write(";\n\n");
	out.Write("// End of TEV stage %d\n", n);
	out.Write("}\n");
}

template<class T>
void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType)
{
	out.SetConstantsUsed(C_TEXDIMS+texmap,C_TEXDIMS+texmap);
	
	if (ApiType == API_D3D11)
		out.Write("Tex%d.Sample(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", texmap,texmap, texcoords, texmap, texswap);
	else
		out.Write("%s(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", ApiType == API_OPENGL ? "texture" : "tex2D", texmap, texcoords, texmap, texswap);
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",			// NEVER
	"(prev.a < %s)",	// LESS
	"(prev.a == %s)",	// EQUAL
	"(prev.a <= %s)",	// LEQUAL
	"(prev.a > %s)",	// GREATER
	"(prev.a != %s)",	// NEQUAL
	"(prev.a >= %s)",	// GEQUAL
	"(true)"			// ALWAYS
};

static const char *tevAlphaFunclogicTable[] =
{
	" && ", // and
	" || ", // or
	" != ", // xor
	" == "  // xnor
};

template<class T>
static void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, API_TYPE ApiType, DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA"[0].r",
		I_ALPHA"[0].g"
	};

	out.SetConstantsUsed(C_ALPHA, C_ALPHA);

	// using discard then return works the same in cg and dx9 but not in dx11
	out.Write("\tif(!( ");

	uid_data.alpha_test_comp0 = bpmem.alpha_test.comp0;
	uid_data.alpha_test_comp1 = bpmem.alpha_test.comp1;
	uid_data.alpha_test_logic = bpmem.alpha_test.logic;

	// Lookup the first component from the alpha function table
	int compindex = bpmem.alpha_test.comp0;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[0]);

	out.Write("%s", tevAlphaFunclogicTable[bpmem.alpha_test.logic]);//lookup the logic op

	// Lookup the second component from the alpha function table
	compindex = bpmem.alpha_test.comp1;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[1]);
	out.Write(")) {\n");

	out.Write("\t\tocol0 = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		out.Write("\t\tocol1 = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
	if(per_pixel_depth)
		out.Write("\t\tdepth = 1.f;\n");

	// HAXX: zcomploc (aka early_ztest) is a way to control whether depth test is done before
	// or after texturing and alpha test. PC graphics APIs have no way to support this
	// feature properly as of 2012: Depth buffer and depth test are not
	// programmable and the depth test is always done after texturing.
	// Most importantly, they do not allow writing to the z-buffer without
	// writing a color value (unless color writing is disabled altogether).
	// We implement "depth test before texturing" by disabling alpha test when early-z is in use.
	// It seems to be less buggy than not to update the depth buffer if alpha test fails,
	// but both ways wouldn't be accurate.
	
	// OpenGL 4.2 has a flag which allows the driver to still update the depth buffer 
	// if alpha test fails. The driver doesn't have to, but I assume they all do because
	// it's the much faster code path for the GPU.
	uid_data.alpha_test_use_zcomploc_hack = bpmem.zcontrol.early_ztest && bpmem.zmode.updateenable && !g_ActiveConfig.backend_info.bSupportsEarlyZ;
	if (!uid_data.alpha_test_use_zcomploc_hack)
	{
		out.Write("\t\tdiscard;\n");
		if (ApiType != API_D3D11)
			out.Write("\t\treturn;\n");
	}

	out.Write("}\n");
}

static const char *tevFogFuncsTable[] =
{
	"",																// No Fog
	"",																// ?
	"",																// Linear
	"",																// ?
	"\tfog = 1.0f - pow(2.0f, -8.0f * fog);\n",						// exp
	"\tfog = 1.0f - pow(2.0f, -8.0f * fog * fog);\n",				// exp2
	"\tfog = pow(2.0f, -8.0f * (1.0f - fog));\n",					// backward exp
	"\tfog = 1.0f - fog;\n   fog = pow(2.0f, -8.0f * fog * fog);\n"	// backward exp2
};

template<class T>
static void WriteFog(T& out, pixel_shader_uid_data& uid_data)
{
	uid_data.fog_fsel = bpmem.fog.c_proj_fsel.fsel;
	if(bpmem.fog.c_proj_fsel.fsel == 0)
		return; // no Fog

	uid_data.fog_proj = bpmem.fog.c_proj_fsel.proj;

	out.SetConstantsUsed(C_FOG, C_FOG+1);
	if (bpmem.fog.c_proj_fsel.proj == 0)
	{
		// perspective
		// ze = A/(B - (Zs >> B_SHF)
		out.Write("\tfloat ze = " I_FOG"[1].x / (" I_FOG"[1].y - (zCoord / " I_FOG"[1].w));\n");
	}
	else
	{
		// orthographic
		// ze = a*Zs	(here, no B_SHF)
		out.Write("\tfloat ze = " I_FOG"[1].x * zCoord;\n");
	}

	// x_adjust = sqrt((x-center)^2 + k^2)/k
	// ze *= x_adjust
	// this is completely theoretical as the real hardware seems to use a table intead of calculating the values.
	uid_data.fog_RangeBaseEnabled = bpmem.fogRange.Base.Enabled;
	if (bpmem.fogRange.Base.Enabled)
	{
		out.SetConstantsUsed(C_FOG+2, C_FOG+2);
		out.Write("\tfloat x_adjust = (2.0f * (clipPos.x / " I_FOG"[2].y)) - 1.0f - " I_FOG"[2].x;\n");
		out.Write("\tx_adjust = sqrt(x_adjust * x_adjust + " I_FOG"[2].z * " I_FOG"[2].z) / " I_FOG"[2].z;\n");
		out.Write("\tze *= x_adjust;\n");
	}

	out.Write("\tfloat fog = clamp(ze - " I_FOG"[1].z, 0.0f, 1.0f);\n");

	if (bpmem.fog.c_proj_fsel.fsel > 3)
	{
		out.Write("%s", tevFogFuncsTable[bpmem.fog.c_proj_fsel.fsel]);
	}
	else
	{
		if (bpmem.fog.c_proj_fsel.fsel != 2)
			WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpmem.fog.c_proj_fsel.fsel);
	}

	out.Write("\tfog = FIX_PRECISION_U8(fog);\n");
	out.Write("\tprev.rgb = lerp(prev.rgb, " I_FOG"[0].rgb, fog);\n");
}

void GetPixelShaderUid(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderUid>(object, dstAlphaMode, ApiType, components);
}

void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderCode>(object, dstAlphaMode, ApiType, components);
}

void GetPixelShaderConstantProfile(PixelShaderConstantProfile& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderConstantProfile>(object, dstAlphaMode, ApiType, components);
}

