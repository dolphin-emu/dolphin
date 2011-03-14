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

#include "LineGeometryShader.h"

#include <sstream>
#include "D3DBase.h"
#include "D3DShader.h"
#include "VertexShaderGen.h"

namespace DX11
{

union LineGSParams
{
	struct
	{
		FLOAT LineWidth; // In units of 1/6 of an EFB pixel
		FLOAT TexOffset;
	};
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[16]; // Pad to the next multiple of 16 bytes
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
	"} Params;\n"
"}\n"

"[maxvertexcount(4)]\n"
"void main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> outStream)\n"
"{\n"
	// Pretend input[0] is on the bottom and input[1] is on top.
	// We generate vertices to the left and right.

	// Correct w coordinate so screen-space math will work
	"VS_OUTPUT l0 = input[0];\n"
	"l0.pos /= l0.pos.w;\n"
	"VS_OUTPUT r0 = l0;\n"
	"VS_OUTPUT l1 = input[1];\n"
	"l1.pos /= l1.pos.w;\n"
	"VS_OUTPUT r1 = l1;\n"

	// GameCube/Wii's line drawing algorithm is a little quirky. It does not
	// use the correct line caps. Instead, the line caps are vertical or
	// horizontal depending the slope of the line.

	"float2 offset;\n"
	"float2 to = input[1].pos.xy - input[0].pos.xy;\n"
	// FIXME: What does real hardware do when line is at a 45-degree angle?
	// FIXME: Lines aren't drawn at the correct width. See Twilight Princess map.
	"if (abs(to.y) > abs(to.x)) {\n"
		// Line is more tall. Extend geometry left and right.
		// Lerp Params.LineWidth/2 from [0..640] to [-1..1]
		"offset = float2(Params.LineWidth/640, 0);\n"
	"} else {\n"
		// Line is more wide. Extend geometry up and down.
		// Lerp Params.LineWidth/2 from [0..528] to [1..-1]
		"offset = float2(0, -Params.LineWidth/528);\n"
	"}\n"

	"l0.pos.xy -= offset;\n"
	"r0.pos.xy += offset;\n"
	"l1.pos.xy -= offset;\n"
	"r1.pos.xy += offset;\n"

"#ifndef NUM_TEXCOORDS\n"
"#error NUM_TEXCOORDS not defined\n"
"#endif\n"

	// Apply TexOffset to all tex coordinates in the vertex
"#if NUM_TEXCOORDS >= 1\n"
	"r0.tex0.x += Params.TexOffset;\n"
	"r1.tex0.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 2\n"
	"r0.tex1.x += Params.TexOffset;\n"
	"r1.tex1.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 3\n"
	"r0.tex2.x += Params.TexOffset;\n"
	"r1.tex2.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 4\n"
	"r0.tex3.x += Params.TexOffset;\n"
	"r1.tex3.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 5\n"
	"r0.tex4.x += Params.TexOffset;\n"
	"r1.tex4.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 6\n"
	"r0.tex5.x += Params.TexOffset;\n"
	"r1.tex5.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 7\n"
	"r0.tex6.x += Params.TexOffset;\n"
	"r1.tex6.x += Params.TexOffset;\n"
"#endif\n"
"#if NUM_TEXCOORDS >= 8\n"
	"r0.tex7.x += Params.TexOffset;\n"
	"r1.tex7.x += Params.TexOffset;\n"
"#endif\n"

	"outStream.Append(l0);\n"
	"outStream.Append(r0);\n"
	"outStream.Append(l1);\n"
	"outStream.Append(r1);\n"
"}\n"
;

LineGeometryShader::LineGeometryShader()
	: m_ready(false), m_paramsBuffer(NULL)
{ }

void LineGeometryShader::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create constant buffer for uploading data to geometry shader
	
	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(LineGSParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, NULL, &m_paramsBuffer);
	CHECK(SUCCEEDED(hr), "create line geometry shader params buffer");
	D3D::SetDebugObjectName(m_paramsBuffer, "line geometry shader params buffer");

	m_ready = true;
}

void LineGeometryShader::Shutdown()
{
	m_ready = false;

	for (ComboMap::iterator it = m_shaders.begin(); it != m_shaders.end(); ++it)
	{
		SAFE_RELEASE(it->second);
	}
	m_shaders.clear();

	SAFE_RELEASE(m_paramsBuffer);
}

bool LineGeometryShader::SetShader(u32 components, float lineWidth, float texOffset)
{
	if (!m_ready)
		return false;

	// Make sure geometry shader for "components" is available
	ComboMap::iterator shaderIt = m_shaders.find(components);
	if (shaderIt == m_shaders.end())
	{
		// Generate new shader. Warning: not thread-safe.
		static char code[16384];
		char* p = code;
		p = GenerateVSOutputStruct(p, components, API_D3D11);
		p += sprintf(p, "\n%s", LINE_GS_COMMON);
		
		std::stringstream numTexCoordsStr;
		numTexCoordsStr << xfregs.numTexGen.numTexGens;
		
		INFO_LOG(VIDEO, "Compiling line geometry shader for components 0x%.08X (num texcoords %d)",
			components, xfregs.numTexGen.numTexGens);

		D3D_SHADER_MACRO macros[] = {
			{ "NUM_TEXCOORDS", numTexCoordsStr.str().c_str() },
			{ NULL, NULL }
		};
		ID3D11GeometryShader* newShader = D3D::CompileAndCreateGeometryShader(code, unsigned int(strlen(code)), macros);
		if (!newShader)
		{
			WARN_LOG(VIDEO, "Line geometry shader for components 0x%.08X failed to compile", components);
			// Add dummy shader to prevent trying to compile again
			m_shaders[components] = NULL;
			return false;
		}

		shaderIt = m_shaders.insert(std::make_pair(components, newShader)).first;
	}

	if (shaderIt != m_shaders.end())
	{
		if (shaderIt->second)
		{
			LineGSParams params = { 0 };
			params.LineWidth = lineWidth;
			params.TexOffset = texOffset;
			D3D::context->UpdateSubresource(m_paramsBuffer, 0, NULL, &params, 0, 0);

			D3D::context->GSSetShader(shaderIt->second, NULL, 0);
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
