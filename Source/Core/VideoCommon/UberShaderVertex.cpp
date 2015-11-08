// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace UberShader
{

ShaderCode GenVertexShader(API_TYPE ApiType)
{
	ShaderCode out;
	const u32 components = VertexLoaderManager::g_current_components;

	out.Write("// Vertex UberShader\n\n");

	// Bad
	u32 numTexgen = xfmem.numTexGen.numTexGens;


	out.Write("%s", s_lighting_struct);

	// uniforms
	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
	else
		out.Write("cbuffer VSBlock {\n");
	out.Write(s_shader_uniforms);
	out.Write("};\n");

	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers(out, ApiType, numTexgen, false);
	out.Write("};\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("in float4 rawpos; // ATTR%d,\n", SHADER_POSITION_ATTRIB);
		out.Write("in int posmtx; // ATTR%d,\n", SHADER_POSMTX_ATTRIB);
		out.Write("in float3 rawnorm0; // ATTR%d,\n", SHADER_NORM0_ATTRIB);
		out.Write("in float3 rawnorm1; // ATTR%d,\n", SHADER_NORM1_ATTRIB);
		out.Write("in float3 rawnorm2; // ATTR%d,\n", SHADER_NORM2_ATTRIB);

		out.Write("in float4 color0; // ATTR%d,\n", SHADER_COLOR0_ATTRIB);
		out.Write("in float4 color1; // ATTR%d,\n", SHADER_COLOR1_ATTRIB);

		for (int i = 0; i < 8; ++i)
		{
			out.Write("in float3 tex%d; // ATTR%d,\n", i, SHADER_TEXTURE0_ATTRIB + i);
		}

		// TODO: No Geometery shader fallback.
		out.Write("out VertexData {\n");
		GenerateVSOutputMembers(out, ApiType, xfmem.numTexGen.numTexGens, false, GetInterpolationQualifier(ApiType, false, true));
		out.Write("} vs;\n");

		out.Write("void main()\n{\n");
	}
	else // D3D
	{
		out.Write("VS_OUTPUT main(\n");

		// inputs
		out.Write("  float3 rawnorm0 : NORMAL0,\n");
		out.Write("  float3 rawnorm1 : NORMAL1,\n");
		out.Write("  float3 rawnorm2 : NORMAL2,\n");
		out.Write("  float4 color0 : COLOR0,\n");
		out.Write("  float4 color1 : COLOR1,\n");
		for (int i = 0; i < 8; ++i)
		{
			out.Write("  float3 tex%d : TEXCOORD%d,\n", i, i);
		}
		out.Write("  int posmtx : BLENDINDICES,\n");
		out.Write("  float4 rawpos : POSITION) {\n");
	}

	out.Write(
		"	VS_OUTPUT o;\n"
		"\n");

	// Transforms
	out.Write(
		"	// Position matrix\n"
		"	float4 P0;\n"
		"	float4 P1;\n"
		"	float4 P2;\n"
		"\n"
		"	// Normal matrix\n"
		"	float3 N0;\n"
		"	float3 N1;\n"
		"	float3 N2;\n"
		"\n"
		"	if ((components & %uu) != 0u) {// VB_HAS_POSMTXIDX\n", VB_HAS_POSMTXIDX);
	out.Write(
		"		// Vertex format has a per-vertex matrix\n"
		"		P0 = " I_TRANSFORMMATRICES"[posmtx];\n"
		"		P1 = " I_TRANSFORMMATRICES"[posmtx+1];\n"
		"		P2 = " I_TRANSFORMMATRICES"[posmtx+2];\n"
		"\n"
		"		int normidx = posmtx >= 32 ? (posmtx - 32) : posmtx;\n"
		"		N0 = " I_NORMALMATRICES"[normidx].xyz;\n"
		"		N1 = " I_NORMALMATRICES"[normidx+1].xyz;\n"
		"		N2 = " I_NORMALMATRICES"[normidx+2].xyz;\n"
		"	} else {\n"
		"		// One shared matrix\n"
		"		P0 = " I_POSNORMALMATRIX"[0];\n"
		"		P1 = " I_POSNORMALMATRIX"[1];\n"
		"		P2 = " I_POSNORMALMATRIX"[2];\n"
		"		N0 = " I_POSNORMALMATRIX"[3].xyz;\n"
		"		N1 = " I_POSNORMALMATRIX"[4].xyz;\n"
		"		N2 = " I_POSNORMALMATRIX"[5].xyz;\n"
		"	}\n"
		"\n"
		"	float4 pos = float4(dot(P0, rawpos), dot(P1, rawpos), dot(P2, rawpos), 1.0);\n"
		"	o.pos = float4(dot(" I_PROJECTION"[0], pos), dot(" I_PROJECTION"[1], pos), dot(" I_PROJECTION"[2], pos), dot(" I_PROJECTION"[3], pos));\n"
		"\n"
		"	// Only the first normal gets normalized (TODO: why?)\n"
		"	float3 _norm0 = float3(0.0, 0.0, 0.0);\n"
		"	if ((components & %uu) != 0u) // VB_HAS_NRM0\n", VB_HAS_NRM0);
	out.Write(
		"		_norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n"
		"\n"
		"	float3 _norm1 = float3(0.0, 0.0, 0.0);\n"
		"	if ((components & %uu) != 0u) // VB_HAS_NRM1\n", VB_HAS_NRM1);
	out.Write(
		"		_norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n"
		"\n"
		"	float3 _norm2 = float3(0.0, 0.0, 0.0);\n"
		"	if ((components & %uu) != 0u) // VB_HAS_NRM2\n", VB_HAS_NRM2);
	out.Write(
		"		_norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n"
		"\n");

	// Lighting (basic color passthrough for now)
	out.Write(
		"	if ((components & %uu) != 0u) // VB_HAS_COL0\n", VB_HAS_COL0);
	out.Write(
		"		o.colors_0 = color0;\n"
		"	else\n"
		"		o.colors_0 = float4(1.0, 1.0, 1.0, 1.0);\n"
		"\n"
		"	if ((components & %uu) != 0u) // VB_HAS_COL1\n", VB_HAS_COL1);
	out.Write(
		"		o.colors_1 = color1;\n"
		"	else\n"
		"		o.colors_1 = o.colors_0;\n"
		"\n");

	// TODO: Actual Hardware Lighting

	// TODO: Texture Coordinates

	// Zero tex coords (if you don't assign to them, you get random data in opengl.)
	for (u32 i = 0; i < numTexgen; i++)
	{
		out.Write(
			"	o.tex[%i] = float3(0.0, 0.0, 1.0);\n", i);
	}
	// clipPos/w needs to be done in pixel shader, not here
	out.Write("	o.clipPos = o.pos;\n");

	// TODO: Per Vertex Lighting?

	//write the true depth value, if the game uses depth textures pixel shaders will override with the correct values
	//if not early z culling will improve speed
	if (g_ActiveConfig.backend_info.bSupportsClipControl)
	{
		out.Write("	o.pos.z = -o.pos.z;\n");
	}
	else // OGL
	{
		// this results in a scale from -1..0 to -1..1 after perspective
		// divide
		out.Write("	o.pos.z = o.pos.z * -2.0 - o.pos.w;\n");

		// the next steps of the OGL pipeline are:
		// (x_c,y_c,z_c,w_c) = o.pos  //switch to OGL spec terminology
		// clipping to -w_c <= (x_c,y_c,z_c) <= w_c
		// (x_d,y_d,z_d) = (x_c,y_c,z_c)/w_c//perspective divide
		// z_w = (f-n)/2*z_d + (n+f)/2
		// z_w now contains the value to go to the 0..1 depth buffer

		//trying to get the correct semantic while not using glDepthRange
		//seems to get rather complicated
	}

	// The console GPU places the pixel center at 7/12 in screen space unless
	// antialiasing is enabled, while D3D and OpenGL place it at 0.5. This results
	// in some primitives being placed one pixel too far to the bottom-right,
	// which in turn can be critical if it happens for clear quads.
	// Hence, we compensate for this pixel center difference so that primitives
	// get rasterized correctly.
	out.Write("	o.pos.xy = o.pos.xy - o.pos.w * " I_PIXELCENTERCORRECTION".xy;\n");

	if (ApiType == API_OPENGL)
	{
		// TODO: No Geometery shader fallback.
		AssignVSOutputMembers(out, "vs", "o", numTexgen, false);

		out.Write("	gl_Position = o.pos;\n");
	}
	else // D3D
	{
		out.Write("	return o;\n");
	}
	out.Write("}\n");

	return out;
}


}