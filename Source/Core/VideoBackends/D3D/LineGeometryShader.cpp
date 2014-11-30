// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/LineGeometryShader.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11
{

struct LineGSParams
{
	FLOAT LineWidth; // In units of 1/6 of an EFB pixel
	FLOAT TexOffset;
	FLOAT VpWidth; // Width and height of the viewport in EFB pixels
	FLOAT VpHeight;
	FLOAT TexOffsetEnable[8]; // For each tex coordinate, whether to apply offset to it (1 on, 0 off)
};

union LineGSParams_Padded
{
	LineGSParams params;
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[(sizeof(LineGSParams) + 15) & ~15];
};

static const char LINE_GS_COMMON[] =
// The struct VS_OUTPUT used by the vertex shader goes here.
"// dolphin-emu line geometry shader common part\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match LineGSParams above
	"{\n"
		"float LineWidth;\n"
		"float TexOffset;\n"
		"float VpWidth;\n"
		"float VpHeight;\n"
		"float TexOffsetEnable[8];\n"
	"} Params;\n"
"}\n"

"[maxvertexcount(4)]\n"
"void main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> outStream)\n"
"{\n"
	// Pretend input[0] is on the bottom and input[1] is on top.
	// We generate vertices to the left and right.

	"VS_OUTPUT l0 = input[0];\n"
	"VS_OUTPUT r0 = l0;\n"
	"VS_OUTPUT l1 = input[1];\n"
	"VS_OUTPUT r1 = l1;\n"

	// GameCube/Wii's line drawing algorithm is a little quirky. It does not
	// use the correct line caps. Instead, the line caps are vertical or
	// horizontal depending the slope of the line.

	"float2 offset;\n"
	"float2 to = abs(input[1].pos.xy - input[0].pos.xy);\n"
	// FIXME: What does real hardware do when line is at a 45-degree angle?
	// FIXME: Lines aren't drawn at the correct width. See Twilight Princess map.
	"if (Params.VpHeight*to.y > Params.VpWidth*to.x) {\n"
		// Line is more tall. Extend geometry left and right.
		// Lerp Params.LineWidth/2 from [0..VpWidth] to [-1..1]
		"offset = float2(Params.LineWidth/Params.VpWidth, 0);\n"
	"} else {\n"
		// Line is more wide. Extend geometry up and down.
		// Lerp Params.LineWidth/2 from [0..VpHeight] to [1..-1]
		"offset = float2(0, -Params.LineWidth/Params.VpHeight);\n"
	"}\n"

	"l0.pos.xy -= offset * input[0].pos.w;\n"
	"r0.pos.xy += offset * input[0].pos.w;\n"
	"l1.pos.xy -= offset * input[1].pos.w;\n"
	"r1.pos.xy += offset * input[1].pos.w;\n"

"#ifndef NUM_TEXCOORDS\n"
"#error NUM_TEXCOORDS not defined\n"
"#endif\n"

	// Apply TexOffset to all tex coordinates in the vertex.
	// They can each be enabled separately.
"#if NUM_TEXCOORDS >= 1\n"
	"r0.tex0.x += Params.TexOffset * Params.TexOffsetEnable[0];\n"
	"r1.tex0.x += Params.TexOffset * Params.TexOffsetEnable[0];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 2\n"
	"r0.tex1.x += Params.TexOffset * Params.TexOffsetEnable[1];\n"
	"r1.tex1.x += Params.TexOffset * Params.TexOffsetEnable[1];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 3\n"
	"r0.tex2.x += Params.TexOffset * Params.TexOffsetEnable[2];\n"
	"r1.tex2.x += Params.TexOffset * Params.TexOffsetEnable[2];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 4\n"
	"r0.tex3.x += Params.TexOffset * Params.TexOffsetEnable[3];\n"
	"r1.tex3.x += Params.TexOffset * Params.TexOffsetEnable[3];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 5\n"
	"r0.tex4.x += Params.TexOffset * Params.TexOffsetEnable[4];\n"
	"r1.tex4.x += Params.TexOffset * Params.TexOffsetEnable[4];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 6\n"
	"r0.tex5.x += Params.TexOffset * Params.TexOffsetEnable[5];\n"
	"r1.tex5.x += Params.TexOffset * Params.TexOffsetEnable[5];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 7\n"
	"r0.tex6.x += Params.TexOffset * Params.TexOffsetEnable[6];\n"
	"r1.tex6.x += Params.TexOffset * Params.TexOffsetEnable[6];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 8\n"
	"r0.tex7.x += Params.TexOffset * Params.TexOffsetEnable[7];\n"
	"r1.tex7.x += Params.TexOffset * Params.TexOffsetEnable[7];\n"
"#endif\n"

	"outStream.Append(l0);\n"
	"outStream.Append(r0);\n"
	"outStream.Append(l1);\n"
	"outStream.Append(r1);\n"
"}\n"
;

LineGeometryShader::LineGeometryShader()
	: m_ready(false), m_paramsBuffer(nullptr)
{ }

void LineGeometryShader::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create constant buffer for uploading data to geometry shader

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(LineGSParams_Padded),
		D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateBuffer(&bd, nullptr, &m_paramsBuffer);
	CHECK(SUCCEEDED(hr), "create line geometry shader params buffer");
	D3D::SetDebugObjectName(m_paramsBuffer, "line geometry shader params buffer");

	m_ready = true;
}

void LineGeometryShader::Shutdown()
{
	m_ready = false;

	for (auto& it : m_shaders)
	{
		SAFE_RELEASE(it.second);
	}
	m_shaders.clear();

	SAFE_RELEASE(m_paramsBuffer);
}

bool LineGeometryShader::SetShader(u32 components, float lineWidth,
	float texOffset, float vpWidth, float vpHeight, const bool* texOffsetEnable)
{
	if (!m_ready)
		return false;

	// Make sure geometry shader for "components" is available
	ComboMap::iterator shaderIt = m_shaders.find(components);
	if (shaderIt == m_shaders.end())
	{
		// Generate new shader. Warning: not thread-safe.
		static char buffer[16384];
		ShaderCode code;
		code.SetBuffer(buffer);
		GenerateVSOutputStruct(code, API_D3D);
		code.Write("\n%s", LINE_GS_COMMON);

		std::stringstream numTexCoordsStream;
		numTexCoordsStream << xfmem.numTexGen.numTexGens;

		INFO_LOG(VIDEO, "Compiling line geometry shader for components 0x%.08X (num texcoords %d)",
			components, xfmem.numTexGen.numTexGens);

		const std::string& numTexCoordsStr = numTexCoordsStream.str();
		D3D_SHADER_MACRO macros[] = {
			{ "NUM_TEXCOORDS", numTexCoordsStr.c_str() },
			{ nullptr, nullptr }
		};
		ID3D11GeometryShader* newShader = D3D::CompileAndCreateGeometryShader(code.GetBuffer(), macros);
		if (!newShader)
		{
			WARN_LOG(VIDEO, "Line geometry shader for components 0x%.08X failed to compile", components);
			// Add dummy shader to prevent trying to compile again
			m_shaders[components] = nullptr;
			return false;
		}

		shaderIt = m_shaders.insert(std::make_pair(components, newShader)).first;
	}

	if (shaderIt != m_shaders.end())
	{
		if (shaderIt->second)
		{
			D3D11_MAPPED_SUBRESOURCE map;
			HRESULT hr = D3D::context->Map(m_paramsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			if (SUCCEEDED(hr))
			{
				LineGSParams* params = (LineGSParams*)map.pData;
				params->LineWidth = lineWidth;

				params->TexOffset = texOffset;
				params->VpWidth = vpWidth;
				params->VpHeight = vpHeight;
				for (int i = 0; i < 8; ++i)
					params->TexOffsetEnable[i] = texOffsetEnable[i] ? 1.f : 0.f;

				D3D::context->Unmap(m_paramsBuffer, 0);
			}
			else
				ERROR_LOG(VIDEO, "Failed to map line gs params buffer");

			DEBUG_LOG(VIDEO, "Line params: width %f, texOffset %f, vpWidth %f, vpHeight %f",
				lineWidth, texOffset, vpWidth, vpHeight);

			D3D::context->GSSetShader(shaderIt->second, nullptr, 0);
			D3D::context->GSSetConstantBuffers(0, 1, &m_paramsBuffer);

			return true;
		}
		else
			return false;
	}
	else
		return false;
}

}
