// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/UberShaderPixel.h"

namespace UberShader
{

static char text[32768];

template<typename T>
std::string BitfieldExtract(const std::string& source, T type)
{
	return StringFromFormat("bitfieldExtract(%s, %u, %u)", source.c_str(), u32(type.offset), u32(type.size));
}

void GetPixelShaderUid(PixelShaderUid & shaderuid, DSTALPHA_MODE dstAlphaMode)
{
	pixel_ubershader_uid_data* uid = shaderuid.GetUidData<pixel_ubershader_uid_data>();
	uid->numTexgens = xfmem.numTexGen.numTexGens;
	uid->dualSourceBlending = dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND;
}

ShaderCode GenPixelShader(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, bool per_pixel_depth)
{
	ShaderCode out;
	out.SetBuffer(text);
	text[sizeof(text) - 1] = 0x7C;  // canary

	out.Write("// Pixel UberShader\n");
	WritePixelShaderCommonHeader(out, ApiType);

	// Bad
	u32 numTexgen = xfmem.numTexGen.numTexGens;

	// TODO: This is variable based on number of texcoord gens
	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers(out, ApiType, numTexgen);
	out.Write("};\n");

	// TEV constants
	if (ApiType == API_OPENGL) out.Write(
		"layout(std140, binding = 4) uniform UBERBlock {\n");
	else out.Write(
		"cbuffer UBERBlock : register(b1) {\n");
	out.Write(
		"	uint	bpmem_genmode;\n"
		"	uint	bpmem_alphaTest;\n"
		"	uint	bpmem_fogParam3;\n"
		"	uint	bpmem_fogRangeBase;\n"
		"	uint	bpmem_tevorder[8];\n"
		"	uint2	bpmem_combiners[16];\n"
		"	uint	bpmem_tevksel[8];\n"
		"	int4	konstLookup[32];\n"
		"	float4  debug;\n"
		"};\n");

	// TODO: Per pixel lighting (not really needed)

	// TODO: early depth tests (we will need multiple shaders)

	// ==============================================
	//  BitfieldExtract for APIs which don't have it
	// ==============================================

	if (!g_ActiveConfig.backend_info.bSupportsBitfield)
	{
		out.Write(
			"uint bitfieldExtract(uint val, int off, int size) {\n"
			"	// This built-in function is only support in OpenGL 4.0+ and ES 3.1+\n"
			"	// Microsoft's HLSL compiler automatically optimises this to a bitfield extract instruction.\n"
			"	uint mask = uint((1 << size) - 1);\n"
			"	return uint(val >> off) & mask;\n"
			"}\n\n");
	}

	// =====================
	//   Texture Sampling
	// =====================

	if (g_ActiveConfig.backend_info.bSupportsDynamicSamplerIndexing)
	{
		// Doesn't look like directx supports this. Oh well the code path is here just incase it supports this in the future.
		out.Write(
			"int4 sampleTexture(uint sampler_num, float2 uv) {\n");
		if (ApiType == API_OPENGL) out.Write(
			"	return int4(texture(samp[sampler_num], float3(uv, 0.0)) * 255.0);\n");
		else if (ApiType == API_D3D) out.Write(
			"	return int4(Tex[sampler_num].Sample(samp[sampler_num], float3(uv, 0.0)) * 255.0);\n");
		out.Write(
			"}\n\n");
	}
	else
	{
		out.Write(
			"int4 sampleTexture(uint sampler_num, float2 uv) {\n"
			"	// This is messy, but DirectX, OpenGl 3.3 and Opengl ES 3.0 doesn't support dynamic indexing of the sampler array\n"
			"	// With any luck the shader compiler will optimise this if the hardware supports dynamic indexing.\n"
			"	switch(sampler_num) {\n");
		for (int i = 0; i < 8; i++)
		{
			if (ApiType == API_OPENGL) out.Write(
				"	case %du: return int4(texture(samp[%d], float3(uv, 0.0)) * 255.0);\n", i, i);
			else if (ApiType == API_D3D) out.Write(
				"	case %du: return int4(Tex[%d].Sample(samp[%d], float3(uv, 0.0)) * 255.0);\n", i, i, i);
		}
		out.Write(
			"	}\n"
			"}\n\n");
	}

	// ======================
	//   TEV's Special Lerp
	// ======================

	out.Write(
		"// One channel worth of TEV's Linear Interpolate, plus bias, add/subtract and scale\n"
		"int tevLerp(int A, int B, int C, int D, uint bias, bool op, uint shift) {\n"
		"	C += C >> 7; // Scale C from 0..255 to 0..256\n"
		"	int lerp = (A << 8) + (B - A)*C;\n"
		"	if (shift != 3u) {\n"
		"		lerp = lerp << shift;\n"
		"		lerp = lerp + (op ? 127 : 128);\n"
		"	}\n"
		"	int result = lerp >> 8;\n"
		"\n"
		"	// Add/Subtract D (and bias)\n"
		"	if (bias == 1u) result += 128;\n"
		"	else if (bias == 2u) result -= 128;\n"
		"	if(op) // Subtract\n"
		"		result = D - result;\n"
		"	else // Add\n"
		"		result = D + result;\n"
		"\n"
		"	// Most of the Shift was moved inside the lerp for improved percision\n"
		"	// But we still do the divide by 2 here\n"
		"	if (shift == 3u)\n"
		"		result = result >> 1;\n"
		"	return result;\n"
		"}\n\n");

	// =======================
	//   TEV's Color Compare
	// =======================

	out.Write(
		"// Implements operations 0-5 of tev's compare mode,\n"
		"// which are common to both color and alpha channels\n"
		"bool tevCompare(uint op, int3 color_A, int3 color_B) {\n"
		"	switch (op) {\n"
		"	case 0u: // TEVCMP_R8_GT\n"
		"		return (color_A.r > color_B.r);\n"
		"	case 1u: // TEVCMP_R8_EQ\n"
		"		return (color_A.r == color_B.r);\n"
		"	case 2u: // TEVCMP_GR16_GT\n"
		"		int A_16 = (color_A.r | (color_A.g << 8));\n"
		"		int B_16 = (color_B.r | (color_B.g << 8));\n"
		"		return A_16 > B_16;\n"
		"	case 3u: // TEVCMP_GR16_EQ\n"
		"		return (color_A.r == color_B.r && color_A.g == color_B.g);\n"
		"	case 4u: // TEVCMP_BGR24_GT\n"
		"		int A_24 = (color_A.r | (color_A.g << 8) | (color_A.b << 16));\n"
		"		int B_24 = (color_B.r | (color_B.g << 8) | (color_B.b << 16));\n"
		"		return A_24 > B_24;\n"
		"	case 5u: // TEVCMP_BGR24_EQ\n"
		"		return (color_A.r == color_B.r && color_A.g == color_B.g && color_A.b == color_B.b);\n"
		"	default:\n"
		"		return false;\n"
		"	}\n"
		"}\n\n");

	// =================
	//   Alpha Compare
	// =================

	out.Write(
		"// Helper function for Alpha Test\n"
		"bool alphaCompare(int a, int b, uint compare) {\n"
		"	switch (compare) {\n"
		"	case 0u: // NEVER\n"
		"		return false;\n"
		"	case 1u: // LESS\n"
		"		return a < b;\n"
		"	case 2u: // EQUAL\n"
		"		return a == b;\n"
		"	case 3u: // LEQUAL\n"
		"		return a <= b;\n"
		"	case 4u: // GREATER\n"
		"		return a > b;\n"
		"	case 5u: // NEQUAL;\n"
		"		return a != b;\n"
		"	case 6u: // GEQUAL\n"
		"		return a >= b;\n"
		"	case 7u: // ALWAYS\n"
		"		return true;\n"
		"	}\n"
		"}\n\n");

	// =================
	//   Input Selects
	// =================

	out.Write(
		"struct State {\n"
		"	int4 Reg[4];\n"
		"	int4 RasColor;\n"
		"	int4 TexColor;\n"
		"	int4 KonstColor;\n"
		"};\n"
		"\n"
		"int3 selectColorInput(State s, uint index) {\n"
		"	switch (index) {\n"
		"	case 0u: // prev.rgb\n"
		"		return s.Reg[0].rgb;\n"
		"	case 1u: // prev.aaa\n"
		"		return s.Reg[0].aaa;\n"
		"	case 2u: // c0.rgb\n"
		"		return s.Reg[1].rgb;\n"
		"	case 3u: // c0.aaa\n"
		"		return s.Reg[1].aaa;\n"
		"	case 4u: // c1.rgb\n"
		"		return s.Reg[2].rgb;\n"
		"	case 5u: // c1.aaa\n"
		"		return s.Reg[2].aaa;\n"
		"	case 6u: // c2.rgb\n"
		"		return s.Reg[3].rgb;\n"
		"	case 7u: // c2.aaa\n"
		"		return s.Reg[3].aaa;\n"
		"	case 8u:\n"
		"		return s.TexColor.rgb;\n"
		"	case 9u:\n"
		"		return s.TexColor.aaa;\n"
		"	case 10u:\n"
		"		return s.RasColor.rgb;\n"
		"	case 11u:\n"
		"		return s.RasColor.aaa;\n"
		"	case 12u: // One\n"
		"		return int3(255, 255, 255);\n"
		"	case 13u: // Half\n"
		"		return int3(128, 128, 128);\n"
		"	case 14u:\n"
		"		return s.KonstColor.rgb;\n"
		"	case 15u: // Zero\n"
		"		return int3(0, 0, 0);\n"
		"	}\n"
		"}\n"
		"int selectAlphaInput(State s, uint index) {\n"
		"	switch (index) {\n"
		"	case 0u: // prev.a\n"
		"		return s.Reg[0].a;\n"
		"	case 1u: // c0.a\n"
		"		return s.Reg[1].a;\n"
		"	case 2u: // c1.a\n"
		"		return s.Reg[2].a;\n"
		"	case 3u: // c2.a\n"
		"		return s.Reg[3].a;\n"
		"	case 4u:\n"
		"		return s.TexColor.a;\n"
		"	case 5u:\n"
		"		return s.RasColor.a;\n"
		"	case 6u:\n"
		"		return s.KonstColor.a;\n"
		"	case 7u: // Zero\n"
		"		return 0;\n"
		"	}\n"
		"}\n"
		"\n"
		"void setRegColor(inout State s, uint index, int3 color) {\n"
		"	switch (index) {\n"
		"	case 0u: // prev\n"
		"		s.Reg[0].rgb = color;\n"
		"		break;\n"
		"	case 1u: // c0\n"
		"		s.Reg[1].rgb = color;\n"
		"		break;\n"
		"	case 2u: // c1\n"
		"		s.Reg[2].rgb = color;\n"
		"		break;\n"
		"	case 3u: // c2\n"
		"		s.Reg[3].rgb = color;\n"
		"		break;\n"
		"	}\n"
		"}\n"
		"\n"
		"void setRegAlpha(inout State s, uint index, int alpha) {\n"
		"	switch (index) {\n"
		"	case 0u: // prev\n"
		"		s.Reg[0].a = alpha;\n"
		"		break;\n"
		"	case 1u: // c0\n"
		"		s.Reg[1].a = alpha;\n"
		"		break;\n"
		"	case 2u: // c1\n"
		"		s.Reg[2].a = alpha;\n"
		"		break;\n"
		"	case 3u: // c2\n"
		"		s.Reg[3].a = alpha;\n"
		"		break;\n"
		"	}\n"
		"}\n"
		"\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("out vec4 ocol0;\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("out vec4 ocol1;\n");

		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");

		out.Write("in VertexData {\n");
		GenerateVSOutputMembers(out, ApiType, numTexgen, GetInterpolationQualifier(ApiType, true, true));

		// TODO: Stereo Mode

		out.Write("};\n\n");

		// TODO: Add support for OpenGL without geometery shaders back in.

		out.Write("void main()\n{\n");

		out.Write("\tfloat4 rawpos = gl_FragCoord;\n");
	}
	else // D3D
	{
		out.Write("void main(\n");
		out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
				  dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
				  per_pixel_depth ? "\n  out float depth : SV_Depth," : "");

		out.Write("  in %s float4 colors_0 : COLOR0,\n", GetInterpolationQualifier(ApiType));
		out.Write("  in %s float4 colors_1 : COLOR1\n", GetInterpolationQualifier(ApiType));

		// compute window position if needed because binding semantic WPOS is not widely supported
		if (numTexgen > 0)
			out.Write(",\n  in %s float3 tex[%d] : TEXCOORD0", GetInterpolationQualifier(ApiType), numTexgen);
		out.Write(",\n  in %s float4 clipPos : TEXCOORD%d", GetInterpolationQualifier(ApiType), numTexgen);
		if (g_ActiveConfig.bEnablePixelLighting)
		{
			out.Write(",\n  in %s float3 Normal : TEXCOORD%d", GetInterpolationQualifier(ApiType), numTexgen + 1);
			out.Write(",\n  in %s float3 WorldPos : TEXCOORD%d", GetInterpolationQualifier(ApiType), numTexgen + 2);
		}
		if (g_ActiveConfig.iStereoMode > 0)
			out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
		out.Write("        ) {\n");
	}

	out.Write(
		"	int AlphaBump = 0;\n"
		"	int4 icolors_0 = int4(colors_0 * 255.0);\n"
		"	int4 icolors_1 = int4(colors_1 * 255.0);\n"
		"	int4 TevResult;\n"
		"	State s;\n"
		"	s.TexColor = int4(0, 0, 0, 0);\n"
		"	s.RasColor = int4(0, 0, 0, 0);\n"
		"	s.KonstColor = int4(0, 0, 0, 0);\n"
		"\n");
	for (int i = 0; i < 4; i++) out.Write(
		"	s.Reg[%d] = " I_COLORS"[%d];\n", i, i);


	out.Write(
		"\n"
		"	uint num_stages = %s;\n\n", BitfieldExtract("bpmem_genmode", GenMode().numtevstages).c_str());

	out.Write(
		"	// Main tev loop\n");
	if (ApiType == API_D3D) out.Write(
		"	[loop]\n"); // Tell DirectX we don't want this loop unrolled (it crashes if it tries to)
	out.Write(
		"	for(uint stage = 0u; stage <= num_stages; stage++)\n"
		"	{\n"
		"		uint cc = bpmem_combiners[stage].x;\n"
		"		uint ac = bpmem_combiners[stage].y;\n"
		"		uint order = bpmem_tevorder[stage>>1];\n"
		"		if ((stage & 1u) == 1u)\n"
		"			order = order >> %d;\n\n", TwoTevStageOrders().enable1.offset - TwoTevStageOrders().enable0.offset);

	// TODO: Indirect Texturing
	out.Write("\t\t// TODO: Indirect textures\n\n");

	// Disable texturing when there are no texgens (for now)
	if (numTexgen != 0)
	{
	out.Write(
		"		// Sample texture for stage\n"
		"		int4 texColor;\n"
		"		if((order & %du) != 0u) {\n", 1 << TwoTevStageOrders().enable0.offset);
	out.Write(
		"			// Texture is enabled\n"
		"			uint sampler_num = %s;\n", BitfieldExtract("order", TwoTevStageOrders().texmap0).c_str());
	out.Write(
		"			uint tex_coord = %s;\n", BitfieldExtract("order", TwoTevStageOrders().texcoord0).c_str());
	out.Write(
		"\n"
		"			// TODO: there is an optional perspective divide here (not to mention all of indirect)\n"
		"			int2 fixedPoint_uv = itrunc(tex[tex_coord].xy * " I_TEXDIMS"[sampler_num].zw * 128.0);\n"
		"			float2 uv = (float2(fixedPoint_uv) / 128.0) * " I_TEXDIMS"[sampler_num].xy;\n"
		"\n"
		"			texColor = sampleTexture(sampler_num, uv);\n"
		"		} else {\n"
		"			// Texture is disabled\n"
		"			texColor = int4(255, 255, 255, 255);\n"
		"		}\n"
		"		// TODO: color channel swapping\n"
		"		s.TexColor = texColor;\n"
		"\n");
	}

	out.Write(
		"		// Set Konst for stage\n"
		"		// TODO: a switch case might be better here than an dynamically indexed uniform lookup\n"
		"		uint tevksel = bpmem_tevksel[stage>>1];\n"
		"		if ((stage & 1u) == 0u)\n"
		"			s.KonstColor = int4(konstLookup[%s].rgb, konstLookup[%s].a);\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel0).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel0).c_str());
	out.Write(
		"		else\n"
		"			s.KonstColor = int4(konstLookup[%s].rgb, konstLookup[%s].a);\n\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel1).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel1).c_str());
	out.Write(
		"\n");

	out.Write(
		"		// Set Ras for stage\n"
		"		int4 ras;\n"
		"		switch (%s) {\n", BitfieldExtract("order", TwoTevStageOrders().colorchan0).c_str());
	out.Write(
		"		case 0u: // Color 0\n"
		"			ras = icolors_0;\n"
		"			break;\n"
		"		case 1u: // Color 1\n"
		"			ras = icolors_1;\n"
		"			break;\n"
		"		case 5u: // Alpha Bump\n"
		"			ras = int4(AlphaBump, AlphaBump, AlphaBump, AlphaBump);\n"
		"			break;\n"
		"		case 6u: // Normalzied Alpha Bump\n"
		"			int normalized = AlphaBump | AlphaBump >> 5;\n"
		"			ras = int4(normalized, normalized, normalized, normalized);\n"
		"			break;\n"
		"		default:\n"
		"			ras = int4(0, 0, 0, 0);\n"
		"			break;\n"
		"		}\n"
		"		// TODO: color channel swapping\n"
		"		s.RasColor = ras;\n"
		"\n");

	out.Write(
		"		// This is the Meat of TEV\n"
		"		{\n"
		"			// Color Combiner\n");
	out.Write("\t\t\tuint color_a = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.a).c_str());
	out.Write("\t\t\tuint color_b = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.b).c_str());
	out.Write("\t\t\tuint color_c = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.c).c_str());
	out.Write("\t\t\tuint color_d = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.d).c_str());

	out.Write("\t\t\tuint color_bias = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.bias).c_str());
	out.Write("\t\t\tbool color_op = bool(%s);\n", BitfieldExtract("cc", TevStageCombiner().colorC.op).c_str());
	out.Write("\t\t\tbool color_clamp = bool(%s);\n", BitfieldExtract("cc", TevStageCombiner().colorC.clamp).c_str());
	out.Write("\t\t\tuint color_shift = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.shift).c_str());
	out.Write("\t\t\tuint color_dest = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.dest).c_str());

	out.Write(
		"			uint color_compare_op = color_shift << 1 | uint(color_op);\n"
		"\n"
		"			int3 color_A = selectColorInput(s, color_a) & int3(255, 255, 255);\n"
		"			int3 color_B = selectColorInput(s, color_b) & int3(255, 255, 255);\n"
		"			int3 color_C = selectColorInput(s, color_c) & int3(255, 255, 255);\n"
		"			int3 color_D = selectColorInput(s, color_d);  // 10 bits + sign\n" // TODO: do we need to sign extend?
		"\n"
		"			int3 color;\n"
		"			if(color_bias != 3u) { // Normal mode\n"
		"				color.r = tevLerp(color_A.r, color_B.r, color_C.r, color_D.r, color_bias, color_op, color_shift);\n"
		"				color.g = tevLerp(color_A.g, color_B.g, color_C.g, color_D.g, color_bias, color_op, color_shift);\n"
		"				color.b = tevLerp(color_A.b, color_B.b, color_C.b, color_D.b, color_bias, color_op, color_shift);\n"
		"			} else { // Compare mode\n"
		"				// op 6 and 7 do a select per color channel\n"
		"				if (color_compare_op == 6u) {\n"
		"					// TEVCMP_RGB8_GT\n"
		"					color.r = (color_A.r > color_B.r) ? color_C.r : 0;\n"
		"					color.g = (color_A.g > color_B.g) ? color_C.g : 0;\n"
		"					color.b = (color_A.b > color_B.b) ? color_C.b : 0;\n"
		"				} else if (color_compare_op == 7u) {\n"
		"					// TEVCMP_RGB8_EQ\n"
		"					color.r = (color_A.r == color_B.r) ? color_C.r : 0;\n"
		"					color.g = (color_A.g == color_B.g) ? color_C.g : 0;\n"
		"					color.b = (color_A.b == color_B.b) ? color_C.b : 0;\n"
		"				} else {\n"
		"					// The remaining ops do one compare which selects all 3 channels\n"
		"					color = tevCompare(color_compare_op, color_A, color_B) ? color_C : int3(0, 0, 0);\n"
		"				}\n"
		"				color = color_D + color;\n"
		"			}\n"
		"\n"
		"			// Clamp result\n"
		"			if (color_clamp)\n"
		"				color = clamp(color, 0, 255);\n"
		"			else\n"
		"				color = clamp(color, -1024, 1023);\n"
		"\n"
		"			if (stage == num_stages) { // If this is the last stage\n"
		"				// Write result to output\n"
		"				TevResult.rgb = color;\n"
		"			} else {\n"
		"				// Write result to the correct input register of the next stage\n"
		"				setRegColor(s, color_dest, color);\n"
		"			}\n"
		"\n");

	// Alpha combiner
	out.Write(
		"		// Alpha Combiner\n");
	out.Write("\t\t\tuint alpha_a = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.a).c_str());
	out.Write("\t\t\tuint alpha_b = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.b).c_str());
	out.Write("\t\t\tuint alpha_c = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.c).c_str());
	out.Write("\t\t\tuint alpha_d = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.d).c_str());

	out.Write("\t\t\tuint alpha_bias = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.bias).c_str());
	out.Write("\t\t\tbool alpha_op = bool(%s);\n", BitfieldExtract("ac", TevStageCombiner().alphaC.op).c_str());
	out.Write("\t\t\tbool alpha_clamp = bool(%s);\n", BitfieldExtract("ac", TevStageCombiner().alphaC.clamp).c_str());
	out.Write("\t\t\tuint alpha_shift = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.shift).c_str());
	out.Write("\t\t\tuint alpha_dest = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.dest).c_str());

	out.Write(
		"			uint alpha_compare_op = alpha_shift << 1 | uint(alpha_op);\n"
		"\n"
		"			int alpha_A;"
		"			int alpha_B;"
		"			if (alpha_bias != 3u || alpha_compare_op > 5u) {\n"
		"				// Small optimisation here: alpha_A and alpha_B are unused by compare ops 0-5\n"
		"				alpha_A = selectAlphaInput(s, alpha_a) & 255;\n"
		"				alpha_B = selectAlphaInput(s, alpha_b) & 255;\n"
		"			};\n"
		"			int alpha_C = selectAlphaInput(s, alpha_c) & 255;\n"
		"			int alpha_D = selectAlphaInput(s, alpha_d); // 10 bits + sign\n" // TODO: do we need to sign extend?
		"\n"
		"			int alpha;\n"
		"			if(alpha_bias != 3u) { // Normal mode\n"
		"				alpha = tevLerp(alpha_A, alpha_B, alpha_C, alpha_D, alpha_bias, alpha_op, alpha_shift);\n"
		"			} else { // Compare mode\n"
		"				if (alpha_compare_op == 6u) {\n"
		"					// TEVCMP_A8_GT\n"
		"					alpha = (alpha_A > alpha_B) ? alpha_C : 0;\n"
		"				} else if (alpha_compare_op == 7u) {\n"
		"					// TEVCMP_A8_EQ\n"
		"					alpha = (alpha_A == alpha_B) ? alpha_C : 0;\n"
		"				} else {\n"
		"					// All remaining alpha compare ops actually compare the color channels\n"
		"					alpha = tevCompare(alpha_compare_op, color_A, color_B) ? alpha_C : 0;\n"
		"				}\n"
		"				alpha = alpha_D + alpha;\n"
		"			}\n"
		"\n"
		"			// Clamp result\n"
		"			if (alpha_clamp)\n"
		"				alpha = clamp(alpha, 0, 255);\n"
		"			else\n"
		"				alpha = clamp(alpha, -1024, 1023);\n"
		"\n"
		"			if (stage == num_stages) { // If this is the last stage\n"
		"				// Write result to output\n"
		"				TevResult.a = alpha;\n"
		"				break;\n"
		"			} else {\n"
		"				// Write result to the correct input register of the next stage\n"
		"				setRegAlpha(s, alpha_dest, alpha);\n"
		"			}\n"
		"		}\n");

	out.Write(
		"	} // Main tev loop\n"
		"\n");

	// TODO: Depth textures

	// TODO: Optimise the value of bpmem_alphatest so it's zero when there is no test to do?
	out.Write(
		"	// Alpha Test\n"
		"	bool comp0 = alphaCompare(TevResult.a, " I_ALPHA".r, %s);\n", BitfieldExtract("bpmem_alphaTest", AlphaTest().comp0).c_str());
	out.Write(
		"	bool comp1 = alphaCompare(TevResult.a, " I_ALPHA".g, %s);\n", BitfieldExtract("bpmem_alphaTest", AlphaTest().comp1).c_str());
	out.Write(
		"\n"
		"	// These if statements are written weirdly to work around intel and qualcom bugs with handling booleans.\n"
		"	switch (%s) {\n", BitfieldExtract("bpmem_alphaTest", AlphaTest().logic).c_str());
	out.Write(
		"	case 0u: // AND\n"
		"		if (comp0 && comp1) break; else discard; break;\n"
		"	case 1u: // OR\n"
		"		if (comp0 || comp1) break; else discard; break;\n"
		"	case 2u: // XOR\n"
		"		if (comp0 != comp1) break; else discard; break;\n"
		"	case 3u: // XNOR\n"
		"		if (comp0 == comp1) break; else discard; break;\n"
		"	}\n");

	// FIXME: Fog is implemented the same as ShaderGen, but ShaderGen's fog is all hacks.
	//        Should be fixed point, and should not make guesses about Range-Based adjustments.
	out.Write(
		"	// Fog\n"
		"	uint fog_function = %s;\n", BitfieldExtract("bpmem_fogParam3", FogParam3().fsel).c_str());
	out.Write(
		"	if (fog_function != 0u) {\n"
		"		// TODO: This all needs to be converted from float to fixed point\n"
		"\n"
		"		// TODO: zCoord is hardcoded to fast depth with no zfreeze\n"
		"		int zCoord = " I_ZBIAS"[1].x + int((clipPos.z / clipPos.w) * float(" I_ZBIAS"[1].y));\n"
		"\n"
		"		float ze;\n"
		"		if (%s == 0u) {\n", BitfieldExtract("bpmem_fogParam3", FogParam3().proj).c_str());
	out.Write(
		"			// perspective\n"
		"			// ze = A/(B - (Zs >> B_SHF)\n"
		"			ze = (" I_FOGF"[1].x * 16777216.0) / float(" I_FOGI".y - (zCoord >> " I_FOGI".w));\n"
		"		} else {\n"
		"			// orthographic\n"
		"			// ze = a*Zs    (here, no B_SHF)\n"
		"			ze = " I_FOGF"[1].x * float(zCoord) / 16777216.0;\n"
		"		}\n"
		"\n"
		"		if (bool(%s)) {\n", BitfieldExtract("bpmem_fogRangeBase", FogRangeParams::RangeBase().Enabled).c_str());
	out.Write(
		"			// x_adjust = sqrt((x-center)^2 + k^2)/k\n"
		"			// ze *= x_adjust\n"
		"			// TODO Instead of this theoretical calculation, we should use the\n"
		"			//      coefficient table given in the fog range BP registers!\n"
		"			float x_adjust = (2.0 * (rawpos.x / " I_FOGF"[0].y)) - 1.0 - " I_FOGF"[0].x; \n"
		"			x_adjust = sqrt(x_adjust * x_adjust + " I_FOGF"[0].z * " I_FOGF"[0].z) / " I_FOGF"[0].z;\n"
		"			ze *= x_adjust;\n"
		"		}\n"
		"\n"
		"		float fog = clamp(ze - " I_FOGF"[1].z, 0.0, 1.0);\n"
		"\n"
		"		if (fog_function > 3u) {\n"
		"			switch (fog_function) {\n"
		"			case 4u:\n"
		"				fog = 1.0 - exp2(-8.0 * fog);\n"
		"				break;\n"
		"			case 5u:\n"
		"				fog = 1.0 - exp2(-8.0 * fog * fog);\n"
		"				break;\n"
		"			case 6u:\n"
		"				fog = exp2(-8.0 * (1.0 - fog));\n"
		"				break;\n"
		"			case 7u:\n"
		"				fog = 1.0 - fog;\n"
		"				fog = exp2(-8.0 * fog * fog);\n"
		"				break;\n"
		"			}\n"
		"		}\n"
		"\n"
		"		int ifog = iround(fog * 256.0);\n"
		"		TevResult.rgb = (TevResult.rgb * (256 - ifog) + " I_FOGCOLOR".rgb * ifog) >> 8;\n"
		"	}\n"
		"\n");

	out.Write(
		"	ocol0 = float4(TevResult) / 255.0;\n"
		"\n");
	// Use dual-source color blending to perform dst alpha in a single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		// Colors will be blended against the alpha from ocol1 and
		// the alpha from ocol0 will be written to the framebuffer.
		out.Write(
			"	// dual source blending\n"
			"	ocol1 = float4(TevResult) / 255.0;\n"
			"	ocol0.a = float(" I_ALPHA".a) / 255.0;\n"
			"\n");
	}

	out.Write("}");

	// TODO: This is the worst idea ever.
	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("Pixel UberShader - buffer too small, canary has been eaten!");
	return out;
}


}
