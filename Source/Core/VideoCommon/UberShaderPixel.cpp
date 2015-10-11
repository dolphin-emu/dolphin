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
	out.Write(
		"layout(std140, binding = 4) uniform UBERBlock {\n"
		"	uint	bpmem_genmode;\n"
		"	uint	bpmem_tevorder[8];\n"
		"	uint2	bpmem_combiners[16];\n"
		"	uint	bpmem_tevksel[8];\n"
		"	int4   konstLookup[32];\n"
		"	float4  debug;\n"
		"};\n");

	// TODO: Per pixel lighting (not really needed)

	// TODO: early depth tests (we will need multiple shaders)

	if (ApiType == API_OPENGL)
	{
		out.Write("out vec4 ocol0;\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("out vec4 ocol1;\n");

		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");

		//if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		//{
			out.Write("in VertexData {\n");
			GenerateVSOutputMembers(out, ApiType, numTexgen, false, GetInterpolationQualifier(ApiType, true, true));

			//if (g_ActiveConfig.iStereoMode > 0)
			//	out.Write("\tflat int layer;\n");

			out.Write("};\n");
		//}
		//else
		//{
		//	out.Write("%s in float4 colors_0;\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
		//	out.Write("%s in float4 colors_1;\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
			// compute window position if needed because binding semantic WPOS is not widely supported
			// Let's set up attributes
		//	for (unsigned int i = 0; i < numTexgen; ++i)
		//	{
		//		out.Write("%s in float3 uv%d;\n", GetInterpolationQualifier(ApiType, msaa, ssaa), i);
		//	}
		//	out.Write("%s in float4 clipPos;\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
		//	if (g_ActiveConfig.bEnablePixelLighting)
		//	{
		//		out.Write("%s in float3 Normal;\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
		//		out.Write("%s in float3 WorldPos;\n", GetInterpolationQualifier(ApiType, msaa, ssaa));
		//	}
		//}

		out.Write("void main()\n{\n");

		//if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		//{
		//	for (unsigned int i = 0; i < numTexgen; ++i)
		//		out.Write("\tfloat3 uv%d = tex%d;\n", i, i);
		//}

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
		for (unsigned int i = 0; i < numTexgen; ++i)
			out.Write(",\n  in %s float3 uv%d : TEXCOORD%d", GetInterpolationQualifier(ApiType, msaa, ssaa), i, i);
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
		"	// Main tev loop\n"
		"	for(uint stage=0; stage <= num_stages; stage++)\n"
		"	{\n"
		"		uint cc = bpmem_combiners[stage].x;\n"
		"		uint ac = bpmem_combiners[stage].y;\n"
		"		uint order = bpmem_tevorder[stage>>1];\n"
		"		if ((stage & 1u) == 1u)\n"
		"			order = order >> %d;\n\n", TwoTevStageOrders().enable1.offset - TwoTevStageOrders().enable0.offset);

	// TODO: like all of Texturing
	out.Write("\t\t// TODO: Like all of Texturing\n\n");

	out.Write(
		"		// Set Konst for stage\n"
		"		uint tevksel = bpmem_tevksel[stage>>1];\n"
		"		int4 konst;\n"
		"		if ((stage & 1) == 0)\n"
		"			konst = int4(konstLookup[%s].rgb, konstLookup[%s]);\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel0).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel0).c_str());
	out.Write(
		"		else\n"
		"			konst = int4(konstLookup[%s].rgb, konstLookup[%s]);\n\n",
		BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel1).c_str(),
		BitfieldExtract("tevksel", bpmem.tevksel[0].kasel1).c_str());
	out.Write(
		"		ColorInput[8] = konst.rgb;\n"
		"		ColorInput[9] = konst.aaa;\n"
		"		AlphaInput[4] = konst.a;\n"
		"\n");

	out.Write(
		"		// Set Ras for stage\n"
		"		int4 ras;\n"
		"		switch (%s) {\n", BitfieldExtract("order", TwoTevStageOrders().colorchan0).c_str());
	out.Write(
		"		case 0: // Color 0\n"
		"			ras = icolors_0;\n"
		"			break;\n"
		"		case 1: // Color 1\n"
		"			ras = icolors_1;\n"
		"			break;\n"
		"		case 5: // Alpha Bump\n"
		"			ras = int4(AlphaBump);\n"
		"			break;\n"
		"		case 6: // Normalzied Alpha Bump\n"
		"			ras = int4(AlphaBump | AlphaBump >> 5);\n"
		"			break;\n"
		"		default:\n"
		"			ras = int4(0);\n"
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
		"			if(bias != 3) { // Normal mode\n"
		"				// Lerp A and B with C\n"
		"				C += C >> 7; // Scale C from 0..255 to 0..256\n"
		"				int3 lerp = (A << 8) + (B - A)*C;\n"
		"				if (shift != 3) {\n"
		"					lerp = lerp << shift;\n"
		"					lerp = lerp + (op ? 127 : 128);\n"
		"				}\n"
		"				result = lerp >> 8;\n"
		"\n"
		"				// Add/Subtract D (and bias)\n"
		"				if (bias == 1) result += 128;\n"
		"				else if (bias == 2) result -= 128;\n"
		"				if(!op) // Add\n"
		"					result = D + result;\n"
		"				else // Subtract\n"
		"					result = D - result;\n"
		"\n"
		"				// Most of the Shift was moved inside the lerp for improved percision\n"
		"				// But we still do the divide by 2 here\n"
		"				if (shift == 3)\n"
		"					result = result >> 1;\n"
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
		"			} else {\n"
		"				// Write result to the correct input register of the next stage\n"
		"				ColorInput[dest<<1] = result;\n"
		"			}\n"
		"		}\n");

	// TODO: Alpha combiner (we should make the code above more generic)

	out.Write(
		"	} // Main tev loop\n"
		"\n"
		"	TevResult.a = 255;\n"
		"	float4 newColor = float4(TevResult) / 255.0;\n"
		"\n");

	// TODO: Fog, Depth textures

	out.Write(
		"	ocol0 = newColor; //float4(TevResult) / 255.0;\n"
		"}\n");

	return out;
}


}
