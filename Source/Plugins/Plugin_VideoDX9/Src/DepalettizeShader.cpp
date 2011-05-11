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

#include "DepalettizeShader.h"

#include "D3DShader.h"
#include "FramebufferManager.h"
#include "Render.h"
#include "VertexShaderCache.h"

namespace DX9
{
	
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

static const char DEPALETTIZE_SHADER[] =
"// dolphin-emu depalettizing shader for DX9\n"

"#ifndef NUM_COLORS\n"
"#error NUM_COLORS was not defined\n"
"#endif\n"

"uniform sampler s_Base : register(s0);\n"
// s_Palette is assumed to be a sampler into a 1D texture of width NUM_COLORS.
// TODO: When we support 16384 colors, we'll need to cheat and store the palette
// in multiple rows to avoid having a 16384-wide texture!
"uniform sampler s_Palette : register(s1);\n"

"void main(out float4 ocol0 : COLOR0, in float2 uv0 : TEXCOORD0)\n"
"{\n"
	"float sample = tex2D(s_Base, uv0);\n"
	"float index = floor(sample * (NUM_COLORS-1));\n"
	"ocol0 = tex1D(s_Palette, (index + 0.5) / NUM_COLORS);\n"
"}\n"
;

DepalettizeShader::DepalettizeShader()
	: m_unorm4Shader(NULL), m_unorm8Shader(NULL)
{ }

DepalettizeShader::~DepalettizeShader()
{
	SAFE_RELEASE(m_unorm4Shader);
	SAFE_RELEASE(m_unorm8Shader);
}

void DepalettizeShader::Depalettize(LPDIRECT3DTEXTURE9 dstTex,
	LPDIRECT3DTEXTURE9 baseTex, BaseType baseType, LPDIRECT3DTEXTURE9 paletteTex)
{
	LPDIRECT3DPIXELSHADER9 shader = GetShader(baseType);
	if (!shader)
		return;

	D3DSURFACE_DESC dstDesc;
	dstTex->GetLevelDesc(0, &dstDesc);

	g_renderer->ResetAPIState();

	D3D::dev->SetDepthStencilSurface(NULL);
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	dstTex->GetSurfaceLevel(0, &renderSurface);
	D3D::dev->SetRenderTarget(0, renderSurface);

	D3DVIEWPORT9 vp = { 0, 0, dstDesc.Width, dstDesc.Height, 0.f, 1.f };
	D3D::dev->SetViewport(&vp);

	// Set shader inputs
	// Texture 0 will be set to baseTex by drawShadedTexQuad
	D3D::SetTexture(1, paletteTex);
	
	// Depalettize!
	RECT rectSrc = { 0, 0, 1, 1 };
	D3D::drawShadedTexQuad(baseTex,
		&rectSrc,
		1, 1,
		dstDesc.Width, dstDesc.Height,
		shader,
		VertexShaderCache::GetSimpleVertexShader(0)
		);

	// Clean up
	D3D::SetTexture(1, NULL);
	g_renderer->RestoreAPIState();

	SAFE_RELEASE(renderSurface);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

LPDIRECT3DPIXELSHADER9 DepalettizeShader::GetShader(BaseType type)
{
	switch (type)
	{
	case Unorm4:
		if (!m_unorm4Shader)
		{
			D3DXMACRO macros[] = {
				{ "NUM_COLORS", "16" },
				{ NULL, NULL }
			};
			m_unorm4Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_SHADER,
				sizeof(DEPALETTIZE_SHADER), macros);
		}
		return m_unorm4Shader;
	case Unorm8:
		if (!m_unorm8Shader)
		{
			D3DXMACRO macros[] = {
				{ "NUM_COLORS", "256" },
				{ NULL, NULL }
			};
			m_unorm8Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_SHADER,
				sizeof(DEPALETTIZE_SHADER), macros);
		}
		return m_unorm8Shader;
	default:
		_assert_msg_(VIDEO, 0, "Invalid depalettizer base type");
		return NULL;
	}
}

}
