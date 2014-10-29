// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/LightingShaderGen.h"

static char text[16384];

template<class T>
static inline void GenerateGeometryShader(T& out, u32 components, API_TYPE ApiType)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	geometry_shader_uid_data dummy_data;
	geometry_shader_uid_data* uid_data = out.template GetUidData<geometry_shader_uid_data>();
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

	out.Write("//Geometry Shader for 3D stereoscopy\n");

	if (ApiType == API_OPENGL)
	{
		// Insert layout parameters
		uid_data->stereo = g_ActiveConfig.bStereo;
		out.Write("layout(triangles, invocations = %d) in;\n", g_ActiveConfig.bStereo ? 2 : 1);
		out.Write("layout(triangle_strip, max_vertices = 3) out;\n");
	}

	out.Write("%s", s_lighting_struct);

	// uniforms
	if (ApiType == API_OPENGL)
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

	GenerateVSOutputStruct(out, ApiType);

	out.Write("centroid in VS_OUTPUT v[];\n");
	out.Write("centroid out VS_OUTPUT o;\n");

	out.Write("flat out int eye;\n");

	out.Write("void main()\n{\n");
	out.Write("\tfor (int i = 0; i < gl_in.length(); ++i) {\n");
	out.Write("\t\to = v[i];\n");
	out.Write("\t\teye = gl_InvocationID;\n");
	if (g_ActiveConfig.bStereo)
		out.Write("\t\to.pos = float4(dot(" I_STEREOPROJECTION"[eye * 4 + 0], v[i].rawpos), dot(" I_STEREOPROJECTION"[eye * 4 + 1], v[i].rawpos), dot(" I_STEREOPROJECTION"[eye * 4 + 2], v[i].rawpos), dot(" I_STEREOPROJECTION"[eye * 4 + 3], v[i].rawpos)); \n");
	out.Write("\t\tgl_Position = o.pos;\n");
	out.Write("\t\tgl_Layer = eye;\n");
	out.Write("\t\tEmitVertex();\n");
	out.Write("\t}\n");
	out.Write("\tEndPrimitive();\n");
	out.Write("}\n");

	if (is_writing_shadercode)
	{
		if (text[sizeof(text) - 1] != 0x7C)
			PanicAlert("GeometryShader generator - buffer too small, canary has been eaten!");

#ifndef ANDROID
		uselocale(old_locale); // restore locale
		freelocale(locale);
#endif
	}
}

void GetGeometryShaderUid(GeometryShaderUid& object, u32 components, API_TYPE ApiType)
{
	GenerateGeometryShader<GeometryShaderUid>(object, components, ApiType);
}

void GenerateGeometryShaderCode(ShaderCode& object, u32 components, API_TYPE ApiType)
{
	GenerateGeometryShader<ShaderCode>(object, components, ApiType);
}
