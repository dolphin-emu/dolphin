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

#include <math.h>
#include <locale.h>

#include "NativeVertexFormat.h"

#include "BPMemory.h"
#include "CPMemory.h"
#include "LightingShaderGen.h"
#include "VertexShaderGen.h"
#include "VideoConfig.h"

// Mash together all the inputs that contribute to the code of a generated vertex shader into
// a unique identifier, basically containing all the bits. Yup, it's a lot ....
void GetVertexShaderId(VERTEXSHADERUID *uid, u32 components)
{
	memset(uid->values, 0, sizeof(uid->values));
	uid->values[0] = components |
		(xfregs.numTexGen.numTexGens << 23) |
		(xfregs.numChan.numColorChans << 27) |
		(xfregs.dualTexTrans.enabled << 29);

	// TODO: If pixel lighting is enabled, do we even have to bother about storing lighting related registers here?
	GetLightingShaderId(&uid->values[1]);

	uid->values[2] |= (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting) << 31;
	u32 *pcurvalue = &uid->values[3];
	for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i) {
		TexMtxInfo tinfo = xfregs.texMtxInfo[i];
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP)
			tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR)
			tinfo.projection = 0;

		u32 val = ((tinfo.hex >> 1) & 0x1ffff);
		if (xfregs.dualTexTrans.enabled && tinfo.texgentype == XF_TEXGEN_REGULAR) {
			// rewrite normalization and post index
			val |= ((u32)xfregs.postMtxInfo[i].index << 17) | ((u32)xfregs.postMtxInfo[i].normalize << 23);
		}

		switch (i & 3) {
			case 0: pcurvalue[0] |= val; break;
			case 1: pcurvalue[0] |= val << 24; pcurvalue[1] = val >> 8; ++pcurvalue; break;
			case 2: pcurvalue[0] |= val << 16; pcurvalue[1] = val >> 16; ++pcurvalue; break;
			case 3: pcurvalue[0] |= val << 8; ++pcurvalue; break;
		}
	}
}

void GetSafeVertexShaderId(VERTEXSHADERUIDSAFE *uid, u32 components)
{
	// Just store all used registers here without caring whether we need all bits or less.
	memset(uid->values, 0, sizeof(uid->values));
	u32* ptr = uid->values;
	*ptr++ = components;
	*ptr++ = xfregs.numTexGen.hex;
	*ptr++ = xfregs.numChan.hex;
	*ptr++ = xfregs.dualTexTrans.hex;

	for (int i = 0; i < 2; ++i) {
		*ptr++ = xfregs.color[i].hex;
		*ptr++ = xfregs.alpha[i].hex;
	}
	*ptr++ = g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting;
	for (unsigned int i = 0; i < 8; ++i) {
		*ptr++ = xfregs.texMtxInfo[i].hex;
		*ptr++ = xfregs.postMtxInfo[i].hex;
	}
	_assert_((ptr - uid->values) == uid->GetNumValues());
}


void ValidateVertexShaderIDs(API_TYPE api, VERTEXSHADERUIDSAFE old_id, const std::string& old_code, u32 components)
{
	if (!g_ActiveConfig.bEnableShaderDebugging)
		return;

	VERTEXSHADERUIDSAFE new_id;
	GetSafeVertexShaderId(&new_id, components);

	if (!(old_id == new_id))
	{
		std::string new_code(GenerateVertexShaderCode(components, api));
		if (old_code != new_code)
		{
			_assert_(old_id.GetNumValues() == new_id.GetNumValues());

			char msg[8192];
			char* ptr = msg;
			ptr += sprintf(ptr, "Vertex shader IDs matched but unique IDs did not!\nUnique IDs (old <-> new):\n");
			const int N = new_id.GetNumValues();
			for (int i = 0; i < N/2; ++i)
				ptr += sprintf(ptr, "%02d, %08X  %08X  |  %08X  %08X\n", 2*i, old_id.values[2*i], old_id.values[2*i+1],
																			new_id.values[2*i], new_id.values[2*i+1]);
			if (N % 2)
				ptr += sprintf(ptr, "%02d, %08X  |  %08X\n", N-1, old_id.values[N-1], new_id.values[N-1]);
				
			static int num_failures = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%svsuid_mismatch_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file(szTemp);
			file << msg;
			file << "\n\nOld shader code:\n" << old_code;
			file << "\n\nNew shader code:\n" << new_code;
			file.close();

			PanicAlert("Unique pixel shader ID mismatch!\n\nReport this to the devs, along with the contents of %s.", szTemp);
		}
	}
}


static char text[16384];

#define WRITE p+=sprintf

char* GenerateVSOutputStruct(char* p, u32 components, API_TYPE ApiType)
{
	// This turned a bit ugly with GLSL
	// Will be less ugly with GLSL 1.3...hopefully
	WRITE(p, "struct VS_OUTPUT {\n");
	WRITE(p, "  float4 pos %s POSITION;\n", ApiType == API_GLSL ? ";//" : ":");
	WRITE(p, "  float4 colors_0 %s COLOR0;\n", ApiType == API_GLSL ? ";//" : ":");
	WRITE(p, "  float4 colors_1 %s COLOR1;\n", ApiType == API_GLSL ? ";//" : ":");

	if (xfregs.numTexGen.numTexGens < 7) {
		for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
			WRITE(p, "  float3 tex%d %s TEXCOORD%d;\n", i, ApiType == API_GLSL ? ";//" : ":", i);
		WRITE(p, "  float4 clipPos %s TEXCOORD%d;\n", ApiType == API_GLSL ? ";//" : ":", xfregs.numTexGen.numTexGens);
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			WRITE(p, "  float4 Normal %s TEXCOORD%d;\n", ApiType == API_GLSL ? ";//" : ":", xfregs.numTexGen.numTexGens + 1);
	} else {
		// clip position is in w of first 4 texcoords
		if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
		{
			for (int i = 0; i < 8; ++i)
				WRITE(p, "  float4 tex%d %s TEXCOORD%d;\n", i, ApiType == API_GLSL ? ";//" : ":", i);
		}
		else
		{
			for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				WRITE(p, "  float%d tex%d %s TEXCOORD%d;\n", i < 4 ? 4 : 3 , i, ApiType == API_GLSL ? ";//" : ":", i);
		}
	}
	WRITE(p, "};\n");

	return p;
}

extern const char* WriteRegister(API_TYPE ApiType, const char *prefix, const u32 num);
extern const char* WriteBinding(API_TYPE ApiType, const u32 num);
const char *WriteLocation(API_TYPE ApiType);

const char *GenerateVertexShaderCode(u32 components, API_TYPE ApiType)
{
	setlocale(LC_NUMERIC, "C"); // Reset locale for compilation
	text[sizeof(text) - 1] = 0x7C;  // canary

	_assert_(bpmem.genMode.numtexgens == xfregs.numTexGen.numTexGens);
	_assert_(bpmem.genMode.numcolchans == xfregs.numChan.numColorChans);
	
	bool is_d3d = (ApiType & API_D3D9 || ApiType == API_D3D11);
	u32 lightMask = 0;
	if (xfregs.numChan.numColorChans > 0)
		lightMask |= xfregs.color[0].GetFullLightMask() | xfregs.alpha[0].GetFullLightMask();
	if (xfregs.numChan.numColorChans > 1)
		lightMask |= xfregs.color[1].GetFullLightMask() | xfregs.alpha[1].GetFullLightMask();

	char *p = text;
	WRITE(p, "//Vertex Shader: comp:%x, \n", components);
	
	if (ApiType == API_GLSL)
	{
		// A few required defines and ones that will make our lives a lot easier
		if (g_ActiveConfig.backend_info.bSupportsGLSLBinding || g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			WRITE(p, "#version 330 compatibility\n");
			if (g_ActiveConfig.backend_info.bSupportsGLSLBinding)
				WRITE(p, "#extension GL_ARB_shading_language_420pack : enable\n");
			if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
				WRITE(p, "#extension GL_ARB_uniform_buffer_object : enable\n");
			WRITE(p, "#define ATTRIN in\n");
			WRITE(p, "#define ATTROUT out\n");
		}
		else
		{
			WRITE(p, "#version 120\n");
			WRITE(p, "#define ATTRIN attribute\n");
			WRITE(p, "#define ATTROUT attribute\n");
		}
		if (g_ActiveConfig.backend_info.bSupportsGLSLATTRBind)
			WRITE(p, "#extension GL_ARB_explicit_attrib_location : enable\n");
		// Silly differences
		WRITE(p, "#define float2 vec2\n");
		WRITE(p, "#define float3 vec3\n");
		WRITE(p, "#define float4 vec4\n");

		// cg to glsl function translation
		WRITE(p, "#define frac(x) fract(x)\n");
		WRITE(p, "#define saturate(x) clamp(x, 0.0f, 1.0f)\n");
		WRITE(p, "#define lerp(x, y, z) mix(x, y, z)\n");
	}
	
	// uniforms
	if (ApiType == API_GLSL && g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		WRITE(p, "layout(std140) uniform VSBlock {\n");
		
	WRITE(p, "%sfloat4 "I_POSNORMALMATRIX"[6] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_POSNORMALMATRIX));
	WRITE(p, "%sfloat4 "I_PROJECTION"[4] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_PROJECTION));	
	WRITE(p, "%sfloat4 "I_MATERIALS"[4] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_MATERIALS));
	WRITE(p, "%sfloat4 "I_LIGHTS"[40] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_LIGHTS));
	WRITE(p, "%sfloat4 "I_TEXMATRICES"[24] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_TEXMATRICES)); // also using tex matrices
	WRITE(p, "%sfloat4 "I_TRANSFORMMATRICES"[64] %s;\n", WriteLocation(ApiType),WriteRegister(ApiType, "c", C_TRANSFORMMATRICES));
	WRITE(p, "%sfloat4 "I_NORMALMATRICES"[32] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_NORMALMATRICES));
	WRITE(p, "%sfloat4 "I_POSTTRANSFORMMATRICES"[64] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_POSTTRANSFORMMATRICES));
	WRITE(p, "%sfloat4 "I_DEPTHPARAMS" %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_DEPTHPARAMS));
		
	if (ApiType == API_GLSL && g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		WRITE(p, "};\n");
	

	p = GenerateVSOutputStruct(p, components, ApiType);

	if(ApiType == API_GLSL)
	{
		if (components & VB_HAS_NRM0)
			WRITE(p, " float3 rawnorm0 = gl_Normal; // NORMAL0,\n");

		if (g_ActiveConfig.backend_info.bSupportsGLSLATTRBind)
		{
			if (components & VB_HAS_POSMTXIDX)
				WRITE(p, "layout(location = %d) ATTRIN float fposmtx;\n", SHADER_POSMTX_ATTRIB);
			if (components & VB_HAS_NRM1)
				WRITE(p, "layout(location = %d) ATTRIN float3 rawnorm1;\n", SHADER_NORM1_ATTRIB);
			if (components & VB_HAS_NRM2)
				WRITE(p, "layout(location = %d) ATTRIN float3 rawnorm2;\n", SHADER_NORM2_ATTRIB);
		}
		else
		{
			if (components & VB_HAS_POSMTXIDX)
				WRITE(p, "ATTRIN float fposmtx; // ATTR%d,\n", SHADER_POSMTX_ATTRIB);
			if (components & VB_HAS_NRM1)
				WRITE(p, "ATTRIN float3 rawnorm1; // ATTR%d,\n", SHADER_NORM1_ATTRIB);
			if (components & VB_HAS_NRM2)
				WRITE(p, "ATTRIN float3 rawnorm2; // ATTR%d,\n", SHADER_NORM2_ATTRIB);
		}

		if (components & VB_HAS_COL0)
			WRITE(p, "  float4 color0 = gl_Color; // COLOR0,\n");
		if (components & VB_HAS_COL1)
			WRITE(p, "  float4 color1 = gl_SecondaryColor; // COLOR1,\n");
		for (int i = 0; i < 8; ++i) {
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0<<i));
			if ((components & (VB_HAS_UV0<<i)) || hastexmtx)
				WRITE(p, "  float%d tex%d = gl_MultiTexCoord%d.xy%s; // TEXCOORD%d,\n", hastexmtx ? 3 : 2, i, i, hastexmtx ? "z" : "", i);
		}

		// Let's set up attributes
		if (xfregs.numTexGen.numTexGens < 7)
		{
			for (int i = 0; i < 8; ++i)
				WRITE(p, "ATTROUT  float3 uv%d_2;\n", i);
			WRITE(p, "ATTROUT   float4 clipPos_2;\n");
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
				WRITE(p, "ATTROUT   float4 Normal_2;\n");
		}
		else
		{
			// wpos is in w of first 4 texcoords
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
					WRITE(p, "ATTROUT   float4 uv%d_2;\n", i);
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
					WRITE(p, "ATTROUT   float%d uv%d_2;\n", i < 4 ? 4 : 3 , i);
			}
		}
		WRITE(p, "  float4 rawpos = gl_Vertex;\n") ;

		WRITE(p, "void main()\n{\n");
	}
	else
	{
		WRITE(p, "VS_OUTPUT main(\n");	
		
		// inputs
		if (components & VB_HAS_NRM0)
			WRITE(p, "  float3 rawnorm0 : NORMAL0,\n");
		if (components & VB_HAS_NRM1) {
			if (is_d3d)
				WRITE(p, "  float3 rawnorm1 : NORMAL1,\n");
			else
				WRITE(p, "  float3 rawnorm1 : ATTR%d,\n", SHADER_NORM1_ATTRIB);
		}
		if (components & VB_HAS_NRM2) {
			if (is_d3d)
				WRITE(p, "  float3 rawnorm2 : NORMAL2,\n");
			else
				WRITE(p, "  float3 rawnorm2 : ATTR%d,\n", SHADER_NORM2_ATTRIB);
		}
		if (components & VB_HAS_COL0)
			WRITE(p, "  float4 color0 : COLOR0,\n");
		if (components & VB_HAS_COL1)
			WRITE(p, "  float4 color1 : COLOR1,\n");
		for (int i = 0; i < 8; ++i) {
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0<<i));
			if ((components & (VB_HAS_UV0<<i)) || hastexmtx)
				WRITE(p, "  float%d tex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i, i);
		}
		if (components & VB_HAS_POSMTXIDX) {
			if (is_d3d)
				WRITE(p, "  float4 blend_indices : BLENDINDICES,\n");
			else
				WRITE(p, "  float fposmtx : ATTR%d,\n", SHADER_POSMTX_ATTRIB);
		}
		WRITE(p, "  float4 rawpos : POSITION) {\n");
	}
	WRITE(p, "VS_OUTPUT o;\n");	

	// transforms
	if (components & VB_HAS_POSMTXIDX)
	{
		if (ApiType & API_D3D9)
		{
			WRITE(p, "int4 indices = D3DCOLORtoUBYTE4(blend_indices);\n");
			WRITE(p, "int posmtx = indices.x;\n");
		}
		else if (ApiType == API_D3D11)
		{
			WRITE(p, "int posmtx = blend_indices.x * 255.0f;\n");
		}
		else
		{
			WRITE(p, "int posmtx = int(fposmtx);\n");
		}

		WRITE(p, "float4 pos = float4(dot("I_TRANSFORMMATRICES"[posmtx], rawpos), dot("I_TRANSFORMMATRICES"[posmtx+1], rawpos), dot("I_TRANSFORMMATRICES"[posmtx+2], rawpos), 1);\n");		

		if (components & VB_HAS_NRMALL) {
			WRITE(p, "int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
			WRITE(p, "float3 N0 = "I_NORMALMATRICES"[normidx].xyz, N1 = "I_NORMALMATRICES"[normidx+1].xyz, N2 = "I_NORMALMATRICES"[normidx+2].xyz;\n");
		}

		if (components & VB_HAS_NRM0)
			WRITE(p, "float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			WRITE(p, "float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			WRITE(p, "float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
	}
	else
	{
		WRITE(p, "float4 pos = float4(dot("I_POSNORMALMATRIX"[0], rawpos), dot("I_POSNORMALMATRIX"[1], rawpos), dot("I_POSNORMALMATRIX"[2], rawpos), 1.0f);\n");
		if (components & VB_HAS_NRM0)
			WRITE(p, "float3 _norm0 = normalize(float3(dot("I_POSNORMALMATRIX"[3].xyz, rawnorm0), dot("I_POSNORMALMATRIX"[4].xyz, rawnorm0), dot("I_POSNORMALMATRIX"[5].xyz, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			WRITE(p, "float3 _norm1 = float3(dot("I_POSNORMALMATRIX"[3].xyz, rawnorm1), dot("I_POSNORMALMATRIX"[4].xyz, rawnorm1), dot("I_POSNORMALMATRIX"[5].xyz, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			WRITE(p, "float3 _norm2 = float3(dot("I_POSNORMALMATRIX"[3].xyz, rawnorm2), dot("I_POSNORMALMATRIX"[4].xyz, rawnorm2), dot("I_POSNORMALMATRIX"[5].xyz, rawnorm2));\n");
	}

	if (!(components & VB_HAS_NRM0))
		WRITE(p, "float3 _norm0 = float3(0.0f, 0.0f, 0.0f);\n");

	

	WRITE(p, "o.pos = float4(dot("I_PROJECTION"[0], pos), dot("I_PROJECTION"[1], pos), dot("I_PROJECTION"[2], pos), dot("I_PROJECTION"[3], pos));\n");

	WRITE(p, "float4 mat, lacc;\n"
	"float3 ldir, h;\n"
	"float dist, dist2, attn;\n");

	if (xfregs.numChan.numColorChans == 0)
	{
		if (components & VB_HAS_COL0)
			WRITE(p, "o.colors_0 = color0;\n");
		else
			WRITE(p, "o.colors_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");		
	}

	// TODO: This probably isn't necessary if pixel lighting is enabled.
	p = GenerateLightingShader(p, components, I_MATERIALS, I_LIGHTS, "color", "o.colors_");

	if (xfregs.numChan.numColorChans < 2)
	{
		if (components & VB_HAS_COL1)
			WRITE(p, "o.colors_1 = color1;\n");
		else
			WRITE(p, "o.colors_1 = o.colors_0;\n");		
	}
	// special case if only pos and tex coord 0 and tex coord input is AB11
	// donko - this has caused problems in some games. removed for now.
	bool texGenSpecialCase = false;
	/*bool texGenSpecialCase =
		((g_VtxDesc.Hex & 0x60600L) == g_VtxDesc.Hex) && // only pos and tex coord 0
		(g_VtxDesc.Tex0Coord != NOT_PRESENT) &&
		(xfregs.texcoords[0].texmtxinfo.inputform == XF_TEXINPUT_AB11);
		*/

	// transform texcoords
	WRITE(p, "float4 coord = float4(0.0f, 0.0f, 1.0f, 1.0f);\n");
	for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i) {
		TexMtxInfo& texinfo = xfregs.texMtxInfo[i];

		WRITE(p, "{\n");
		WRITE(p, "coord = float4(0.0f, 0.0f, 1.0f, 1.0f);\n");
		switch (texinfo.sourcerow) {
		case XF_SRCGEOM_INROW:
			_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
			WRITE(p, "coord = rawpos;\n"); // pos.w is 1
			break;
		case XF_SRCNORMAL_INROW:
			if (components & VB_HAS_NRM0) {
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				WRITE(p, "coord = float4(rawnorm0.xyz, 1.0f);\n");
			}
			break;
		case XF_SRCCOLORS_INROW:
			_assert_( texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1 );
			break;
		case XF_SRCBINORMAL_T_INROW:
			if (components & VB_HAS_NRM1) {
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				WRITE(p, "coord = float4(rawnorm1.xyz, 1.0f);\n");
			}
			break;
		case XF_SRCBINORMAL_B_INROW:
			if (components & VB_HAS_NRM2) {
				_assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
				WRITE(p, "coord = float4(rawnorm2.xyz, 1.0f);\n");
			}
			break;
		default:
			_assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
			if (components & (VB_HAS_UV0<<(texinfo.sourcerow - XF_SRCTEX0_INROW)) )
				WRITE(p, "coord = float4(tex%d.x, tex%d.y, 1.0f, 1.0f);\n", texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);
			break;
		}

		// first transformation
		switch (texinfo.texgentype) {
			case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

				if (components & (VB_HAS_NRM1|VB_HAS_NRM2)) {
					// transform the light dir into tangent space
					WRITE(p, "ldir = normalize("I_LIGHTS"[%d + 3].xyz - pos.xyz);\n", texinfo.embosslightshift);
					WRITE(p, "o.tex%d.xyz = o.tex%d.xyz + float3(dot(ldir, _norm1), dot(ldir, _norm2), 0.0f);\n", i, texinfo.embosssourceshift);
				}
				else
				{
					_assert_(0); // should have normals
					WRITE(p, "o.tex%d.xyz = o.tex%d.xyz;\n", i, texinfo.embosssourceshift);
				}

				break;
			case XF_TEXGEN_COLOR_STRGBC0:
				_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
				WRITE(p, "o.tex%d.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
				break;
			case XF_TEXGEN_COLOR_STRGBC1:
				_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
				WRITE(p, "o.tex%d.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
				break;
			case XF_TEXGEN_REGULAR:
			default:
				if (components & (VB_HAS_TEXMTXIDX0<<i))
				{
					WRITE(p, "int tmp = int(tex%d.z);\n", i);
					if (texinfo.projection == XF_TEXPROJ_STQ)
						WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES"[tmp]), dot(coord, "I_TRANSFORMMATRICES"[tmp+1]), dot(coord, "I_TRANSFORMMATRICES"[tmp+2]));\n", i);
					else {
						WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES"[tmp]), dot(coord, "I_TRANSFORMMATRICES"[tmp+1]), 1);\n", i);
					}
				}
				else {
					if (texinfo.projection == XF_TEXPROJ_STQ)
						WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES"[%d]), dot(coord, "I_TEXMATRICES"[%d]), dot(coord, "I_TEXMATRICES"[%d]));\n", i, 3*i, 3*i+1, 3*i+2);
					else
						WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES"[%d]), dot(coord, "I_TEXMATRICES"[%d]), 1);\n", i, 3*i, 3*i+1);
				}
				break;
		}

		if (xfregs.dualTexTrans.enabled && texinfo.texgentype == XF_TEXGEN_REGULAR) { // only works for regular tex gen types?
			const PostMtxInfo& postInfo = xfregs.postMtxInfo[i];

			int postidx = postInfo.index;
			WRITE(p, "float4 P0 = "I_POSTTRANSFORMMATRICES"[%d];\n"
				"float4 P1 = "I_POSTTRANSFORMMATRICES"[%d];\n"
				"float4 P2 = "I_POSTTRANSFORMMATRICES"[%d];\n",
				postidx&0x3f, (postidx+1)&0x3f, (postidx+2)&0x3f);

			if (texGenSpecialCase) {
				// no normalization
				// q of input is 1
				// q of output is unknown

				// multiply by postmatrix
				WRITE(p, "o.tex%d.xyz = float3(dot(P0.xy, o.tex%d.xy) + P0.z + P0.w, dot(P1.xy, o.tex%d.xy) + P1.z + P1.w, 0.0f);\n", i, i, i);
			}
			else
			{
				if (postInfo.normalize)
					WRITE(p, "o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

				// multiply by postmatrix
				WRITE(p, "o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
			}
		}

		WRITE(p, "}\n");
	}

	// clipPos/w needs to be done in pixel shader, not here
	if (xfregs.numTexGen.numTexGens < 7) {
		WRITE(p, "o.clipPos = float4(pos.x,pos.y,o.pos.z,o.pos.w);\n");
	} else {
		WRITE(p, "o.tex0.w = pos.x;\n");
		WRITE(p, "o.tex1.w = pos.y;\n");
		WRITE(p, "o.tex2.w = o.pos.z;\n");
		WRITE(p, "o.tex3.w = o.pos.w;\n");
	}

	if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		if (xfregs.numTexGen.numTexGens < 7) {
			WRITE(p, "o.Normal = float4(_norm0.x,_norm0.y,_norm0.z,pos.z);\n");
		} else {
			WRITE(p, "o.tex4.w = _norm0.x;\n");
			WRITE(p, "o.tex5.w = _norm0.y;\n");
			WRITE(p, "o.tex6.w = _norm0.z;\n");
			if (xfregs.numTexGen.numTexGens < 8)
				WRITE(p, "o.tex7 = pos.xyzz;\n");
			else
				WRITE(p, "o.tex7.w = pos.z;\n");
		}		
		if (components & VB_HAS_COL0)
			WRITE(p, "o.colors_0 = color0;\n");		

		if (components & VB_HAS_COL1)
			WRITE(p, "o.colors_1 = color1;\n");
	}

	//write the true depth value, if the game uses depth textures pixel shaders will override with the correct values
	//if not early z culling will improve speed
	if (is_d3d)
	{
		WRITE(p, "o.pos.z = " I_DEPTHPARAMS".x * o.pos.w + o.pos.z * " I_DEPTHPARAMS".y;\n");
	}
	else
	{
	    // this results in a scale from -1..0 to -1..1 after perspective
	    // divide
	    WRITE(p, "o.pos.z = o.pos.w + o.pos.z * 2.0f;\n");

	    // Sonic Unleashed puts its final rendering at the near or
	    // far plane of the viewing frustrum(actually box, they use
	    // orthogonal projection for that), and we end up putting it
	    // just beyond, and the rendering gets clipped away. (The
	    // primitive gets dropped)
	    WRITE(p, "o.pos.z = o.pos.z * 1048575.0f/1048576.0f;\n");

	    // the next steps of the OGL pipeline are:
	    // (x_c,y_c,z_c,w_c) = o.pos  //switch to OGL spec terminology
	    // clipping to -w_c <= (x_c,y_c,z_c) <= w_c
	    // (x_d,y_d,z_d) = (x_c,y_c,z_c)/w_c//perspective divide
	    // z_w = (f-n)/2*z_d + (n+f)/2
	    // z_w now contains the value to go to the 0..1 depth buffer
	    
	    //trying to get the correct semantic while not using glDepthRange
	    //seems to get rather complicated
	}

	if (api_type & API_D3D9)
	{
		// D3D9 is addressing pixel centers instead of pixel boundaries in clip space.
		// Thus we need to offset the final position by half a pixel
		WRITE(p, "o.pos = o.pos + float4(" I_DEPTHPARAMS".z, " I_DEPTHPARAMS".w, 0.f, 0.f);\n");
	}

	if(ApiType == API_GLSL)
	{
		// Bit ugly here
		// Will look better when we bind uniforms in GLSL 1.3
		// clipPos/w needs to be done in pixel shader, not here

		if (xfregs.numTexGen.numTexGens < 7) {
			for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				WRITE(p, " uv%d_2.xyz =  o.tex%d;\n", i, i);
			WRITE(p, "  clipPos_2 = o.clipPos;\n");
			if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
				WRITE(p, "  Normal_2 = o.Normal;\n");
		} else {
			// clip position is in w of first 4 texcoords
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
					WRITE(p, " uv%d_2 = o.tex%d;\n", i, i);
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
					WRITE(p, "  uv%d_2%s = o.tex%d;\n", i, i < 4 ? ".xyzw" : ".xyz" , i);
			}
		}               
		WRITE(p, "gl_FrontColor = o.colors_0;\n");
		WRITE(p, "gl_FrontSecondaryColor = o.colors_1;\n");
		WRITE(p, "gl_Position = o.pos;\n");
		WRITE(p, "}\n");
	}
	else
		WRITE(p, "return o;\n}\n");

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("VertexShader generator - buffer too small, canary has been eaten!");
	setlocale(LC_NUMERIC, ""); // restore locale
	return text;
}
