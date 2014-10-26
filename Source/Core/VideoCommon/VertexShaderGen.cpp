// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

static char text[16768];

template<class T>
static void DefineVSOutputStructMember(T& object, API_TYPE api_type, const char* type, const char* name, int var_index, const char* semantic, int semantic_index = -1)
{
	object.Write("  %s %s", type, name);
	if (var_index != -1)
		object.Write("%d", var_index);

	if (api_type == API_OPENGL)
		object.Write(";\n");
	else // D3D
	{
		if (semantic_index != -1)
			object.Write(" : %s%d;\n", semantic, semantic_index);
		else
			object.Write(" : %s;\n", semantic);
	}
}

template<class T>
static inline void GenerateVSOutputStruct(T& object, API_TYPE api_type)
{
	object.Write("struct VS_OUTPUT {\n");
	DefineVSOutputStructMember(object, api_type, "float4", "pos", -1, "POSITION");
	DefineVSOutputStructMember(object, api_type, "float4", "colors_", 0, "COLOR", 0);
	DefineVSOutputStructMember(object, api_type, "float4", "colors_", 1, "COLOR", 1);

	for (unsigned int i = 0; i < xfmem.numTexGen.numTexGens; ++i)
		DefineVSOutputStructMember(object, api_type, "float3", "tex", i, "TEXCOORD", i);

	DefineVSOutputStructMember(object, api_type, "float4", "clipPos", -1, "TEXCOORD", xfmem.numTexGen.numTexGens);

	if (g_ActiveConfig.bEnablePixelLighting)
		DefineVSOutputStructMember(object, api_type, "float4", "Normal", -1, "TEXCOORD", xfmem.numTexGen.numTexGens + 1);

	if (g_ActiveConfig.bStereo)
		DefineVSOutputStructMember(object, api_type, "float4", "rawpos", -1, "POSITION");

	object.Write("};\n");
}

template<class T>
static inline void GenerateVertexShader(T& out, u32 components, API_TYPE api_type)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	vertex_shader_uid_data dummy_data;
	vertex_shader_uid_data* uid_data = out.template GetUidData<vertex_shader_uid_data>();
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

	_assert_(bpmem.genMode.numtexgens == xfmem.numTexGen.numTexGens);
	_assert_(bpmem.genMode.numcolchans == xfmem.numChan.numColorChans);

	out.Write("%s", s_lighting_struct);

	// uniforms
	if (api_type == API_OPENGL)
		out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
	else
		out.Write("cbuffer VSBlock {\n");
	out.Write(
		"\tfloat4 " I_POSNORMALMATRIX"[6];\n"
		"\tfloat4 " I_PROJECTION"[4];\n"
		"\tint4 " I_MATERIALS"[4];\n"
		"\tLight " I_LIGHTS"[8];\n"
		"\tfloat4 " I_TEXMATRICES"[24];\n"
		"\tfloat4 " I_TRANSFORMMATRICES"[64];\n"
		"\tfloat4 " I_NORMALMATRICES"[32];\n"
		"\tfloat4 " I_POSTTRANSFORMMATRICES"[64];\n"
		"\tfloat4 " I_DEPTHPARAMS";\n"
		"\tfloat4 " I_STEREOPROJECTION"[8];\n"
		"};\n");

	GenerateVSOutputStruct(out, api_type);

	uid_data->numTexGens = xfmem.numTexGen.numTexGens;
	uid_data->components = components;
	uid_data->pixel_lighting = g_ActiveConfig.bEnablePixelLighting;

	if (api_type == API_OPENGL)
	{
		out.Write("in float4 rawpos; // ATTR%d,\n", SHADER_POSITION_ATTRIB);
		if (components & VB_HAS_POSMTXIDX)
			out.Write("in int posmtx; // ATTR%d,\n", SHADER_POSMTX_ATTRIB);
		if (components & VB_HAS_NRM0)
			out.Write("in float3 rawnorm0; // ATTR%d,\n", SHADER_NORM0_ATTRIB);
		if (components & VB_HAS_NRM1)
			out.Write("in float3 rawnorm1; // ATTR%d,\n", SHADER_NORM1_ATTRIB);
		if (components & VB_HAS_NRM2)
			out.Write("in float3 rawnorm2; // ATTR%d,\n", SHADER_NORM2_ATTRIB);

		if (components & VB_HAS_COL0)
			out.Write("in float4 color0; // ATTR%d,\n", SHADER_COLOR0_ATTRIB);
		if (components & VB_HAS_COL1)
			out.Write("in float4 color1; // ATTR%d,\n", SHADER_COLOR1_ATTRIB);

		for (int i = 0; i < 8; ++i)
		{
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0<<i));
			if ((components & (VB_HAS_UV0<<i)) || hastexmtx)
				out.Write("in float%d tex%d; // ATTR%d,\n", hastexmtx ? 3 : 2, i, SHADER_TEXTURE0_ATTRIB + i);
		}


		if (g_ActiveConfig.bStereo)
			out.Write("centroid out VS_OUTPUT v;\n");
		else
			out.Write("centroid out VS_OUTPUT o;\n");

		out.Write("void main()\n{\n");

		if (g_ActiveConfig.bStereo)
			out.Write("VS_OUTPUT o;\n");
	}
	else // D3D
	{
		out.Write("VS_OUTPUT main(\n");

		// inputs
		if (components & VB_HAS_NRM0)
			out.Write("  float3 rawnorm0 : NORMAL0,\n");
		if (components & VB_HAS_NRM1)
			out.Write("  float3 rawnorm1 : NORMAL1,\n");
		if (components & VB_HAS_NRM2)
			out.Write("  float3 rawnorm2 : NORMAL2,\n");
		if (components & VB_HAS_COL0)
			out.Write("  float4 color0 : COLOR0,\n");
		if (components & VB_HAS_COL1)
			out.Write("  float4 color1 : COLOR1,\n");
		for (int i = 0; i < 8; ++i)
		{
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0<<i));
			if ((components & (VB_HAS_UV0<<i)) || hastexmtx)
				out.Write("  float%d tex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i, i);
		}
		if (components & VB_HAS_POSMTXIDX)
			out.Write("  int posmtx : BLENDINDICES,\n");
		out.Write("  float4 rawpos : POSITION) {\n");

		out.Write("VS_OUTPUT o;\n");
	}

	// transforms
	if (components & VB_HAS_POSMTXIDX)
	{
		if (is_writing_shadercode && (DriverDetails::HasBug(DriverDetails::BUG_NODYNUBOACCESS) && !DriverDetails::HasBug(DriverDetails::BUG_ANNIHILATEDUBOS)) )
		{
			// This'll cause issues, but  it can't be helped
			out.Write("float4 pos = float4(dot(" I_TRANSFORMMATRICES"[0], rawpos), dot(" I_TRANSFORMMATRICES"[1], rawpos), dot(" I_TRANSFORMMATRICES"[2], rawpos), 1);\n");
			if (components & VB_HAS_NRMALL)
				out.Write("float3 N0 = " I_NORMALMATRICES"[0].xyz, N1 = " I_NORMALMATRICES"[1].xyz, N2 = " I_NORMALMATRICES"[2].xyz;\n");
		}
		else
		{
			out.Write("float4 pos = float4(dot(" I_TRANSFORMMATRICES"[posmtx], rawpos), dot(" I_TRANSFORMMATRICES"[posmtx+1], rawpos), dot(" I_TRANSFORMMATRICES"[posmtx+2], rawpos), 1);\n");

			if (components & VB_HAS_NRMALL)
			{
				out.Write("int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
				out.Write("float3 N0 = " I_NORMALMATRICES"[normidx].xyz, N1 = " I_NORMALMATRICES"[normidx+1].xyz, N2 = " I_NORMALMATRICES"[normidx+2].xyz;\n");
			}
		}

		if (components & VB_HAS_NRM0)
			out.Write("float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			out.Write("float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			out.Write("float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
	}
	else
	{
		out.Write("float4 pos = float4(dot(" I_POSNORMALMATRIX"[0], rawpos), dot(" I_POSNORMALMATRIX"[1], rawpos), dot(" I_POSNORMALMATRIX"[2], rawpos), 1.0);\n");
		if (components & VB_HAS_NRM0)
			out.Write("float3 _norm0 = normalize(float3(dot(" I_POSNORMALMATRIX"[3].xyz, rawnorm0), dot(" I_POSNORMALMATRIX"[4].xyz, rawnorm0), dot(" I_POSNORMALMATRIX"[5].xyz, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			out.Write("float3 _norm1 = float3(dot(" I_POSNORMALMATRIX"[3].xyz, rawnorm1), dot(" I_POSNORMALMATRIX"[4].xyz, rawnorm1), dot(" I_POSNORMALMATRIX"[5].xyz, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			out.Write("float3 _norm2 = float3(dot(" I_POSNORMALMATRIX"[3].xyz, rawnorm2), dot(" I_POSNORMALMATRIX"[4].xyz, rawnorm2), dot(" I_POSNORMALMATRIX"[5].xyz, rawnorm2));\n");
	}

	if (!(components & VB_HAS_NRM0))
		out.Write("float3 _norm0 = float3(0.0, 0.0, 0.0);\n");


	out.Write("o.pos = float4(dot(" I_PROJECTION"[0], pos), dot(" I_PROJECTION"[1], pos), dot(" I_PROJECTION"[2], pos), dot(" I_PROJECTION"[3], pos));\n");

	if (g_ActiveConfig.bStereo)
		out.Write("o.rawpos = pos;\n");

	out.Write("int4 lacc;\n"
			"float3 ldir, h;\n"
			"float dist, dist2, attn;\n");

	uid_data->numColorChans = xfmem.numChan.numColorChans;
	if (xfmem.numChan.numColorChans == 0)
	{
		if (components & VB_HAS_COL0)
			out.Write("o.colors_0 = color0;\n");
		else
			out.Write("o.colors_0 = float4(1.0, 1.0, 1.0, 1.0);\n");
	}

	GenerateLightingShader<T>(out, uid_data->lighting, components, "color", "o.colors_");

	if (xfmem.numChan.numColorChans < 2)
	{
		if (components & VB_HAS_COL1)
			out.Write("o.colors_1 = color1;\n");
		else
			out.Write("o.colors_1 = o.colors_0;\n");
	}
	// special case if only pos and tex coord 0 and tex coord input is AB11
	// donko - this has caused problems in some games. removed for now.
	bool texGenSpecialCase = false;
	/*bool texGenSpecialCase =
		((g_main_cp_state.vtx_desc.Hex & 0x60600L) == g_main_cp_state.vtx_desc.Hex) && // only pos and tex coord 0
		(g_main_cp_state.vtx_desc.Tex0Coord != NOT_PRESENT) &&
		(xfmem.texcoords[0].texmtxinfo.inputform == XF_TEXINPUT_AB11);
		*/

	// transform texcoords
	out.Write("float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n");
	for (unsigned int i = 0; i < xfmem.numTexGen.numTexGens; ++i)
	{
		TexMtxInfo& texinfo = xfmem.texMtxInfo[i];

		out.Write("{\n");
		out.Write("coord = float4(0.0, 0.0, 1.0, 1.0);\n");
		uid_data->texMtxInfo[i].sourcerow = xfmem.texMtxInfo[i].sourcerow;
		switch (texinfo.sourcerow)
		{
		case XF_SRCGEOM_INROW:
			_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
			out.Write("coord = rawpos;\n"); // pos.w is 1
			break;
		case XF_SRCNORMAL_INROW:
			if (components & VB_HAS_NRM0)
			{
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				out.Write("coord = float4(rawnorm0.xyz, 1.0);\n");
			}
			break;
		case XF_SRCCOLORS_INROW:
			_assert_( texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1 );
			break;
		case XF_SRCBINORMAL_T_INROW:
			if (components & VB_HAS_NRM1)
			{
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				out.Write("coord = float4(rawnorm1.xyz, 1.0);\n");
			}
			break;
		case XF_SRCBINORMAL_B_INROW:
			if (components & VB_HAS_NRM2)
			{
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				out.Write("coord = float4(rawnorm2.xyz, 1.0);\n");
			}
			break;
		default:
			_assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
			if (components & (VB_HAS_UV0<<(texinfo.sourcerow - XF_SRCTEX0_INROW)) )
				out.Write("coord = float4(tex%d.x, tex%d.y, 1.0, 1.0);\n", texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);
			break;
		}

		// first transformation
		uid_data->texMtxInfo[i].texgentype = xfmem.texMtxInfo[i].texgentype;
		switch (texinfo.texgentype)
		{
			case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

				if (components & (VB_HAS_NRM1|VB_HAS_NRM2))
				{
					// transform the light dir into tangent space
					uid_data->texMtxInfo[i].embosslightshift = xfmem.texMtxInfo[i].embosslightshift;
					uid_data->texMtxInfo[i].embosssourceshift = xfmem.texMtxInfo[i].embosssourceshift;
					out.Write("ldir = normalize(" LIGHT_POS".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(texinfo.embosslightshift));
					out.Write("o.tex%d.xyz = o.tex%d.xyz + float3(dot(ldir, _norm1), dot(ldir, _norm2), 0.0);\n", i, texinfo.embosssourceshift);
				}
				else
				{
					_assert_(0); // should have normals
					uid_data->texMtxInfo[i].embosssourceshift = xfmem.texMtxInfo[i].embosssourceshift;
					out.Write("o.tex%d.xyz = o.tex%d.xyz;\n", i, texinfo.embosssourceshift);
				}

				break;
			case XF_TEXGEN_COLOR_STRGBC0:
				_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
				out.Write("o.tex%d.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
				break;
			case XF_TEXGEN_COLOR_STRGBC1:
				_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
				out.Write("o.tex%d.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
				break;
			case XF_TEXGEN_REGULAR:
			default:
				uid_data->texMtxInfo_n_projection |= xfmem.texMtxInfo[i].projection << i;
				if (components & (VB_HAS_TEXMTXIDX0<<i))
				{
					out.Write("int tmp = int(tex%d.z);\n", i);
					if (texinfo.projection == XF_TEXPROJ_STQ)
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), dot(coord, " I_TRANSFORMMATRICES"[tmp+2]));\n", i);
					else
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), 1);\n", i);
				}
				else
				{
					if (texinfo.projection == XF_TEXPROJ_STQ)
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]));\n", i, 3*i, 3*i+1, 3*i+2);
					else
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), 1);\n", i, 3*i, 3*i+1);
				}
				break;
		}

		uid_data->dualTexTrans_enabled = xfmem.dualTexTrans.enabled;
		// CHECKME: does this only work for regular tex gen types?
		if (xfmem.dualTexTrans.enabled && texinfo.texgentype == XF_TEXGEN_REGULAR)
		{
			const PostMtxInfo& postInfo = xfmem.postMtxInfo[i];

			uid_data->postMtxInfo[i].index = xfmem.postMtxInfo[i].index;
			int postidx = postInfo.index;
			out.Write("float4 P0 = " I_POSTTRANSFORMMATRICES"[%d];\n"
				"float4 P1 = " I_POSTTRANSFORMMATRICES"[%d];\n"
				"float4 P2 = " I_POSTTRANSFORMMATRICES"[%d];\n",
				postidx&0x3f, (postidx+1)&0x3f, (postidx+2)&0x3f);

			if (texGenSpecialCase)
			{
				// no normalization
				// q of input is 1
				// q of output is unknown

				// multiply by postmatrix
				out.Write("o.tex%d.xyz = float3(dot(P0.xy, o.tex%d.xy) + P0.z + P0.w, dot(P1.xy, o.tex%d.xy) + P1.z + P1.w, 0.0);\n", i, i, i);
			}
			else
			{
				uid_data->postMtxInfo[i].normalize = xfmem.postMtxInfo[i].normalize;
				if (postInfo.normalize)
					out.Write("o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

				// multiply by postmatrix
				out.Write("o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
			}
		}

		out.Write("}\n");
	}

	// clipPos/w needs to be done in pixel shader, not here
	out.Write("o.clipPos = float4(pos.x,pos.y,o.pos.z,o.pos.w);\n");

	if (g_ActiveConfig.bEnablePixelLighting)
	{
		out.Write("o.Normal = float4(_norm0.x,_norm0.y,_norm0.z,pos.z);\n");

		if (components & VB_HAS_COL0)
			out.Write("o.colors_0 = color0;\n");

		if (components & VB_HAS_COL1)
			out.Write("o.colors_1 = color1;\n");
	}

	//write the true depth value, if the game uses depth textures pixel shaders will override with the correct values
	//if not early z culling will improve speed
	if (api_type == API_D3D)
	{
		out.Write("o.pos.z = " I_DEPTHPARAMS".x * o.pos.w + o.pos.z * " I_DEPTHPARAMS".y;\n");
	}
	else // OGL
	{
		// this results in a scale from -1..0 to -1..1 after perspective
		// divide
		out.Write("o.pos.z = o.pos.w + o.pos.z * 2.0;\n");

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
	out.Write("o.pos.xy = o.pos.xy - " I_DEPTHPARAMS".zw;\n");

	if (api_type == API_OPENGL)
	{
		if (g_ActiveConfig.bStereo)
			out.Write("v = o;\n");

		out.Write("gl_Position = o.pos;\n");
	}
	else // D3D
	{
		out.Write("return o;\n");
	}
	out.Write("}\n");

	if (is_writing_shadercode)
	{
		if (text[sizeof(text) - 1] != 0x7C)
			PanicAlert("VertexShader generator - buffer too small, canary has been eaten!");

#ifndef ANDROID
		uselocale(old_locale); // restore locale
		freelocale(locale);
#endif
	}
}

void GetVertexShaderUid(VertexShaderUid& object, u32 components, API_TYPE api_type)
{
	GenerateVertexShader<VertexShaderUid>(object, components, api_type);
}

void GenerateVertexShaderCode(VertexShaderCode& object, u32 components, API_TYPE api_type)
{
	GenerateVertexShader<VertexShaderCode>(object, components, api_type);
}

void GenerateVSOutputStructForGS(ShaderCode& object, API_TYPE api_type)
{
	GenerateVSOutputStruct<ShaderCode>(object, api_type);
}
