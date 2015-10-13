// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/UberShaderPixel.h"

namespace UberShader
{

template<typename T>
std::string BitfieldExtract(const std::string& source, T type)
{
	return StringFromFormat("bitfieldExtract(%s, %u, %u)", source.c_str(), type.offset, type.size);
}

ShaderCode GenPixelShader(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, bool per_pixel_depth, bool msaa, bool ssaa)
{
	ShaderCode out;

	out.Write("// Pixel UberShader\n");
	WritePixelShaderCommonHeader(out, ApiType);

	// Bad
	u32 numTexgen = xfmem.numTexGen.numTexGens;

	// TODO: This is variable based on number of texcoord gens
	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers(out, ApiType, numTexgen, false);
	out.Write("};\n");

	// TEV constants
	if (ApiType == API_OPENGL) out.Write(
		"layout(std140, binding = 4) uniform UBERBlock {\n");
	else out.Write(
		"cbuffer UBERBlock : register(b1) {\n");
	out.Write(
		"	uint	bpmem_genmode;\n"
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
			"	switch(sampler_num & 0x7u) {\n");
		for (int i=0; i < 8; i++)
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

	// ==========
	//    Lerp
	// ==========

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
		"	if(!op) // Add\n"
		"		result = D + result;\n"
		"	else // Subtract\n"
		"		result = D - result;\n"
		"\n"
		"	// Most of the Shift was moved inside the lerp for improved percision\n"
		"	// But we still do the divide by 2 here\n"
		"	if (shift == 3u)\n"
		"		result = result >> 1;\n"
		"	return result;\n"
		"}\n\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("out vec4 ocol0;\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("out vec4 ocol1;\n");

		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");

		out.Write("in VertexData {\n");
		GenerateVSOutputMembers(out, ApiType, numTexgen, false, GetInterpolationQualifier(ApiType, true, true));

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

		out.Write("  in %s float4 colors_0 : COLOR0,\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
		out.Write("  in %s float4 colors_1 : COLOR1\n", GetInterpolationQualifier(ApiType, msaa, ssaa));

		// compute window position if needed because binding semantic WPOS is not widely supported
		if (numTexgen > 0)
			out.Write(",\n  in %s float3 tex[%d] : TEXCOORD0", GetInterpolationQualifier(ApiType, msaa, ssaa), numTexgen);
		out.Write(",\n  in %s float4 clipPos : TEXCOORD%d", GetInterpolationQualifier(ApiType, msaa, ssaa), numTexgen);
		if (g_ActiveConfig.bEnablePixelLighting)
		{
			out.Write(",\n  in %s float3 Normal : TEXCOORD%d", GetInterpolationQualifier(ApiType, msaa, ssaa), numTexgen + 1);
			out.Write(",\n  in %s float3 WorldPos : TEXCOORD%d", GetInterpolationQualifier(ApiType, msaa, ssaa), numTexgen + 2);
		}
		if (g_ActiveConfig.iStereoMode > 0)
			out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
		out.Write("        ) {\n");
	}

	// input lookup arrays
	out.Write(
		"	int3 ColorInput[16];\n"
		"	// ColorInput initial state:\n"
		"	ColorInput[0]  = " I_COLORS"[0].rgb;\n"
		"	ColorInput[1]  = " I_COLORS"[0].aaa;\n"
		"	ColorInput[2]  = " I_COLORS"[1].rgb;\n"
		"	ColorInput[3]  = " I_COLORS"[1].aaa;\n"
		"	ColorInput[4]  = " I_COLORS"[2].rgb;\n"
		"	ColorInput[5]  = " I_COLORS"[2].aaa;\n"
		"	ColorInput[6]  = " I_COLORS"[3].rgb;\n"
		"	ColorInput[7]  = " I_COLORS"[3].aaa;\n"
		"	ColorInput[8]  = int3(0, 0, 0); // TexColor.rgb (uninitilized)\n"
		"	ColorInput[9]  = int3(0, 0, 0); // TexColor.aaa (uninitilized)\n"
		"	ColorInput[10] = int3(0, 0, 0); // RasColor.rgb (uninitilized)\n"
		"	ColorInput[11] = int3(0, 0, 0); // RasColor.aaa (uninitilized)\n"
		"	ColorInput[12] = int3(255, 255, 255); // One constant\n"
		"	ColorInput[13] = int3(128, 128, 128); // Half constant\n"
		"	ColorInput[14] = int3(0, 0, 0); // KonstColor.rgb (unititilized)\n"
		"	ColorInput[15] = int3(0, 0, 0); // Zero constant\n"
		"\n"
		"	int AlphaInput[8];\n"
		"	// AlphaInput's intial state:\n"
		"	AlphaInput[0] = " I_COLORS"[0].a;\n"
		"	AlphaInput[1] = " I_COLORS"[1].a;\n"
		"	AlphaInput[2] = " I_COLORS"[2].a;\n"
		"	AlphaInput[3] = " I_COLORS"[3].a;\n"
		"	AlphaInput[4] = 0; // TexColor.a (uninitilized)\n"
		"	AlphaInput[5] = 0; // RasColor.a (uninitilized)\n"
		"	AlphaInput[6] = 0; // KostColor.a (uninitilized)\n"
		"	AlphaInput[7] = 0; // Zero constant\n"
		"\n");

	out.Write(
		"	int AlphaBump = 0;\n"
		"	int4 icolors_0 = int4(colors_0 * 255.0);\n"
		"	int4 icolors_1 = int4(colors_1 * 255.0);\n"
		"	int4 TevResult = " I_COLORS"[0];\n"
		"\n");

	out.Write("	uint num_stages = %s;\n", BitfieldExtract("bpmem_genmode", bpmem.genMode.numtevstages).c_str());

	out.Write(
		"	// Main tev loop\n");
	if (ApiType == API_D3D) out.Write(
		"	[loop]\n"); // Tell DirectX we don't want this loop unrolled.
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
		"		ColorInput[8] = texColor.rgb;\n"
		"		ColorInput[9] = texColor.aaa;\n"
		"		AlphaInput[4] = texColor.a;\n"
		"\n");
	}

	out.Write(
		"		// Set Konst for stage\n"
		"		uint tevksel = bpmem_tevksel[stage>>1];\n"
		"		int4 konst;\n"
		"		if ((stage & 1u) == 0u)\n"
		"			konst = int4(konstLookup[%s].rgb, konstLookup[%s].a);\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel0).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel0).c_str());
	out.Write(
		"		else\n"
		"			konst = int4(konstLookup[%s].rgb, konstLookup[%s].a);\n\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel1).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel1).c_str());
	out.Write(
		"		ColorInput[14] = konst.rgb;\n"
		"		AlphaInput[6] = konst.a;\n"
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
		"		ColorInput[10] = ras.rgb;\n"
		"		ColorInput[11] = ras.aaa;\n"
		"		AlphaInput[5]  = ras.a;\n"
		"\n");

	out.Write(
		"		// Color Combiner\n"
		"		{\n");
	out.Write("\t\t\tuint a = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.a).c_str());
	out.Write("\t\t\tuint b = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.b).c_str());
	out.Write("\t\t\tuint c = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.c).c_str());
	out.Write("\t\t\tuint d = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.d).c_str());

	out.Write("\t\t\tuint bias = %s;\n",  BitfieldExtract("cc", TevStageCombiner().colorC.bias).c_str());
	out.Write("\t\t\tbool op = bool(%s);\n",    BitfieldExtract("cc", TevStageCombiner().colorC.op).c_str());
	out.Write("\t\t\tbool _clamp = bool(%s);\n", BitfieldExtract("cc", TevStageCombiner().colorC.clamp).c_str());
	out.Write("\t\t\tuint shift = %s;\n", BitfieldExtract("cc", TevStageCombiner().colorC.shift).c_str());
	out.Write("\t\t\tuint dest = %s;\n",  BitfieldExtract("cc", TevStageCombiner().colorC.dest).c_str());

	out.Write(
		"\n"
		"			int3 A = ColorInput[a] & int3(255, 255, 255);\n"
		"			int3 B = ColorInput[b] & int3(255, 255, 255);\n"
		"			int3 C = ColorInput[c] & int3(255, 255, 255);\n"
		"			int3 D = ColorInput[d]; // 10 bits + sign\n" // TODO: do we need to sign extend?
		"\n"
		"			int3 result;\n"
		"			if(bias != 3u) { // Normal mode\n"
		"				result.r = tevLerp(A.r, B.r, C.r, D.r, bias, op, shift);\n"
		"				result.g = tevLerp(A.g, B.g, C.g, D.g, bias, op, shift);\n"
		"				result.b = tevLerp(A.b, B.b, C.b, D.b, bias, op, shift);\n"
		"			} else { // Compare mode\n"
		"				// Not implemented\n" // Not Implemented
		"				result = int3(255, 0, 0);\n"
		"			}\n"
		"\n"
		"			// Clamp result\n"
		"			if (_clamp)\n"
		"				result = clamp(result, 0, 255);\n"
		"			else\n"
		"				result = clamp(result, -1024, 1023);\n"
		"\n"
		"			if (stage == num_stages) { // If this is the last stage\n"
		"				// Write result to output\n"
		"				TevResult.rgb = result;\n"
		"				//break;\n"
		"			} else {\n"
		"				// Write result to the correct input register of the next stage\n"
		"				ColorInput[dest<<1] = result;\n"
		"			}\n"
		"		}\n");

	// Alpha combiner
	// TODO: we should make the above code slightly more generic instead of just copy/pasting
	out.Write(
		"		// Alpha Combiner\n"
		"		{\n");
	out.Write("\t\t\tuint a = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.a).c_str());
	out.Write("\t\t\tuint b = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.b).c_str());
	out.Write("\t\t\tuint c = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.c).c_str());
	out.Write("\t\t\tuint d = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.d).c_str());

	out.Write("\t\t\tuint bias = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.bias).c_str());
	out.Write("\t\t\tbool op = bool(%s);\n", BitfieldExtract("ac", TevStageCombiner().alphaC.op).c_str());
	out.Write("\t\t\tbool _clamp = bool(%s);\n", BitfieldExtract("ac", TevStageCombiner().alphaC.clamp).c_str());
	out.Write("\t\t\tuint shift = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.shift).c_str());
	out.Write("\t\t\tuint dest = %s;\n", BitfieldExtract("ac", TevStageCombiner().alphaC.dest).c_str());

	out.Write(
		"\n"
		"			int A = AlphaInput[a] & 255;\n"
		"			int B = AlphaInput[b] & 255;\n"
		"			int C = AlphaInput[c] & 255;\n"
		"			int D = AlphaInput[d]; // 10 bits + sign\n" // TODO: do we need to sign extend?
		"\n"
		"			int result;\n"
		"			if(bias != 3u) { // Normal mode\n"
		"				result = tevLerp(A, B, C, D, bias, op, shift);\n"
		"			} else { // Compare mode\n"
		"				// Not implemented\n" // Not Implemented
		"				result = 255;\n"
		"			}\n"
		"\n"
		"			// Clamp result\n"
		"			if (_clamp)\n"
		"				result = clamp(result, 0, 255);\n"
		"			else\n"
		"				result = clamp(result, -1024, 1023);\n"
		"\n"
		"			if (stage == num_stages) { // If this is the last stage\n"
		"				// Write result to output\n"
		"				TevResult.a = result;\n"
		"			} else {\n"
		"				// Write result to the correct input register of the next stage\n"
		"				AlphaInput[dest] = result;\n"
		"				ColorInput[(dest << 1) + 1u] = int3(result, result, result);\n"
		"			}\n"
		"		}\n");

	out.Write(
		"	} // Main tev loop\n"
		"\n");

	// TODO: Fog, Depth textures


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

	return out;
}


}
