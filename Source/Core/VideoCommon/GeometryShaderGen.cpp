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

	uid_data->stereo = g_ActiveConfig.bStereo;
	if (ApiType == API_OPENGL)
	{
		// Insert layout parameters
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("layout(triangles, invocations = %d) in;\n", g_ActiveConfig.bStereo ? 2 : 1);
		else
			out.Write("layout(triangles) in;\n");
		out.Write("layout(triangle_strip, max_vertices = %d) out;\n", g_ActiveConfig.backend_info.bSupportsGSInstancing ? 3 : 6);
	}

	out.Write("%s", s_lighting_struct);

	// uniforms
	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
	else
		out.Write("cbuffer VSBlock {\n");
	out.Write(s_shader_uniforms);
	out.Write("};\n");

	GenerateVSOutputStruct(out, ApiType);

	out.Write("centroid in VS_OUTPUT v[];\n");
	out.Write("centroid out VS_OUTPUT o;\n");

	out.Write("flat out int layer;\n");

	out.Write("void main()\n{\n");

	if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
		out.Write("\tlayer = gl_InvocationID;\n");
	else
		out.Write("\tfor (layer = 0; layer < %d; ++layer) {\n", g_ActiveConfig.bStereo ? 2 : 1);

	out.Write("\tgl_Layer = layer;\n");
	out.Write("\tvec4 stereoproj = "I_PROJECTION"[0];\n");
	out.Write("\tstereoproj[2] += "I_STEREOOFFSET"[layer] * stereoproj[0];\n");

	out.Write("\tfor (int i = 0; i < gl_in.length(); ++i) {\n");
	out.Write("\t\to = v[i];\n");

	if (g_ActiveConfig.bStereo)
		out.Write("\t\to.pos = float4(dot(stereoproj, v[i].rawpos), dot(" I_PROJECTION"[1], v[i].rawpos), dot(" I_PROJECTION"[2], v[i].rawpos), dot(" I_PROJECTION"[3], v[i].rawpos)); \n");

	out.Write("\t\tgl_Position = o.pos;\n");
	out.Write("\t\tEmitVertex();\n");
	out.Write("\t}\n");
	out.Write("\tEndPrimitive();\n");

	if (!g_ActiveConfig.backend_info.bSupportsGSInstancing)
		out.Write("\t}\n");

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
