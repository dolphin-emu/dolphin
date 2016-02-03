// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/StringUtil.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

// Pixel Shader blobs
static D3DBlob* s_color_matrix_program_blob[2] = {};
static D3DBlob* s_color_copy_program_blob[2] = {};
static D3DBlob* s_depth_matrix_program_blob[2] = {};
static D3DBlob* s_depth_copy_program_blob[2] = {};
static D3DBlob* s_clear_program_blob = {};
static D3DBlob* s_anaglyph_program_blob = {};
static D3DBlob* s_rgba6_to_rgb8_program_blob[2] = {};
static D3DBlob* s_rgb8_to_rgba6_program_blob[2] = {};

// Vertex Shader blobs/input layouts
static D3DBlob* s_simple_vertex_shader_blob = {};
static D3DBlob* s_simple_clear_vertex_shader_blob = {};

static const D3D12_INPUT_ELEMENT_DESC s_simple_vertex_shader_input_elements[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static const D3D12_INPUT_LAYOUT_DESC s_simple_vertex_shader_input_layout = {
	s_simple_vertex_shader_input_elements,
	ARRAYSIZE(s_simple_vertex_shader_input_elements)
};

static const D3D12_INPUT_ELEMENT_DESC s_clear_vertex_shader_input_elements[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static const D3D12_INPUT_LAYOUT_DESC s_clear_vertex_shader_input_layout =
{
	s_clear_vertex_shader_input_elements,
	ARRAYSIZE(s_clear_vertex_shader_input_elements)
};

// Geometry Shader blobs
static D3DBlob* s_clear_geometry_shader_blob = nullptr;
static D3DBlob* s_copy_geometry_shader_blob = nullptr;

// Pixel Shader HLSL
static constexpr const char s_clear_program_hlsl[] = {
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float4 incol0 : COLOR0){\n"
	"ocol0 = incol0;\n"
	"}\n"
};

// EXISTINGD3D11TODO: Find some way to avoid having separate shaders for non-MSAA and MSAA...
static constexpr const char s_color_copy_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"ocol0 = Tex0.Sample(samp0,uv0);\n"
	"}\n"
};

static constexpr const char s_depth_copy_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float odepth : SV_Depth,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"odepth = Tex0.Sample(samp0,uv0);\n"
	"}\n"
};

// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009
static constexpr const char s_anaglyph_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"float4 c0 = Tex0.Sample(samp0, float3(uv0.xy, 0.0));\n"
	"float4 c1 = Tex0.Sample(samp0, float3(uv0.xy, 1.0));\n"
	"float3x3 l = float3x3( 0.437, 0.449, 0.164,\n"
	"                      -0.062,-0.062,-0.024,\n"
	"                      -0.048,-0.050,-0.017);\n"
	"float3x3 r = float3x3(-0.011,-0.032,-0.007,\n"
	"                       0.377, 0.761, 0.009,\n"
	"                      -0.026,-0.093, 1.234);\n"
	"ocol0 = float4(mul(l, c0.rgb) + mul(r, c1.rgb), c0.a);\n"
	"}\n"
};

// TODO: Improve sampling algorithm!
static constexpr const char s_color_copy_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"int width, height, slices, samples;\n"
	"Tex0.GetDimensions(width, height, slices, samples);\n"
	"ocol0 = 0;\n"
	"for(int i = 0; i < SAMPLES; ++i)\n"
	"	ocol0 += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"ocol0 /= SAMPLES;\n"
	"}\n"
};

static constexpr const char s_depth_copy_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"    out float depth : SV_Depth,\n"
	"    in float4 pos : SV_Position,\n"
	"    in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	depth = Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), 0).x;\n"
	"	for(int i = 1; i < SAMPLES; ++i)\n"
	"		depth = min(depth, Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i).x);\n"
	"}\n"
};

static constexpr const char s_color_matrix_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

static constexpr const char s_color_matrix_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"int width, height, slices, samples;\n"
	"Tex0.GetDimensions(width, height, slices, samples);\n"
	"float4 texcol = 0;\n"
	"for(int i = 0; i < SAMPLES; ++i)\n"
	"	texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"texcol /= SAMPLES;\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

static constexpr const char s_depth_matrix_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float3 uv0 : TEXCOORD0){\n"
	"	float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

	// Convert to Z24 format
	"	int4 workspace;\n"
	"	workspace.r = (depth >> 16) & 255;\n"
	"	workspace.g = (depth >> 8) & 255;\n"
	"	workspace.b = depth & 255;\n"

	// Convert to Z4 format
	"	workspace.a = (depth >> 16) & 0xF0;\n"

	// Normalize components to [0.0..1.0]
	"	texcol = float4(workspace) / 255.0;\n"

	// Apply color matrix
	"	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

static constexpr const char s_depth_matrix_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float3 uv0 : TEXCOORD0){\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for(int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

	// Convert to Z24 format
	"	int4 workspace;\n"
	"	workspace.r = (depth >> 16) & 255;\n"
	"	workspace.g = (depth >> 8) & 255;\n"
	"	workspace.b = depth & 255;\n"

	// Convert to Z4 format
	"	workspace.a = (depth >> 16) & 0xF0;\n"

	// Normalize components to [0.0..1.0]
	"	texcol = float4(workspace) / 255.0;\n"

	// Apply color matrix
	"	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

static constexpr const char s_reint_rgba6_to_rgb8_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int4 src6 = round(Tex0.Sample(samp0,uv0) * 63.f);\n"
	"	int4 dst8;\n"
	"	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
	"	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
	"	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
	"	dst8.a = 255;\n"
	"	ocol0 = (float4)dst8 / 255.f;\n"
	"}"
};

static constexpr const char s_reint_rgba6_to_rgb8_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for (int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int4 src6 = round(texcol * 63.f);\n"
	"	int4 dst8;\n"
	"	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
	"	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
	"	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
	"	dst8.a = 255;\n"
	"	ocol0 = (float4)dst8 / 255.f;\n"
	"}"
};

static constexpr const char s_reint_rgb8_to_rgba6_program_hlsl[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int4 src8 = round(Tex0.Sample(samp0,uv0) * 255.f);\n"
	"	int4 dst6;\n"
	"	dst6.r = src8.r >> 2;\n"
	"	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
	"	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
	"	dst6.a = src8.b & 0x3F;\n"
	"	ocol0 = (float4)dst6 / 63.f;\n"
	"}\n"
};

static constexpr const char s_reint_rgb8_to_rgba6_program_msaa_hlsl[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for (int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int4 src8 = round(texcol * 255.f);\n"
	"	int4 dst6;\n"
	"	dst6.r = src8.r >> 2;\n"
	"	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
	"	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
	"	dst6.a = src8.b & 0x3F;\n"
	"	ocol0 = (float4)dst6 / 63.f;\n"
	"}\n"
};

// Vertex Shader HLSL
static constexpr const char s_simple_vertex_shader_hlsl[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition : POSITION;\n"
	"float3 vTexCoord : TEXCOORD0;\n"
	"float  vTexCoord1 : TEXCOORD1;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : POSITION,float4 inTEX0 : TEXCOORD0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vTexCoord = inTEX0.xyz;\n"
	"OUT.vTexCoord1 = inTEX0.w;\n"
	"return OUT;\n"
	"}\n"
};

static constexpr const char s_clear_vertex_shader_hlsl[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition   : POSITION;\n"
	"float4 vColor0   : COLOR0;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vColor0 = inColor0;\n"
	"return OUT;\n"
	"}\n"
};

// Geometry Shader HLSL
static constexpr const char s_clear_geometry_shader_hlsl[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"	float4 vPosition   : POSITION;\n"
	"	float4 vColor0   : COLOR0;\n"
	"};\n"
	"struct GSOUTPUT\n"
	"{\n"
	"	float4 vPosition   : POSITION;\n"
	"	float4 vColor0   : COLOR0;\n"
	"	uint slice    : SV_RenderTargetArrayIndex;\n"
	"};\n"
	"[maxvertexcount(6)]\n"
	"void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
	"{\n"
	"for(int slice = 0; slice < 2; slice++)\n"
	"{\n"
	"	for(int i = 0; i < 3; i++)\n"
	"	{\n"
	"		GSOUTPUT OUT;\n"
	"		OUT.vPosition = o[i].vPosition;\n"
	"		OUT.vColor0 = o[i].vColor0;\n"
	"		OUT.slice = slice;\n"
	"		Output.Append(OUT);\n"
	"	}\n"
	"	Output.RestartStrip();\n"
	"}\n"
	"}\n"
};

static constexpr const char s_copy_geometry_shader_hlsl[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"	float4 vPosition : POSITION;\n"
	"	float3 vTexCoord : TEXCOORD0;\n"
	"	float  vTexCoord1 : TEXCOORD1;\n"
	"};\n"
	"struct GSOUTPUT\n"
	"{\n"
	"	float4 vPosition : POSITION;\n"
	"	float3 vTexCoord : TEXCOORD0;\n"
	"	float  vTexCoord1 : TEXCOORD1;\n"
	"	uint slice    : SV_RenderTargetArrayIndex;\n"
	"};\n"
	"[maxvertexcount(6)]\n"
	"void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
	"{\n"
	"for(int slice = 0; slice < 2; slice++)\n"
	"{\n"
	"	for(int i = 0; i < 3; i++)\n"
	"	{\n"
	"		GSOUTPUT OUT;\n"
	"		OUT.vPosition = o[i].vPosition;\n"
	"		OUT.vTexCoord = o[i].vTexCoord;\n"
	"		OUT.vTexCoord.z = slice;\n"
	"		OUT.vTexCoord1 = o[i].vTexCoord1;\n"
	"		OUT.slice = slice;\n"
	"		Output.Append(OUT);\n"
	"	}\n"
	"	Output.RestartStrip();\n"
	"}\n"
	"}\n"
};

D3D12_SHADER_BYTECODE StaticShaderCache::GetReinterpRGBA6ToRGB8PixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		if (!s_rgba6_to_rgb8_program_blob[0])
		{
			D3D::CompilePixelShader(s_reint_rgba6_to_rgb8_program_hlsl, &s_rgba6_to_rgb8_program_blob[0]);
		}

		bytecode = { s_rgba6_to_rgb8_program_blob[0]->Data(), s_rgba6_to_rgb8_program_blob[0]->Size() };
		return bytecode;
	}
	else if (!s_rgba6_to_rgb8_program_blob[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_reint_rgba6_to_rgb8_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_rgba6_to_rgb8_program_blob[1]);
		bytecode = { s_rgba6_to_rgb8_program_blob[1]->Data(), s_rgba6_to_rgb8_program_blob[1]->Size() };
	}
	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetReinterpRGB8ToRGBA6PixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		if (!s_rgb8_to_rgba6_program_blob[0])
		{
			D3D::CompilePixelShader(s_reint_rgb8_to_rgba6_program_hlsl, &s_rgb8_to_rgba6_program_blob[0]);
		}

		bytecode = { s_rgb8_to_rgba6_program_blob[0]->Data(), s_rgb8_to_rgba6_program_blob[0]->Size() };
		return bytecode;
	}
	else if (!s_rgb8_to_rgba6_program_blob[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_reint_rgb8_to_rgba6_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_rgb8_to_rgba6_program_blob[1]);
		bytecode = { s_rgb8_to_rgba6_program_blob[1]->Data(), s_rgb8_to_rgba6_program_blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetColorCopyPixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_color_copy_program_blob[0]->Data(), s_color_copy_program_blob[0]->Size() };
	}
	else if (s_color_copy_program_blob[1])
	{
		bytecode = { s_color_copy_program_blob[1]->Data(), s_color_copy_program_blob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_color_copy_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_color_copy_program_blob[1]);
		bytecode = { s_color_copy_program_blob[1]->Data(), s_color_copy_program_blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetDepthCopyPixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_depth_copy_program_blob[0]->Data(), s_depth_copy_program_blob[0]->Size() };
	}
	else if (s_depth_copy_program_blob[1])
	{
		bytecode = { s_depth_copy_program_blob[1]->Data(), s_depth_copy_program_blob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_depth_copy_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_depth_copy_program_blob[1]);
		bytecode = { s_depth_copy_program_blob[1]->Data(), s_depth_copy_program_blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetColorMatrixPixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_color_matrix_program_blob[0]->Data(), s_color_matrix_program_blob[0]->Size() };
	}
	else if (s_color_matrix_program_blob[1])
	{
		bytecode = { s_color_matrix_program_blob[1]->Data(), s_color_matrix_program_blob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_color_matrix_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_color_matrix_program_blob[1]);
		bytecode = { s_color_matrix_program_blob[1]->Data(), s_color_matrix_program_blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetDepthMatrixPixelShader(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_depth_matrix_program_blob[0]->Data(), s_depth_matrix_program_blob[0]->Size() };
	}
	else if (s_depth_matrix_program_blob[1])
	{
		bytecode = { s_depth_matrix_program_blob[1]->Data(), s_depth_matrix_program_blob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(s_depth_matrix_program_msaa_hlsl, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_depth_matrix_program_blob[1]);

		bytecode = { s_depth_matrix_program_blob[1]->Data(), s_depth_matrix_program_blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetClearPixelShader()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_clear_program_blob->Size();
	shader.pShaderBytecode = s_clear_program_blob->Data();

	return shader;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetAnaglyphPixelShader()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_anaglyph_program_blob->Size();
	shader.pShaderBytecode = s_anaglyph_program_blob->Data();

	return shader;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetSimpleVertexShader()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_simple_vertex_shader_blob->Size();
	shader.pShaderBytecode = s_simple_vertex_shader_blob->Data();

	return shader;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetClearVertexShader()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_simple_clear_vertex_shader_blob->Size();
	shader.pShaderBytecode = s_simple_clear_vertex_shader_blob->Data();

	return shader;
}

D3D12_INPUT_LAYOUT_DESC StaticShaderCache::GetSimpleVertexShaderInputLayout()
{
	return s_simple_vertex_shader_input_layout;
}

D3D12_INPUT_LAYOUT_DESC StaticShaderCache::GetClearVertexShaderInputLayout()
{
	return s_clear_vertex_shader_input_layout;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetClearGeometryShader()
{
	D3D12_SHADER_BYTECODE bytecode = {};
	if (g_ActiveConfig.iStereoMode > 0)
	{
		bytecode.BytecodeLength = s_clear_geometry_shader_blob->Size();
		bytecode.pShaderBytecode = s_clear_geometry_shader_blob->Data();
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE StaticShaderCache::GetCopyGeometryShader()
{
	D3D12_SHADER_BYTECODE bytecode = {};
	if (g_ActiveConfig.iStereoMode > 0)
	{
		bytecode.BytecodeLength = s_copy_geometry_shader_blob->Size();
		bytecode.pShaderBytecode = s_copy_geometry_shader_blob->Data();
	}

	return bytecode;
}

void StaticShaderCache::Init()
{
	// Compile static pixel shaders
	D3D::CompilePixelShader(s_clear_program_hlsl, &s_clear_program_blob);
	D3D::CompilePixelShader(s_anaglyph_program_hlsl, &s_anaglyph_program_blob);
	D3D::CompilePixelShader(s_color_copy_program_hlsl, &s_color_copy_program_blob[0]);
	D3D::CompilePixelShader(s_depth_copy_program_hlsl, &s_depth_copy_program_blob[0]);
	D3D::CompilePixelShader(s_color_matrix_program_hlsl, &s_color_matrix_program_blob[0]);
	D3D::CompilePixelShader(s_depth_matrix_program_hlsl, &s_depth_matrix_program_blob[0]);

	// Compile static vertex shaders
	D3D::CompileVertexShader(s_simple_vertex_shader_hlsl, &s_simple_vertex_shader_blob);
	D3D::CompileVertexShader(s_clear_vertex_shader_hlsl, &s_simple_clear_vertex_shader_blob);

	// Compile static geometry shaders
	D3D::CompileGeometryShader(s_clear_geometry_shader_hlsl, &s_clear_geometry_shader_blob);
	D3D::CompileGeometryShader(s_copy_geometry_shader_hlsl, &s_copy_geometry_shader_blob);
}

// Call this when multisampling mode changes, and shaders need to be regenerated.
void StaticShaderCache::InvalidateMSAAShaders()
{
	SAFE_RELEASE(s_color_copy_program_blob[1]);
	SAFE_RELEASE(s_color_matrix_program_blob[1]);
	SAFE_RELEASE(s_depth_matrix_program_blob[1]);
	SAFE_RELEASE(s_rgb8_to_rgba6_program_blob[1]);
	SAFE_RELEASE(s_rgba6_to_rgb8_program_blob[1]);
}

void StaticShaderCache::Shutdown()
{
	// Free pixel shader blobs

	SAFE_RELEASE(s_clear_program_blob);
	SAFE_RELEASE(s_anaglyph_program_blob);

	for (unsigned int i = 0; i < 2; ++i)
	{
		SAFE_RELEASE(s_color_copy_program_blob[i]);
		SAFE_RELEASE(s_color_matrix_program_blob[i]);
		SAFE_RELEASE(s_depth_matrix_program_blob[i]);
		SAFE_RELEASE(s_rgba6_to_rgb8_program_blob[i]);
		SAFE_RELEASE(s_rgb8_to_rgba6_program_blob[i]);
	}

	// Free vertex shader blobs

	SAFE_RELEASE(s_simple_vertex_shader_blob);
	SAFE_RELEASE(s_simple_clear_vertex_shader_blob);

	// Free geometry shader blobs

	SAFE_RELEASE(s_clear_geometry_shader_blob);
	SAFE_RELEASE(s_copy_geometry_shader_blob);
}

}