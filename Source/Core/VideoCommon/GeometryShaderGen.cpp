// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "VideoCommon/BPMemory.h"
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
	const unsigned int vertex_out = primitive_type == PRIMITIVE_TRIANGLES ? 3 : 4;

	uid_data->stereo = g_ActiveConfig.iStereoMode > 0;
	if (ApiType == API_OPENGL)
	{
		// Insert layout parameters
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
		{
			out.Write("layout(%s, invocations = %d) in;\n", primitives_ogl[primitive_type], g_ActiveConfig.iStereoMode > 0 ? 2 : 1);
			out.Write("layout(triangle_strip, max_vertices = %d) out;\n", vertex_out);
		}
		else
		{
			out.Write("layout(%s) in;\n", primitives_ogl[primitive_type]);
			out.Write("layout(triangle_strip, max_vertices = %d) out;\n", g_ActiveConfig.iStereoMode > 0 ? vertex_out * 2 : vertex_out);
		}
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

		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
		{
			out.Write("[maxvertexcount(%d)]\n[instance(%d)]\n", vertex_out, g_ActiveConfig.iStereoMode > 0 ? 2 : 1);
			out.Write("void main(%s VS_OUTPUT o[%d], inout TriangleStream<GS_OUTPUT> output, in uint InstanceID : SV_GSInstanceID)\n{\n", primitives_d3d[primitive_type], vertex_in);
		}
		else
		{
			out.Write("[maxvertexcount(%d)]\n", g_ActiveConfig.iStereoMode > 0 ? vertex_out * 2 : vertex_out);
			out.Write("void main(%s VS_OUTPUT o[%d], inout TriangleStream<GS_OUTPUT> output)\n{\n", primitives_d3d[primitive_type], vertex_in);
		}

		out.Write("\tGS_OUTPUT gs;\n");
	}

	out.Write("\tVS_OUTPUT f;\n");

	if (primitive_type == PRIMITIVE_LINES)
	{
		// GameCube/Wii's line drawing algorithm is a little quirky. It does not
		// use the correct line caps. Instead, the line caps are vertical or
		// horizontal depending the slope of the line.

		out.Write("\tfloat2 offset;\n");
		out.Write("\tfloat2 to = abs(o[1].pos.xy - o[0].pos.xy);\n");
		// FIXME: What does real hardware do when line is at a 45-degree angle?
		// FIXME: Lines aren't drawn at the correct width. See Twilight Princess map.
		out.Write("\tif (" I_VIEWPORT".y * to.y > " I_VIEWPORT".x * to.x) {\n");
		// Line is more tall. Extend geometry left and right.
		// Lerp LineWidth/2 from [0..VpWidth] to [-1..1]
		out.Write("\t\toffset = float2(" I_LINEPTWIDTH"[0] / " I_VIEWPORT".x, 0);\n");
		out.Write("\t} else {\n");
		// Line is more wide. Extend geometry up and down.
		// Lerp LineWidth/2 from [0..VpHeight] to [1..-1]
		out.Write("\t\toffset = float2(0, -" I_LINEPTWIDTH"[0] / " I_VIEWPORT".y);\n");
		out.Write("\t}\n");
	}

	if (g_ActiveConfig.iStereoMode > 0)
	{
		// If the GPU supports invocation we don't need a for loop and can simply use the
		// invocation identifier to determine which layer we're rendering.
		if (g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("\tint eye = InstanceID;\n");
		else
			out.Write("\tfor (int eye = 0; eye < 2; ++eye) {\n");
	}

	out.Write("\tfor (int i = 0; i < %d; ++i) {\n", vertex_in);
	out.Write("\tVS_OUTPUT f = o[i];\n");

	if (g_ActiveConfig.iStereoMode > 0)
	{
		// Select the output layer
		if (ApiType == API_OPENGL)
		{
			out.Write("\tgl_Layer = eye;\n");
			out.Write("\tlayer = eye;\n");
		}
		else
			out.Write("\tgs.layer = eye;\n");

		// For stereoscopy add a small horizontal offset in Normalized Device Coordinates proportional
		// to the depth of the vertex. We retrieve the depth value from the w-component of the projected
		// vertex which contains the negated z-component of the original vertex.
		// For negative parallax (out-of-screen effects) we subtract a convergence value from
		// the depth value. This results in objects at a distance smaller than the convergence
		// distance to seemingly appear in front of the screen.
		// This formula is based on page 13 of the "Nvidia 3D Vision Automatic, Best Practices Guide"
		out.Write("\tf.clipPos.x += " I_STEREOPARAMS"[eye] * (o[i].clipPos.w - " I_STEREOPARAMS"[2]);\n");
		out.Write("\tf.pos.x += " I_STEREOPARAMS"[eye] * (o[i].pos.w - " I_STEREOPARAMS"[2]);\n");
	}

	if (primitive_type == PRIMITIVE_LINES)
	{
		out.Write("\tVS_OUTPUT l = f;\n");
		out.Write("\tVS_OUTPUT r = f;\n");

		out.Write("\tl.pos.xy -= offset * l.pos.w;\n");
		out.Write("\tr.pos.xy += offset * r.pos.w;\n");

		for (unsigned int i = 0; i < bpmem.genMode.numtexgens; ++i)
		{
			out.Write("\tr.tex%d.x += " I_LINEPTWIDTH"[2] * (" I_TEXOFFSETFLAGS"[%d] % 2);\n", i, i);
		}

		EmitVertex<T>(out, "l", ApiType);
		EmitVertex<T>(out, "r", ApiType);
	}
	else
	{
		EmitVertex<T>(out, "f", ApiType);
	}

	out.Write("\t}\n");

	if (ApiType == API_OPENGL)
		out.Write("\tEndPrimitive();\n");
	else
		out.Write("\toutput.RestartStrip();\n");

	if (g_ActiveConfig.iStereoMode > 0 && !g_ActiveConfig.backend_info.bSupportsGSInstancing)
			out.Write("\t}\n");

	out.Write("}\n");

	if (is_writing_shadercode)
	{
		if (text[sizeof(text) - 1] != 0x7C)
			PanicAlert("GeometryShader generator - buffer too small, canary has been eaten!");
	}
}

template<class T>
static inline void EmitVertex(T& out, const char *vertex, API_TYPE ApiType)
{
	if (ApiType == API_OPENGL)
		out.Write("\tgl_Position = %s.pos;\n", vertex);

	out.Write("\t%s = %s;\n", (ApiType == API_OPENGL) ? "vs" : "gs.vs", vertex);

	if (ApiType == API_OPENGL)
		out.Write("\tEmitVertex();\n");
	else
		out.Write("\toutput.Append(gs);\n");
}

void GetGeometryShaderUid(GeometryShaderUid& object, u32 primitive_type, API_TYPE ApiType)
{
	GenerateGeometryShader<GeometryShaderUid>(object, primitive_type, ApiType);
}

void GenerateGeometryShaderCode(ShaderCode& object, u32 primitive_type, API_TYPE ApiType)
{
	GenerateGeometryShader<ShaderCode>(object, primitive_type, ApiType);
}
