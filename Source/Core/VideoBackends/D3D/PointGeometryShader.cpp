// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/PointGeometryShader.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11
{

struct PointGSParams
{
	FLOAT PointSize; // In units of 1/6 of an EFB pixel
	FLOAT TexOffset;
	FLOAT VpWidth; // Width and height of viewport in EFB pixels
	FLOAT VpHeight;
	FLOAT TexOffsetEnable[8]; // For each tex coordinate, whether to apply offset to it (1 on, 0 off)
};

union PointGSParams_Padded
{
	PointGSParams params;
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[(sizeof(PointGSParams) + 15) & ~15];
};

static const char POINT_GS_COMMON[] =
// The struct VS_OUTPUT used by the vertex shader goes here.
"// dolphin-emu point geometry shader common part\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match PointGSParams above
	"{\n"
		"float PointSize;\n"
		"float TexOffset;\n"
		"float VpWidth;\n"
		"float VpHeight;\n"
		"float TexOffsetEnable[8];\n"
	"} Params;\n"
"}\n"

"[maxvertexcount(4)]\n"
"void main(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> outStream)\n"
"{\n"
	"VS_OUTPUT ptLL = input[0];\n"
	"VS_OUTPUT ptLR = ptLL;\n"
	"VS_OUTPUT ptUL = ptLL;\n"
	"VS_OUTPUT ptUR = ptLL;\n"

	// Offset from center to upper right vertex
	// Lerp Params.PointSize/2 from [0,0..VpWidth,VpHeight] to [-1,1..1,-1]
	"float2 offset = float2(Params.PointSize/Params.VpWidth, -Params.PointSize/Params.VpHeight) * input[0].pos.w;\n"

	"ptLL.pos.xy += float2(-1,-1) * offset;\n"
	"ptLR.pos.xy += float2(1,-1) * offset;\n"
	"ptUL.pos.xy += float2(-1,1) * offset;\n"
	"ptUR.pos.xy += offset;\n"

	"float2 texOffset = float2(Params.TexOffset, Params.TexOffset);\n"

"#ifndef NUM_TEXCOORDS\n"
"#error NUM_TEXCOORDS not defined\n"
"#endif\n"

	// Apply TexOffset to all tex coordinates in the vertex
	// FIXME: The game may be able to enable TexOffset for some coords and
	// disable for others, but where is that information stored?
"#if NUM_TEXCOORDS >= 1\n"
	"ptLL.tex0.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[0];\n"
	"ptLR.tex0.xy += texOffset * Params.TexOffsetEnable[0];\n"
	"ptUR.tex0.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[0];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 2\n"
	"ptLL.tex1.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[1];\n"
	"ptLR.tex1.xy += texOffset * Params.TexOffsetEnable[1];\n"
	"ptUR.tex1.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[1];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 3\n"
	"ptLL.tex2.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[2];\n"
	"ptLR.tex2.xy += texOffset * Params.TexOffsetEnable[2];\n"
	"ptUR.tex2.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[2];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 4\n"
	"ptLL.tex3.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[3];\n"
	"ptLR.tex3.xy += texOffset * Params.TexOffsetEnable[3];\n"
	"ptUR.tex3.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[3];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 5\n"
	"ptLL.tex4.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[4];\n"
	"ptLR.tex4.xy += texOffset * Params.TexOffsetEnable[4];\n"
	"ptUR.tex4.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[4];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 6\n"
	"ptLL.tex5.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[5];\n"
	"ptLR.tex5.xy += texOffset * Params.TexOffsetEnable[5];\n"
	"ptUR.tex5.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[5];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 7\n"
	"ptLL.tex6.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[6];\n"
	"ptLR.tex6.xy += texOffset * Params.TexOffsetEnable[6];\n"
	"ptUR.tex6.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[6];\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 8\n"
	"ptLL.tex7.xy += float2(0,1) * texOffset * Params.TexOffsetEnable[7];\n"
	"ptLR.tex7.xy += texOffset * Params.TexOffsetEnable[7];\n"
	"ptUR.tex7.xy += float2(1,0) * texOffset * Params.TexOffsetEnable[7];\n"
"#endif\n"

	"outStream.Append(ptLL);\n"
	"outStream.Append(ptLR);\n"
	"outStream.Append(ptUL);\n"
	"outStream.Append(ptUR);\n"
"}\n"
;

PointGeometryShader::PointGeometryShader()
	: m_ready(false), m_paramsBuffer(nullptr)
{ }

void PointGeometryShader::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create constant buffer for uploading data to geometry shader

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(PointGSParams_Padded),
		D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateBuffer(&bd, nullptr, &m_paramsBuffer);
	CHECK(SUCCEEDED(hr), "create point geometry shader params buffer");
	D3D::SetDebugObjectName(m_paramsBuffer, "point geometry shader params buffer");

	m_ready = true;
}

void PointGeometryShader::Shutdown()
{
	m_ready = false;

	for (auto& it : m_shaders)
	{
		SAFE_RELEASE(it.second);
	}
	m_shaders.clear();

	SAFE_RELEASE(m_paramsBuffer);
}

bool PointGeometryShader::SetShader(u32 components, float pointSize,
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
		code.Write("\n%s", POINT_GS_COMMON);

		std::stringstream numTexCoordsStream;
		numTexCoordsStream << xfmem.numTexGen.numTexGens;

		INFO_LOG(VIDEO, "Compiling point geometry shader for components 0x%.08X (num texcoords %d)",
			components, xfmem.numTexGen.numTexGens);

		const std::string& numTexCoordsStr = numTexCoordsStream.str();
		D3D_SHADER_MACRO macros[] = {
			{ "NUM_TEXCOORDS", numTexCoordsStr.c_str() },
			{ nullptr, nullptr }
		};
		ID3D11GeometryShader* newShader = D3D::CompileAndCreateGeometryShader(code.GetBuffer(), macros);
		if (!newShader)
		{
			WARN_LOG(VIDEO, "Point geometry shader for components 0x%.08X failed to compile", components);
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
				PointGSParams* params = (PointGSParams*)map.pData;
				params->PointSize = pointSize;

				params->TexOffset = texOffset;
				params->VpWidth = vpWidth;
				params->VpHeight = vpHeight;
				for (int i = 0; i < 8; ++i)
					params->TexOffsetEnable[i] = texOffsetEnable[i] ? 1.f : 0.f;

				D3D::context->Unmap(m_paramsBuffer, 0);
			}
			else
				ERROR_LOG(VIDEO, "Failed to map point gs params buffer");

			DEBUG_LOG(VIDEO, "Point params: size %f, texOffset %f, vpWidth %f, vpHeight %f",
				pointSize, texOffset, vpWidth, vpHeight);

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
