// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"  // for texture projection mode


static const char *tevKSelTableC[] =
{
	"255,255,255",  // 1   = 0x00
	"223,223,223",  // 7_8 = 0x01
	"191,191,191",  // 3_4 = 0x02
	"159,159,159",  // 5_8 = 0x03
	"128,128,128",  // 1_2 = 0x04
	"96,96,96",     // 3_8 = 0x05
	"64,64,64",     // 1_4 = 0x06
	"32,32,32",     // 1_8 = 0x07
	"0,0,0",        // INVALID = 0x08
	"0,0,0",        // INVALID = 0x09
	"0,0,0",        // INVALID = 0x0a
	"0,0,0",        // INVALID = 0x0b
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

static const char *tevKSelTableA[] =
{
	"255",  // 1   = 0x00
	"223",  // 7_8 = 0x01
	"191",  // 3_4 = 0x02
	"159",  // 5_8 = 0x03
	"128",  // 1_2 = 0x04
	"96",   // 3_8 = 0x05
	"64",   // 1_4 = 0x06
	"32",   // 1_8 = 0x07
	"0",    // INVALID = 0x08
	"0",    // INVALID = 0x09
	"0",    // INVALID = 0x0a
	"0",    // INVALID = 0x0b
	"0",    // INVALID = 0x0c
	"0",    // INVALID = 0x0d
	"0",    // INVALID = 0x0e
	"0",    // INVALID = 0x0f
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

static const char *tevCInputTable[] =
{
	"prev.rgb",          // CPREV,
	"prev.aaa",          // APREV,
	"c0.rgb",            // C0,
	"c0.aaa",            // A0,
	"c1.rgb",            // C1,
	"c1.aaa",            // A1,
	"c2.rgb",            // C2,
	"c2.aaa",            // A2,
	"textemp.rgb",       // TEXC,
	"textemp.aaa",       // TEXA,
	"rastemp.rgb",       // RASC,
	"rastemp.aaa",       // RASA,
	"int3(255,255,255)", // ONE
	"int3(128,128,128)", // HALF
	"konsttemp.rgb",     // KONST
	"int3(0,0,0)",       // ZERO
};

static const char *tevAInputTable[] =
{
	"prev.a",        // APREV,
	"c0.a",          // A0,
	"c1.a",          // A1,
	"c2.a",          // A2,
	"textemp.a",     // TEXA,
	"rastemp.a",     // RASA,
	"konsttemp.a",   // KONST,  (hw1 had quarter)
	"0",             // ZERO
};

static const char *tevRasTable[] =
{
	"iround(colors_0 * 255.0)",
	"iround(colors_1 * 255.0)",
	"ERROR13", //2
	"ERROR14", //3
	"ERROR15", //4
	"(int4(1, 1, 1, 1) * alphabump)", // bump alpha (0..248)
	"(int4(1, 1, 1, 1) * (alphabump | (alphabump >> 5)))", // normalized bump alpha (0..255)
	"int4(0, 0, 0, 0)", // zero
};

static const char *tevCOutputTable[]  = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
static const char *tevAOutputTable[]  = { "prev.a", "c0.a", "c1.a", "c2.a" };

static char text[16384];

template<class T> static inline void WriteStage(T& out, pixel_shader_uid_data* uid_data, int n, API_TYPE ApiType, const char swapModeTable[4][5]);
template<class T> static inline void WriteTevRegular(T& out, const char* components, int bias, int op, int clamp, int shift);
template<class T> static inline void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType);
template<class T> static inline void WriteAlphaTest(T& out, pixel_shader_uid_data* uid_data, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth);
template<class T> static inline void WriteFog(T& out, pixel_shader_uid_data* uid_data);

template<class T>
static inline void GeneratePixelShader(T& out, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	pixel_shader_uid_data dummy_data;
	pixel_shader_uid_data* uid_data = out.template GetUidData<pixel_shader_uid_data>();
	if (uid_data == nullptr)
		uid_data = &dummy_data;

	out.SetBuffer(text);
	const bool is_writing_shadercode = (out.GetBuffer() != nullptr);
#ifndef ANDROID
	locale_t locale;
	locale_t old_locale;
	if (is_writing_shadercode)
	{
		locale = newlocale(LC_NUMERIC_MASK, "C", nullptr); // New locale for compilation
		old_locale = uselocale(locale); // Apply the locale for this thread
	}
#endif

	if (is_writing_shadercode)
		text[sizeof(text) - 1] = 0x7C;  // canary

	unsigned int numStages = bpmem.genMode.numtevstages + 1;
	unsigned int numTexgen = bpmem.genMode.numtexgens;

	out.Write("//Pixel Shader for TEV stages\n");
	out.Write("//%i TEV stages, %i texgens, %i IND stages\n",
		numStages, numTexgen, bpmem.genMode.numindstages);

	uid_data->dstAlphaMode = dstAlphaMode;
	uid_data->genMode_numindstages = bpmem.genMode.numindstages;
	uid_data->genMode_numtevstages = bpmem.genMode.numtevstages;
	uid_data->genMode_numtexgens = bpmem.genMode.numtexgens;

	// dot product for integer vectors
	out.Write("int idot(int3 x, int3 y)\n"
	          "{\n"
	          "\tint3 tmp = x * y;\n"
	          "\treturn tmp.x + tmp.y + tmp.z;\n"
	          "}\n");

	out.Write("int idot(int4 x, int4 y)\n"
	          "{\n"
	          "\tint4 tmp = x * y;\n"
	          "\treturn tmp.x + tmp.y + tmp.z + tmp.w;\n"
	          "}\n\n");

	// rounding + casting to integer at once in a single function
	out.Write("int  iround(float  x) { return int (round(x)); }\n"
	          "int2 iround(float2 x) { return int2(round(x)); }\n"
	          "int3 iround(float3 x) { return int3(round(x)); }\n"
	          "int4 iround(float4 x) { return int4(round(x)); }\n\n");

	if (ApiType == API_OPENGL)
	{
		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("SAMPLER_BINDING(%d) uniform sampler2DArray samp%d;\n", i, i);
	}
	else // D3D
	{
		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("sampler samp%d : register(s%d);\n", i, i);

		out.Write("\n");
		for (int i = 0; i < 8; ++i)
			out.Write("Texture2D Tex%d : register(t%d);\n", i, i);
	}
	out.Write("\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("layout(std140%s) uniform PSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 1" : "");
	}
	else
	{
		out.Write("cbuffer PSBlock : register(b0) {\n");
	}
	out.Write(
		"\tint4 " I_COLORS"[4];\n"
		"\tint4 " I_KCOLORS"[4];\n"
		"\tint4 " I_ALPHA";\n"
		"\tfloat4 " I_TEXDIMS"[8];\n"
		"\tint4 " I_ZBIAS"[2];\n"
		"\tint4 " I_INDTEXSCALE"[2];\n"
		"\tint4 " I_INDTEXMTX"[6];\n"
		"\tint4 " I_FOGCOLOR";\n"
		"\tint4 " I_FOGI";\n"
		"\tfloat4 " I_FOGF"[2];\n"
		"};\n");

	if (g_ActiveConfig.bEnablePixelLighting)
	{
		out.Write("%s", s_lighting_struct);

		if (ApiType == API_OPENGL)
		{
			out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
		}
		else
		{
			out.Write("cbuffer VSBlock : register(b1) {\n");
		}
		out.Write(s_shader_uniforms);
		out.Write("};\n");
	}

	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		out.Write(
			"layout(std140, binding = 3) buffer BBox {\n"
			"\tint4 bbox_data;\n"
			"};\n"
		);
	}

	GenerateVSOutputStruct(out, ApiType);

	const bool forced_early_z = g_ActiveConfig.backend_info.bSupportsEarlyZ && bpmem.UseEarlyDepthTest() && (g_ActiveConfig.bFastDepthCalc || bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED);
	const bool per_pixel_depth = (bpmem.ztex2.op != ZTEXTURE_DISABLE && bpmem.UseLateDepthTest()) || (!g_ActiveConfig.bFastDepthCalc && bpmem.zmode.testenable && !forced_early_z);

	if (forced_early_z)
	{
		// Zcomploc (aka early_ztest) is a way to control whether depth test is done before
		// or after texturing and alpha test. PC graphics APIs used to provide no way to emulate
		// this feature properly until 2012: Depth tests were always done after alpha testing.
		// Most importantly, it was not possible to write to the depth buffer without also writing
		// a color value (unless color writing was disabled altogether).

		// OpenGL has a flag which allows the driver to still update the depth buffer if alpha
		// test fails. The driver isn't required to do this, but I (degasus) assume all of them do
		// because it's the much faster code path for the GPU.

		// D3D11 also has a way to force the driver to enable early-z, so we're fine here.
		if(ApiType == API_OPENGL)
		{
			out.Write("layout(early_fragment_tests) in;\n");
		}
		else
		{
			out.Write("[earlydepthstencil]\n");
		}
	}
	else if (bpmem.UseEarlyDepthTest() && (g_ActiveConfig.bFastDepthCalc || bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED) && is_writing_shadercode)
	{
		static bool warn_once = true;
		if (warn_once)
			WARN_LOG(VIDEO, "Early z test enabled but not possible to emulate with current configuration. Make sure to enable fast depth calculations. If this message still shows up your hardware isn't able to emulate the feature properly (a GPU with D3D 11.0 / OGL 4.2 support is required).");
		warn_once = false;
	}

	if (ApiType == API_OPENGL)
	{
		out.Write("out vec4 ocol0;\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("out vec4 ocol1;\n");

		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");

		// We use the flag "centroid" to fix some MSAA rendering bugs. With MSAA, the
		// pixel shader will be executed for each pixel which has at least one passed sample.
		// So there may be rendered pixels where the center of the pixel isn't in the primitive.
		// As the pixel shader usually renders at the center of the pixel, this position may be
		// outside the primitive. This will lead to sampling outside the texture, sign changes, ...
		// As a workaround, we interpolate at the centroid of the coveraged pixel, which
		// is always inside the primitive.
		// Without MSAA, this flag is defined to have no effect.
		out.Write("centroid in VS_OUTPUT o;\n");

		uid_data->stereo = g_ActiveConfig.iStereoMode > 0;
		if (g_ActiveConfig.iStereoMode > 0)
			out.Write("flat in int layer;\n");

		out.Write("void main()\n{\n");

		// compute window position if needed because binding semantic WPOS is not widely supported
		// Let's set up attributes
		for (unsigned int i = 0; i < numTexgen; ++i)
		{
			out.Write("\tfloat3 uv%d = o.tex%d;\n", i, i);
		}
		out.Write("\tfloat4 clipPos = o.clipPos;\n");
		if (g_ActiveConfig.bEnablePixelLighting)
		{
			out.Write("\tfloat4 Normal = o.Normal;\n");
		}

		// On Mali, global variables must be initialized as constants.
		// This is why we initialize these variables locally instead.
		out.Write("\tfloat4 colors_0 = o.colors_0;\n");
		out.Write("\tfloat4 colors_1 = o.colors_1;\n");

		out.Write("\tfloat4 rawpos = gl_FragCoord;\n");
	}
	else // D3D
	{
		out.Write("void main(\n");
		out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
			dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
			per_pixel_depth ? "\n  out float depth : SV_Depth," : "");

		out.Write("  in centroid float4 colors_0 : COLOR0,\n");
		out.Write("  in centroid float4 colors_1 : COLOR1");

		// compute window position if needed because binding semantic WPOS is not widely supported
		for (unsigned int i = 0; i < numTexgen; ++i)
			out.Write(",\n  in centroid float3 uv%d : TEXCOORD%d", i, i);
		out.Write(",\n  in centroid float4 clipPos : TEXCOORD%d", numTexgen);
		if (g_ActiveConfig.bEnablePixelLighting)
			out.Write(",\n  in centroid float4 Normal : TEXCOORD%d", numTexgen + 1);
		out.Write("        ) {\n");
	}

	out.Write("\tint4 c0 = " I_COLORS"[1], c1 = " I_COLORS"[2], c2 = " I_COLORS"[3], prev = " I_COLORS"[0];\n"
	          "\tint4 rastemp = int4(0, 0, 0, 0), textemp = int4(0, 0, 0, 0), konsttemp = int4(0, 0, 0, 0);\n"
	          "\tint3 comp16 = int3(1, 256, 0), comp24 = int3(1, 256, 256*256);\n"
	          "\tint alphabump=0;\n"
	          "\tint3 tevcoord=int3(0, 0, 0);\n"
	          "\tint2 wrappedcoord=int2(0,0), tempcoord=int2(0,0);\n"
	          "\tint4 tevin_a=int4(0,0,0,0),tevin_b=int4(0,0,0,0),tevin_c=int4(0,0,0,0),tevin_d=int4(0,0,0,0);\n\n"); // tev combiner inputs

	if (g_ActiveConfig.bEnablePixelLighting)
	{
		out.Write("\tfloat3 _norm0 = normalize(Normal.xyz);\n\n");
		out.Write("\tfloat3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");

		out.Write("\tint4 lacc;\n"
				"\tfloat3 ldir, h;\n"
				"\tfloat dist, dist2, attn;\n");

		// TODO: Our current constant usage code isn't able to handle more than one buffer.
		//       So we can't mark the VS constant as used here. But keep them here as reference.
		//out.SetConstantsUsed(C_PLIGHT_COLORS, C_PLIGHT_COLORS+7); // TODO: Can be optimized further
		//out.SetConstantsUsed(C_PLIGHTS, C_PLIGHTS+31); // TODO: Can be optimized further
		//out.SetConstantsUsed(C_PMATERIALS, C_PMATERIALS+3);
		uid_data->components = components;
		GenerateLightingShader<T>(out, uid_data->lighting, components, "colors_", "colors_");
	}

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		out.Write("\tint2 fixpoint_uv0 = int2(0, 0);\n\n");
	}
	else
	{
		out.SetConstantsUsed(C_TEXDIMS, C_TEXDIMS+numTexgen-1);
		for (unsigned int i = 0; i < numTexgen; ++i)
		{
			out.Write("\tint2 fixpoint_uv%d = iround(", i);
			// optional perspective divides
			uid_data->texMtxInfo_n_projection |= xfmem.texMtxInfo[i].projection << i;
			if (xfmem.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
			{
				out.Write("(uv%d.z == 0.0 ? uv%d.xy : uv%d.xy / uv%d.z)", i, i, i, i);
			}
			else
			{
				out.Write("uv%d.xy", i);
			}
			out.Write(" * " I_TEXDIMS"[%d].zw * 128.0);\n", i);
			// TODO: S24 overflows here?
		}
	}

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

	uid_data->nIndirectStagesUsed = nIndirectStagesUsed;
	for (u32 i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (nIndirectStagesUsed & (1 << i))
		{
			unsigned int texcoord = bpmem.tevindref.getTexCoord(i);
			unsigned int texmap = bpmem.tevindref.getTexMap(i);

			uid_data->SetTevindrefValues(i, texcoord, texmap);
			if (texcoord < numTexgen)
			{
				out.SetConstantsUsed(C_INDTEXSCALE+i/2,C_INDTEXSCALE+i/2);
				out.Write("\ttempcoord = fixpoint_uv%d >> " I_INDTEXSCALE"[%d].%s;\n", texcoord, i / 2, (i & 1) ? "zw" : "xy");
			}
			else
				out.Write("\ttempcoord = int2(0, 0);\n");

			out.Write("\tint3 iindtex%d = ", i);
			SampleTexture<T>(out, "(float2(tempcoord)/128.0)", "abg", texmap, ApiType);
		}
	}

	// Uid fields for BuildSwapModeTable are set in WriteStage
	char swapModeTable[4][5];
	const char* swapColors = "rgba";
	for (int i = 0; i < 4; i++)
	{
		swapModeTable[i][0] = swapColors[bpmem.tevksel[i*2].swap1];
		swapModeTable[i][1] = swapColors[bpmem.tevksel[i*2].swap2];
		swapModeTable[i][2] = swapColors[bpmem.tevksel[i*2+1].swap1];
		swapModeTable[i][3] = swapColors[bpmem.tevksel[i*2+1].swap2];
		swapModeTable[i][4] = '\0';
	}

	for (unsigned int i = 0; i < numStages; i++)
		WriteStage<T>(out, uid_data, i, ApiType, swapModeTable); // build the equation for this stage

#define MY_STRUCT_OFFSET(str,elem) ((u32)((u64)&(str).elem-(u64)&(str)))
	bool enable_pl = g_ActiveConfig.bEnablePixelLighting;
	uid_data->num_values = (enable_pl) ? sizeof(*uid_data) : MY_STRUCT_OFFSET(*uid_data,stagehash[numStages]);


	if (numStages)
	{
		// The results of the last texenv stage are put onto the screen,
		// regardless of the used destination register
		if (bpmem.combiners[numStages - 1].colorC.dest != 0)
		{
			out.Write("\tprev.rgb = %s;\n", tevCOutputTable[bpmem.combiners[numStages - 1].colorC.dest]);
		}
		if (bpmem.combiners[numStages - 1].alphaC.dest != 0)
		{
			out.Write("\tprev.a = %s;\n", tevAOutputTable[bpmem.combiners[numStages - 1].alphaC.dest]);
		}
	}
	out.Write("\tprev = prev & 255;\n");

	AlphaTest::TEST_RESULT Pretest = bpmem.alpha_test.TestResult();
	uid_data->Pretest = Pretest;

	// NOTE: Fragment may not be discarded if alpha test always fails and early depth test is enabled
	// (in this case we need to write a depth value if depth test passes regardless of the alpha testing result)
	if (Pretest == AlphaTest::UNDETERMINED || (Pretest == AlphaTest::FAIL && bpmem.UseLateDepthTest()))
		WriteAlphaTest<T>(out, uid_data, ApiType, dstAlphaMode, per_pixel_depth);

	// FastDepth means to trust the depth generated in perspective division.
	// It should be correct, but it seems not to be as accurate as required. TODO: Find out why!
	// For disabled FastDepth we just calculate the depth value again.
	// The performance impact of this additional calculation doesn't matter, but it prevents
	// the host GPU driver from performing any early depth test optimizations.
	if (g_ActiveConfig.bFastDepthCalc)
		out.Write("\tint zCoord = iround(rawpos.z * float(0xFFFFFF));\n");
	else
	{
		out.SetConstantsUsed(C_ZBIAS+1, C_ZBIAS+1);
		// the screen space depth value = far z + (clip z / clip w) * z range
		out.Write("\tint zCoord = " I_ZBIAS"[1].x + iround((clipPos.z / clipPos.w) * float(" I_ZBIAS"[1].y));\n");
	}

	// depth texture can safely be ignored if the result won't be written to the depth buffer (early_ztest) and isn't used for fog either
	const bool skip_ztexture = !per_pixel_depth && !bpmem.fog.c_proj_fsel.fsel;

	uid_data->ztex_op = bpmem.ztex2.op;
	uid_data->per_pixel_depth = per_pixel_depth;
	uid_data->forced_early_z = forced_early_z;
	uid_data->fast_depth_calc = g_ActiveConfig.bFastDepthCalc;
	uid_data->early_ztest = bpmem.UseEarlyDepthTest();
	uid_data->fog_fsel = bpmem.fog.c_proj_fsel.fsel;

	// Note: z-textures are not written to depth buffer if early depth test is used
	if (per_pixel_depth && bpmem.UseEarlyDepthTest())
		out.Write("\tdepth = float(zCoord) / float(0xFFFFFF);\n");

	// Note: depth texture output is only written to depth buffer if late depth test is used
	// theoretical final depth value is used for fog calculation, though, so we have to emulate ztextures anyway
	if (bpmem.ztex2.op != ZTEXTURE_DISABLE && !skip_ztexture)
	{
		// use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
		out.SetConstantsUsed(C_ZBIAS, C_ZBIAS+1);
		out.Write("\tzCoord = idot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w %s;\n",
									(bpmem.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");
		out.Write("\tzCoord = zCoord & 0xFFFFFF;\n");
	}

	if (per_pixel_depth && bpmem.UseLateDepthTest())
		out.Write("\tdepth = float(zCoord) / float(0xFFFFFF);\n");

	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);
		out.Write("\tocol0 = float4(float3(prev.rgb), float(" I_ALPHA".a)) / 255.0;\n");
	}
	else
	{
		WriteFog<T>(out, uid_data);
		out.Write("\tocol0 = float4(prev) / 255.0;\n");
	}

	// Use dual-source color blending to perform dst alpha in a single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);

		// Colors will be blended against the alpha from ocol1 and
		// the alpha from ocol0 will be written to the framebuffer.
		out.Write("\tocol1 = float4(prev) / 255.0;\n");
		out.Write("\tocol0.a = float(" I_ALPHA".a) / 255.0;\n");
	}

	if (g_ActiveConfig.backend_info.bSupportsBBox && BoundingBox::active)
	{
		uid_data->bounding_box = true;
		out.Write(
			"\tif(bbox_data.x > int(gl_FragCoord.x)) atomicMin(bbox_data.x, int(gl_FragCoord.x));\n"
			"\tif(bbox_data.y < int(gl_FragCoord.x)) atomicMax(bbox_data.y, int(gl_FragCoord.x));\n"
			"\tif(bbox_data.z > int(gl_FragCoord.y)) atomicMin(bbox_data.z, int(gl_FragCoord.y));\n"
			"\tif(bbox_data.w < int(gl_FragCoord.y)) atomicMax(bbox_data.w, int(gl_FragCoord.y));\n"
		);
	}

	out.Write("}\n");

	if (is_writing_shadercode)
	{
		if (text[sizeof(text) - 1] != 0x7C)
			PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");

#ifndef ANDROID
		uselocale(old_locale); // restore locale
		freelocale(locale);
#endif
	}
}


template<class T>
static inline void WriteStage(T& out, pixel_shader_uid_data* uid_data, int n, API_TYPE ApiType, const char swapModeTable[4][5])
{
	int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
	bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
	bool bHasIndStage = bpmem.tevind[n].bt < bpmem.genMode.numindstages;
	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;

	out.Write("\n\t// TEV stage %d\n", n);

	uid_data->stagehash[n].hasindstage = bHasIndStage;
	uid_data->stagehash[n].tevorders_texcoord = texcoord;
	if (bHasIndStage)
	{
		uid_data->stagehash[n].tevind = bpmem.tevind[n].hex & 0x7FFFFF;

		out.Write("\t// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using iindtex%d as the offset coords
		if (bpmem.tevind[n].bs != ITBA_OFF)
		{
			const char *tevIndAlphaSel[]   = {"", "x", "y", "z"};
			const char *tevIndAlphaMask[] = {"248", "224", "240", "248"}; // 0b11111000, 0b11100000, 0b11110000, 0b11111000
			out.Write("alphabump = iindtex%d.%s & %s;\n",
					bpmem.tevind[n].bt,
					tevIndAlphaSel[bpmem.tevind[n].bs],
					tevIndAlphaMask[bpmem.tevind[n].fmt]);
		}
		else
		{
			// TODO: Should we reset alphabump to 0 here?
		}

		if (bpmem.tevind[n].mid != 0)
		{
			// format
			const char *tevIndFmtMask[] = { "255", "31", "15", "7" };
			out.Write("\tint3 iindtevcrd%d = iindtex%d & %s;\n", n, bpmem.tevind[n].bt, tevIndFmtMask[bpmem.tevind[n].fmt]);

			// bias - TODO: Check if this needs to be this complicated..
			const char *tevIndBiasField[] = { "", "x", "y", "xy", "z", "xz", "yz", "xyz" }; // indexed by bias
			const char *tevIndBiasAdd[] = { "-128", "1", "1", "1" }; // indexed by fmt
			if (bpmem.tevind[n].bias == ITB_S || bpmem.tevind[n].bias == ITB_T || bpmem.tevind[n].bias == ITB_U)
				out.Write("\tiindtevcrd%d.%s += int(%s);\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);
			else if (bpmem.tevind[n].bias == ITB_ST || bpmem.tevind[n].bias == ITB_SU || bpmem.tevind[n].bias == ITB_TU)
				out.Write("\tiindtevcrd%d.%s += int2(%s, %s);\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt], tevIndBiasAdd[bpmem.tevind[n].fmt]);
			else if (bpmem.tevind[n].bias == ITB_STU)
				out.Write("\tiindtevcrd%d.%s += int3(%s, %s, %s);\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt], tevIndBiasAdd[bpmem.tevind[n].fmt], tevIndBiasAdd[bpmem.tevind[n].fmt]);

			// multiply by offset matrix and scale - calculations are likely to overflow badly,
			// yet it works out since we only care about the lower 23 bits (+1 sign bit) of the result
			if (bpmem.tevind[n].mid <= 3)
			{
				int mtxidx = 2*(bpmem.tevind[n].mid-1);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);

				out.Write("\tint2 indtevtrans%d = int2(idot(" I_INDTEXMTX"[%d].xyz, iindtevcrd%d), idot(" I_INDTEXMTX"[%d].xyz, iindtevcrd%d)) >> 3;\n", n, mtxidx, n, mtxidx+1, n);

				// TODO: should use a shader uid branch for this for better performance
				out.Write("\tif (" I_INDTEXMTX"[%d].w >= 0) indtevtrans%d = indtevtrans%d >> " I_INDTEXMTX"[%d].w;\n", mtxidx, n, n, mtxidx);
				out.Write("\telse indtevtrans%d = indtevtrans%d << (-" I_INDTEXMTX"[%d].w);\n", n, n, mtxidx);
			}
			else if (bpmem.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(bpmem.tevind[n].mid >= 5);
				int mtxidx = 2*(bpmem.tevind[n].mid-5);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("\tint2 indtevtrans%d = int2(fixpoint_uv%d * iindtevcrd%d.xx) >> 8;\n", n, texcoord, n);

				out.Write("\tif (" I_INDTEXMTX"[%d].w >= 0) indtevtrans%d = indtevtrans%d >> " I_INDTEXMTX"[%d].w;\n", mtxidx, n, n, mtxidx);
				out.Write("\telse indtevtrans%d = indtevtrans%d << (-" I_INDTEXMTX"[%d].w);\n", n, n, mtxidx);
			}
			else if (bpmem.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(bpmem.tevind[n].mid >= 9);
				int mtxidx = 2*(bpmem.tevind[n].mid-9);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("\tint2 indtevtrans%d = int2(fixpoint_uv%d * iindtevcrd%d.yy) >> 8;\n", n, texcoord, n);

				out.Write("\tif (" I_INDTEXMTX"[%d].w >= 0) indtevtrans%d = indtevtrans%d >> " I_INDTEXMTX"[%d].w;\n", mtxidx, n, n, mtxidx);
				out.Write("\telse indtevtrans%d = indtevtrans%d << (-" I_INDTEXMTX"[%d].w);\n", n, n, mtxidx);
			}
			else
			{
				out.Write("\tint2 indtevtrans%d = int2(0, 0);\n", n);
			}
		}
		else
		{
			out.Write("\tint2 indtevtrans%d = int2(0, 0);\n", n);
		}

		// ---------
		// Wrapping
		// ---------
		const char *tevIndWrapStart[]  = {"0", "(256<<7)", "(128<<7)", "(64<<7)", "(32<<7)", "(16<<7)", "1" }; // TODO: Should the last one be 1 or (1<<7)?

		// wrap S
		if (bpmem.tevind[n].sw == ITW_OFF)
			out.Write("\twrappedcoord.x = fixpoint_uv%d.x;\n", texcoord);
		else if (bpmem.tevind[n].sw == ITW_0)
			out.Write("\twrappedcoord.x = 0;\n");
		else
			out.Write("\twrappedcoord.x = fixpoint_uv%d.x %% %s;\n", texcoord, tevIndWrapStart[bpmem.tevind[n].sw]);

		// wrap T
		if (bpmem.tevind[n].tw == ITW_OFF)
			out.Write("\twrappedcoord.y = fixpoint_uv%d.y;\n", texcoord);
		else if (bpmem.tevind[n].tw == ITW_0)
			out.Write("\twrappedcoord.y = 0;\n");
		else
			out.Write("\twrappedcoord.y = fixpoint_uv%d.y %% %s;\n", texcoord, tevIndWrapStart[bpmem.tevind[n].tw]);

		if (bpmem.tevind[n].fb_addprev) // add previous tevcoord
			out.Write("\ttevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			out.Write("\ttevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);

		// Emulate s24 overflows
		out.Write("\ttevcoord.xy = (tevcoord.xy << 8) >> 8;\n");
	}

	TevStageCombiner::ColorCombiner &cc = bpmem.combiners[n].colorC;
	TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[n].alphaC;

	uid_data->stagehash[n].cc = cc.hex & 0xFFFFFF;
	uid_data->stagehash[n].ac = ac.hex & 0xFFFFF0; // Storing rswap and tswap later

	if (cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC ||
	   cc.b == TEVCOLORARG_RASA || cc.b == TEVCOLORARG_RASC ||
	   cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC ||
	   cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC ||
	   ac.a == TEVALPHAARG_RASA || ac.b == TEVALPHAARG_RASA ||
	   ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
	{
		const int i = bpmem.combiners[n].alphaC.rswap;
		uid_data->stagehash[n].ac |= bpmem.combiners[n].alphaC.rswap;
		uid_data->stagehash[n].tevksel_swap1a = bpmem.tevksel[i*2].swap1;
		uid_data->stagehash[n].tevksel_swap2a = bpmem.tevksel[i*2].swap2;
		uid_data->stagehash[n].tevksel_swap1b = bpmem.tevksel[i*2+1].swap1;
		uid_data->stagehash[n].tevksel_swap2b = bpmem.tevksel[i*2+1].swap2;
		uid_data->stagehash[n].tevorders_colorchan = bpmem.tevorders[n / 2].getColorChan(n & 1);

		const char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
		out.Write("\trastemp = %s.%s;\n", tevRasTable[bpmem.tevorders[n / 2].getColorChan(n & 1)], rasswap);
	}

	uid_data->stagehash[n].tevorders_enable = bpmem.tevorders[n / 2].getEnable(n & 1);
	if (bpmem.tevorders[n/2].getEnable(n&1))
	{
		int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
		if (!bHasIndStage)
		{
			// calc tevcord
			if (bHasTexCoord)
				out.Write("\ttevcoord.xy = fixpoint_uv%d;\n", texcoord);
			else
				out.Write("\ttevcoord.xy = int2(0, 0);\n");
		}

		const int i = bpmem.combiners[n].alphaC.tswap;
		uid_data->stagehash[n].ac |= bpmem.combiners[n].alphaC.tswap << 2;
		uid_data->stagehash[n].tevksel_swap1c = bpmem.tevksel[i*2].swap1;
		uid_data->stagehash[n].tevksel_swap2c = bpmem.tevksel[i*2].swap2;
		uid_data->stagehash[n].tevksel_swap1d = bpmem.tevksel[i*2+1].swap1;
		uid_data->stagehash[n].tevksel_swap2d = bpmem.tevksel[i*2+1].swap2;

		uid_data->stagehash[n].tevorders_texmap= bpmem.tevorders[n/2].getTexMap(n&1);

		const char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];
		uid_data->SetTevindrefTexmap(i, texmap);

		out.Write("\ttextemp = ");
		SampleTexture<T>(out, "(float2(tevcoord.xy)/128.0)", texswap, texmap, ApiType);
	}
	else
	{
		out.Write("\ttextemp = int4(255, 255, 255, 255);\n");
	}


	if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST ||
	    cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST ||
	    ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST ||
	    ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
	{
		int kc = bpmem.tevksel[n / 2].getKC(n & 1);
		int ka = bpmem.tevksel[n / 2].getKA(n & 1);
		uid_data->stagehash[n].tevksel_kc = kc;
		uid_data->stagehash[n].tevksel_ka = ka;
		out.Write("\tkonsttemp = int4(%s, %s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);

		if (kc > 7)
			out.SetConstantsUsed(C_KCOLORS+((kc-0xc)%4),C_KCOLORS+((kc-0xc)%4));
		if (ka > 7)
			out.SetConstantsUsed(C_KCOLORS+((ka-0xc)%4),C_KCOLORS+((ka-0xc)%4));
	}

	if (cc.d == TEVCOLORARG_C0 || cc.d == TEVCOLORARG_A0 || ac.d == TEVALPHAARG_A0)
		out.SetConstantsUsed(C_COLORS+1,C_COLORS+1);

	if (cc.d == TEVCOLORARG_C1 || cc.d == TEVCOLORARG_A1 || ac.d == TEVALPHAARG_A1)
		out.SetConstantsUsed(C_COLORS+2,C_COLORS+2);

	if (cc.d == TEVCOLORARG_C2 || cc.d == TEVCOLORARG_A2 || ac.d == TEVALPHAARG_A2)
		out.SetConstantsUsed(C_COLORS+3,C_COLORS+3);

	if (cc.dest >= GX_TEVREG0 && cc.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+cc.dest, C_COLORS+cc.dest);

	if (ac.dest >= GX_TEVREG0 && ac.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+ac.dest, C_COLORS+ac.dest);


	out.Write("\ttevin_a = int4(%s, %s)&255;\n", tevCInputTable[cc.a], tevAInputTable[ac.a]);
	out.Write("\ttevin_b = int4(%s, %s)&255;\n", tevCInputTable[cc.b], tevAInputTable[ac.b]);
	out.Write("\ttevin_c = int4(%s, %s)&255;\n", tevCInputTable[cc.c], tevAInputTable[ac.c]);
	out.Write("\ttevin_d = int4(%s, %s);\n", tevCInputTable[cc.d], tevAInputTable[ac.d]);

	out.Write("\t// color combine\n");
	out.Write("\t%s = clamp(", tevCOutputTable[cc.dest]);
	if (cc.bias != TevBias_COMPARE)
	{
		WriteTevRegular(out, "rgb", cc.bias, cc.op, cc.clamp, cc.shift);
	}
	else
	{
		const char *function_table[] =
		{
			"((tevin_a.r > tevin_b.r) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_R8_GT
			"((tevin_a.r == tevin_b.r) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_R8_EQ
			"((idot(tevin_a.rgb, comp16) >  idot(tevin_b.rgb, comp16)) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_GR16_GT
			"((idot(tevin_a.rgb, comp16) == idot(tevin_b.rgb, comp16)) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_GR16_EQ
			"((idot(tevin_a.rgb, comp24) >  idot(tevin_b.rgb, comp24)) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_BGR24_GT
			"((idot(tevin_a.rgb, comp24) == idot(tevin_b.rgb, comp24)) ? tevin_c.rgb : int3(0,0,0))", // TEVCMP_BGR24_EQ
			"(max(sign(tevin_a.rgb - tevin_b.rgb), int3(0,0,0)) * tevin_c.rgb)", // TEVCMP_RGB8_GT
			"((int3(1,1,1) - sign(abs(tevin_a.rgb - tevin_b.rgb))) * tevin_c.rgb)" // TEVCMP_RGB8_EQ
		};

		int mode = (cc.shift<<1)|cc.op;
		out.Write("   tevin_d.rgb + ");
		out.Write(function_table[mode]);
	}
	if (cc.clamp)
		out.Write(", int3(0,0,0), int3(255,255,255))");
	else
		out.Write(", int3(-1024,-1024,-1024), int3(1023,1023,1023))");
	out.Write(";\n");

	out.Write("\t// alpha combine\n");
	out.Write("\t%s = clamp(", tevAOutputTable[ac.dest]);
	if (ac.bias != TevBias_COMPARE)
	{
		WriteTevRegular(out, "a", ac.bias, ac.op, ac.clamp, ac.shift);
	}
	else
	{
		const char *function_table[] =
		{
			"((tevin_a.r > tevin_b.r) ? tevin_c.a : 0)", // TEVCMP_R8_GT
			"((tevin_a.r == tevin_b.r) ? tevin_c.a : 0)", // TEVCMP_R8_EQ
			"((idot(tevin_a.rgb, comp16) >  idot(tevin_b.rgb, comp16)) ? tevin_c.a : 0)", // TEVCMP_GR16_GT
			"((idot(tevin_a.rgb, comp16) == idot(tevin_b.rgb, comp16)) ? tevin_c.a : 0)", // TEVCMP_GR16_EQ
			"((idot(tevin_a.rgb, comp24) >  idot(tevin_b.rgb, comp24)) ? tevin_c.a : 0)", // TEVCMP_BGR24_GT
			"((idot(tevin_a.rgb, comp24) == idot(tevin_b.rgb, comp24)) ? tevin_c.a : 0)", // TEVCMP_BGR24_EQ
			"((tevin_a.a >  tevin_b.a) ? tevin_c.a : 0)", // TEVCMP_A8_GT
			"((tevin_a.a == tevin_b.a) ? tevin_c.a : 0)" // TEVCMP_A8_EQ
		};

		int mode = (ac.shift<<1)|ac.op;
		out.Write("   tevin_d.a + ");
		out.Write(function_table[mode]);
	}
	if (ac.clamp)
		out.Write(", 0, 255)");
	else
		out.Write(", -1024, 1023)");

	out.Write(";\n");
}

template<class T>
static inline void WriteTevRegular(T& out, const char* components, int bias, int op, int clamp, int shift)
{
	const char *tevScaleTableLeft[] =
	{
		"",       // SCALE_1
		" << 1",  // SCALE_2
		" << 2",  // SCALE_4
		"",       // DIVIDE_2
	};

	const char *tevScaleTableRight[] =
	{
		"",       // SCALE_1
		"",       // SCALE_2
		"",       // SCALE_4
		" >> 1",  // DIVIDE_2
	};

	const char *tevLerpBias[] = // indexed by 2*op+(shift==3)
	{
		"",
		" + 128",
		"",
		" + 127",
	};

	const char *tevBiasTable[] =
	{
		"",        // ZERO,
		" + 128",  // ADDHALF,
		" - 128",  // SUBHALF,
		"",
	};

	const char *tevOpTable[] = {
		"+",      // TEVOP_ADD = 0,
		"-",      // TEVOP_SUB = 1,
	};

	// Regular TEV stage: (d + bias + lerp(a,b,c)) * scale
	// The GC/Wii GPU uses a very sophisticated algorithm for scale-lerping:
	// - c is scaled from 0..255 to 0..256, which allows dividing the result by 256 instead of 255
	// - if scale is bigger than one, it is moved inside the lerp calculation for increased accuracy
	// - a rounding bias is added before dividing by 256
	out.Write("(((tevin_d.%s%s)%s)", components, tevBiasTable[bias], tevScaleTableLeft[shift]);
	out.Write(" %s ", tevOpTable[op]);
	out.Write("(((((tevin_a.%s<<8) + (tevin_b.%s-tevin_a.%s)*(tevin_c.%s+(tevin_c.%s>>7)))%s)%s)>>8)",
	          components, components, components, components, components,
	          tevScaleTableLeft[shift], tevLerpBias[2*op+(shift!=3)]);
	out.Write(")%s", tevScaleTableRight[shift]);
}

template<class T>
static inline void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType)
{
	out.SetConstantsUsed(C_TEXDIMS+texmap,C_TEXDIMS+texmap);

	if (ApiType == API_D3D)
		out.Write("iround(255.0 * Tex%d.Sample(samp%d,%s.xy * " I_TEXDIMS"[%d].xy)).%s;\n", texmap,texmap, texcoords, texmap, texswap);
	else
		out.Write("iround(255.0 * texture(samp%d, float3(%s.xy * " I_TEXDIMS"[%d].xy, %s))).%s;\n", texmap, texcoords, texmap, g_ActiveConfig.iStereoMode > 0 ? "layer" : "0.0", texswap);
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",					// NEVER
	"(prev.a <  %s)",			// LESS
	"(prev.a == %s)",			// EQUAL
	"(prev.a <= %s)",			// LEQUAL
	"(prev.a >  %s)",			// GREATER
	"(prev.a != %s)",			// NEQUAL
	"(prev.a >= %s)",			// GEQUAL
	"(true)"					// ALWAYS
};

static const char *tevAlphaFunclogicTable[] =
{
	" && ", // and
	" || ", // or
	" != ", // xor
	" == "  // xnor
};

template<class T>
static inline void WriteAlphaTest(T& out, pixel_shader_uid_data* uid_data, API_TYPE ApiType, DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA".r",
		I_ALPHA".g"
	};

	out.SetConstantsUsed(C_ALPHA, C_ALPHA);

	// This outputted if statement is not like 'if(!(cond))' for a reason.
	// Qualcomm's v95 drivers produce incorrect code using logical not
	// Checking against false produces the correct code.
	out.Write("\tif(( ");

	uid_data->alpha_test_comp0 = bpmem.alpha_test.comp0;
	uid_data->alpha_test_comp1 = bpmem.alpha_test.comp1;
	uid_data->alpha_test_logic = bpmem.alpha_test.logic;

	// Lookup the first component from the alpha function table
	int compindex = bpmem.alpha_test.comp0;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[0]);

	out.Write("%s", tevAlphaFunclogicTable[bpmem.alpha_test.logic]); // lookup the logic op

	// Lookup the second component from the alpha function table
	compindex = bpmem.alpha_test.comp1;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[1]);
	out.Write(") == false) {\n");

	out.Write("\t\tocol0 = float4(0.0, 0.0, 0.0, 0.0);\n");
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		out.Write("\t\tocol1 = float4(0.0, 0.0, 0.0, 0.0);\n");
	if (per_pixel_depth)
		out.Write("\t\tdepth = 1.0;\n");

	// ZCOMPLOC HACK:
	// The only way to emulate alpha test + early-z is to force early-z in the shader.
	// As this isn't available on all drivers and as we can't emulate this feature otherwise,
	// we are only able to choose which one we want to respect more.
	// Tests seem to have proven that writing depth even when the alpha test fails is more
	// important that a reliable alpha test, so we just force the alpha test to always succeed.
	// At least this seems to be less buggy.
	uid_data->alpha_test_use_zcomploc_hack = bpmem.UseEarlyDepthTest() && bpmem.zmode.updateenable && !g_ActiveConfig.backend_info.bSupportsEarlyZ;
	if (!uid_data->alpha_test_use_zcomploc_hack)
	{
		out.Write("\t\tdiscard;\n");
		if (ApiType != API_D3D)
			out.Write("\t\treturn;\n");
	}

	out.Write("\t}\n");
}

static const char *tevFogFuncsTable[] =
{
	"",                                                      // No Fog
	"",                                                      // ?
	"",                                                      // Linear
	"",                                                      // ?
	"\tfog = 1.0 - exp2(-8.0 * fog);\n",                     // exp
	"\tfog = 1.0 - exp2(-8.0 * fog * fog);\n",               // exp2
	"\tfog = exp2(-8.0 * (1.0 - fog));\n",                   // backward exp
	"\tfog = 1.0 - fog;\n   fog = exp2(-8.0 * fog * fog);\n" // backward exp2
};

template<class T>
static inline void WriteFog(T& out, pixel_shader_uid_data* uid_data)
{
	uid_data->fog_fsel = bpmem.fog.c_proj_fsel.fsel;
	if (bpmem.fog.c_proj_fsel.fsel == 0)
		return; // no Fog

	uid_data->fog_proj = bpmem.fog.c_proj_fsel.proj;

	out.SetConstantsUsed(C_FOGCOLOR, C_FOGCOLOR);
	out.SetConstantsUsed(C_FOGI, C_FOGI);
	out.SetConstantsUsed(C_FOGF, C_FOGF+1);
	if (bpmem.fog.c_proj_fsel.proj == 0)
	{
		// perspective
		// ze = A/(B - (Zs >> B_SHF)
		// TODO: Verify that we want to drop lower bits here! (currently taken over from software renderer)
		//       Maybe we want to use "ze = (A << B_SHF)/((B << B_SHF) - Zs)" instead?
		//       That's equivalent, but keeps the lower bits of Zs.
		out.Write("\tfloat ze = (" I_FOGF"[1].x * 16777215.0) / float(" I_FOGI".y - (zCoord >> " I_FOGI".w));\n");
	}
	else
	{
		// orthographic
		// ze = a*Zs    (here, no B_SHF)
		out.Write("\tfloat ze = " I_FOGF"[1].x * float(zCoord) / 16777215.0;\n");
	}

	// x_adjust = sqrt((x-center)^2 + k^2)/k
	// ze *= x_adjust
	// TODO Instead of this theoretical calculation, we should use the
	//      coefficient table given in the fog range BP registers!
	uid_data->fog_RangeBaseEnabled = bpmem.fogRange.Base.Enabled;
	if (bpmem.fogRange.Base.Enabled)
	{
		out.SetConstantsUsed(C_FOGF, C_FOGF);
		out.Write("\tfloat x_adjust = (2.0 * (rawpos.x / " I_FOGF"[0].y)) - 1.0 - " I_FOGF"[0].x;\n");
		out.Write("\tx_adjust = sqrt(x_adjust * x_adjust + " I_FOGF"[0].z * " I_FOGF"[0].z) / " I_FOGF"[0].z;\n");
		out.Write("\tze *= x_adjust;\n");
	}

	out.Write("\tfloat fog = clamp(ze - " I_FOGF"[1].z, 0.0, 1.0);\n");

	if (bpmem.fog.c_proj_fsel.fsel > 3)
	{
		out.Write("%s", tevFogFuncsTable[bpmem.fog.c_proj_fsel.fsel]);
	}
	else
	{
		if (bpmem.fog.c_proj_fsel.fsel != 2 && out.GetBuffer() != nullptr)
			WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpmem.fog.c_proj_fsel.fsel);
	}

	out.Write("\tint ifog = iround(fog * 256.0);\n");
	out.Write("\tprev.rgb = (prev.rgb * (256 - ifog) + " I_FOGCOLOR".rgb * ifog) >> 8;\n");
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

