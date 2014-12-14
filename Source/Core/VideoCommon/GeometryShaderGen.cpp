// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

static char text[16384];

static const char* primitives_ogl[] =
{
	"points",
	"lines",
	"triangles"
};

static const char* primitives_d3d[] =
{
	"point",
	"line",
	"triangle"
};

template<class T>
static inline void GenerateGeometryShader(T& out, u32 primitive_type, API_TYPE ApiType)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	geometry_shader_uid_data dummy_data;
	geometry_shader_uid_data* uid_data = out.template GetUidData<geometry_shader_uid_data>();
	if (uid_data == nullptr)
		uid_data = &dummy_data;

	out.SetBuffer(text);
	const bool is_writing_shadercode = (out.GetBuffer() != nullptr);

	if (is_writing_shadercode)
		text[sizeof(text) - 1] = 0x7C;  // canary

	uid_data->primitive_type = primitive_type;
	const unsigned int vertex_in = primitive_type + 1;
	uid_data->stereo = g_ActiveConfig.iStereoMode > 0;
	const unsigned int vertex_out = primitive_type == PRIMITIVE_TRIANGLES ? 3 : 4;

	if (ApiType == API_OPENGL)
	{
		// Insert layout parameters
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("layout(%s, invocations = %d) in;\n", primitives_ogl[primitive_type], g_ActiveConfig.iStereoMode > 0 ? 2 : 1);
		else
			out.Write("layout(%s) in;\n", primitives_ogl[primitive_type]);
		out.Write("layout(triangle_strip, max_vertices = %d) out;\n", g_ActiveConfig.backend_info.bSupportsGSInstancing ? vertex_out : vertex_out * 2);
	}

	out.Write("%s", s_lighting_struct);

	// uniforms
	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform GSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 3" : "");
	else
		out.Write("cbuffer GSBlock {\n");
	out.Write(
		"\tfloat4 " I_STEREOPARAMS";\n"
		"\tfloat4 " I_LINEPTWIDTH";\n"
		"\tfloat4 " I_VIEWPORT";\n"
		"\tuint " I_TEXOFFSETFLAGS"[8];\n"
		"};\n");

	uid_data->numTexGens = bpmem.genMode.numtexgens;
	uid_data->pixel_lighting = g_ActiveConfig.bEnablePixelLighting;

	GenerateVSOutputStruct<T>(out, ApiType);

	if (ApiType == API_OPENGL)
	{
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("#define InstanceID gl_InvocationID\n");

		out.Write("centroid in VS_OUTPUT o[%d];\n", vertex_in);
		out.Write("centroid out VS_OUTPUT vs;\n");

		if (g_ActiveConfig.iStereoMode > 0)
			out.Write("flat out int layer;\n");

		out.Write("void main()\n{\n");
	}
	else // D3D
	{
		out.Write("struct GS_OUTPUT {\n");
		out.Write("\tVS_OUTPUT vs;\n");

		if (g_ActiveConfig.iStereoMode > 0)
			out.Write("\tuint layer : SV_RenderTargetArrayIndex;\n");

		out.Write("};\n");

		out.Write("[maxvertexcount(%d)]\n", vertex_out);
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
		{
			out.Write("[instance(%d)]\n", g_ActiveConfig.iStereoMode > 0 ? 2 : 1);
			out.Write("void main(%s VS_OUTPUT o[%d], inout TriangleStream<GS_OUTPUT> Output, in uint InstanceID : SV_GSInstanceID)\n{\n", primitives_d3d[primitive_type], vertex_in);
		}
		else
		{
			out.Write("void main(%s VS_OUTPUT o[%d], inout TriangleStream<GS_OUTPUT> Output)\n{\n", primitives_d3d[primitive_type], vertex_in);
		}

		out.Write("\tGS_OUTPUT gs;\n");
	}

	out.Write("\tVS_OUTPUT f;\n");

	if (g_ActiveConfig.iStereoMode > 0)
	{
		// If the GPU supports invocation we don't need a for loop and can simply use the
		// invocation identifier to determine which layer we're rendering.
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("\tint eye = InstanceID;\n");
		else
			out.Write("\tfor (int eye = 0; eye < %d; ++eye) {\n", g_ActiveConfig.iStereoMode > 0 ? 2 : 1);
	}

	out.Write("\tfor (int i = 0; i < %d; ++i) {\n", vertex_in);

	out.Write("\t\tf = o[i];\n");
	out.Write("\t\tfloat4 pos = o[i].pos;\n");

	if (g_ActiveConfig.iStereoMode > 0)
	{
		// Select the output layer
		if (ApiType == API_OPENGL)
		{
			out.Write("\t\tgl_Layer = eye;\n");
			out.Write("\t\tlayer = eye;\n");
		}
		else
			out.Write("\t\tgs.layer = eye;\n");

		// For stereoscopy add a small horizontal offset in Normalized Device Coordinates proportional
		// to the depth of the vertex. We retrieve the depth value from the w-component of the projected
		// vertex which contains the negated z-component of the original vertex.
		// For negative parallax (out-of-screen effects) we subtract a convergence value from
		// the depth value. This results in objects at a distance smaller than the convergence
		// distance to seemingly appear in front of the screen.
		// This formula is based on page 13 of the "Nvidia 3D Vision Automatic, Best Practices Guide"
		out.Write("\t\tf.clipPos.x = o[i].clipPos.x + " I_STEREOPARAMS"[eye] * (o[i].clipPos.w - " I_STEREOPARAMS"[2]);\n");
		out.Write("\t\tpos.x = o[i].pos.x + " I_STEREOPARAMS"[eye] * (o[i].pos.w - " I_STEREOPARAMS"[2]);\n");
		out.Write("\t\tf.pos.x = pos.x;\n");
	}

	if (ApiType == API_OPENGL)
		out.Write("\t\tgl_Position = pos;\n");

	out.Write("\t\t%s = f;\n", (ApiType == API_OPENGL) ? "vs" : "gs.vs");

	if (ApiType == API_OPENGL)
		out.Write("\t\tEmitVertex();\n");
	else
		out.Write("\t\tOutput.Append(gs);\n");

	out.Write("\t}\n");

	if (ApiType == API_OPENGL)
		out.Write("\tEndPrimitive();\n");
	else
		out.Write("\tOutput.RestartStrip();\n");

	if (!g_ActiveConfig.backend_info.bSupportsGSInstancing)
		out.Write("\t}\n");

	out.Write("}\n");

	if (is_writing_shadercode)
	{
		if (text[sizeof(text) - 1] != 0x7C)
			PanicAlert("GeometryShader generator - buffer too small, canary has been eaten!");
	}
}

void GetGeometryShaderUid(GeometryShaderUid& object, u32 primitive_type, API_TYPE ApiType)
{
	GenerateGeometryShader<GeometryShaderUid>(object, primitive_type, ApiType);
}

void GenerateGeometryShaderCode(ShaderCode& object, u32 primitive_type, API_TYPE ApiType)
{
	GenerateGeometryShader<ShaderCode>(object, primitive_type, ApiType);
}
