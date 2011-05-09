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

#include "VirtualEFBCopy.h"

#include "D3DShader.h"
#include "EFBCopy.h"
#include "FramebufferManager.h"
#include "Render.h"
#include "TextureCache.h"
#include "VertexShaderCache.h"

namespace DX9
{
	
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

static const UINT C_MATRIX_LOC = 0;
static const UINT C_ADD_LOC = 4;
static const UINT C_SOURCERECT_LOC = 5;
static const UINT C_DISABLEALPHA_LOC = 6;

static const char VIRTUAL_EFB_COPY_PS[] =
"// dolphin-emu DX9 virtual efb copy pixel shader\n"

// Constants
// c_Matrix: Color matrix
"uniform float4x4 c_Matrix : register(c0);\n"
// c_Add: Color add
"uniform float4 c_Add : register(c4);\n"
// c_SourceRect: left, top, right, bottom of source rectangle in texture coordinates
"uniform float4 c_SourceRect : register(c5);\n"
// c_DisableAlpha: If true, alpha will read as 1 from the source
"uniform bool c_DisableAlpha : register(c6);\n"

// Samplers
"uniform sampler s_EFBTexture : register(s0);\n"

// Main entry point
"void main(out float4 ocol0 : COLOR0, in float2 uv0 : TEXCOORD0)\n"
"{\n"
	// TODO: Implement fully
	"float2 coord = lerp(c_SourceRect.xy, c_SourceRect.zw, uv0.xy);\n"
	"float4 pixel = tex2D(s_EFBTexture, coord);\n"
	"if (c_DisableAlpha)\n"
		"pixel.a = 1;\n"
	"ocol0 = mul(pixel, c_Matrix) + c_Add;\n"
"}\n"
;

VirtualCopyShaderManager::VirtualCopyShaderManager()
	: m_shader(NULL)
{ }

VirtualCopyShaderManager::~VirtualCopyShaderManager()
{
	SAFE_RELEASE(m_shader);
}

LPDIRECT3DPIXELSHADER9 VirtualCopyShaderManager::GetShader(bool scale, bool depth)
{
	if (!m_shader)
	{
		INFO_LOG(VIDEO, "Compiling virtual efb copy shader scale %d, depth %d",
			scale ? 1 : 0, depth ? 1 : 0);

		m_shader = D3D::CompileAndCreatePixelShader(VIRTUAL_EFB_COPY_PS,
			sizeof(VIRTUAL_EFB_COPY_PS));
		if (!m_shader)
		{
			ERROR_LOG(VIDEO, "Failed to create virtual EFB copy pixel shader");
			return NULL;
		}
	}

	return m_shader;
}

VirtualEFBCopy::VirtualEFBCopy()
	: m_texture(NULL)
{ }

VirtualEFBCopy::~VirtualEFBCopy()
{
	SAFE_RELEASE(m_texture);
}

// TODO: These matrices should be moved to a backend-independent place.
// They are used for any virtual EFB copy system that encodes to an RGBA
// texture.
static const float RGBA_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

static const float RGB0_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 0
};

static const float RRRA_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 0, 0, 1
};

static const float AAAA_MATRIX[4*4] = {
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1
};

static const float RRRR_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0
};

static const float GGGG_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0
};

static const float BBBB_MATRIX[4*4] = {
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0
};

static const float RRRG_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 1, 0, 0
};

static const float GGGB_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0
};

static const float ZERO_ADD[4] = { 0, 0, 0, 0 };
static const float A1_ADD[4] = { 0, 0, 0, 1 };

void VirtualEFBCopy::Update(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
	bool scaleByHalf)
{
	HRESULT hr;

	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	unsigned int newRealW = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newRealH = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(correctSrc);
	unsigned int newVirtualW = targetRect.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newVirtualH = targetRect.GetHeight() / (scaleByHalf ? 2 : 1);

	bool recreateTexture = !m_texture || newVirtualW != m_virtualW || newVirtualH != m_virtualH;

	if (recreateTexture)
	{
		INFO_LOG(VIDEO, "Creating new storage for virtual efb copy: size %dx%d",
			newVirtualW, newVirtualH);

		SAFE_RELEASE(m_texture);
		hr = D3D::dev->CreateTexture(newVirtualW, newVirtualH, 1, D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_texture, NULL);
		if (FAILED(hr))
		{
			ERROR_LOG(VIDEO, "Failed to create texture for virtual EFB copy");
			return;
		}
	}

	LPDIRECT3DTEXTURE9 efbTexture = (srcFormat == PIXELFMT_Z24)
		? FramebufferManager::GetEFBDepthTexture()
		: FramebufferManager::GetEFBColorTexture();
	
	const float* colorMatrix;
	const float* colorAdd = ZERO_ADD;

	switch (dstFormat)
	{
	case EFB_COPY_R4:
	case EFB_COPY_R8_1:
	case EFB_COPY_R8:
		colorMatrix = RRRR_MATRIX;
		break;
	case EFB_COPY_RA4:
	case EFB_COPY_RA8:
		colorMatrix = RRRA_MATRIX;
		break;
	case EFB_COPY_RGB565:
		colorMatrix = RGB0_MATRIX;
		colorAdd = A1_ADD;
		break;
	case EFB_COPY_RGB5A3:
	case EFB_COPY_RGBA8:
		colorMatrix = RGBA_MATRIX;
		break;
	case EFB_COPY_A8:
		colorMatrix = AAAA_MATRIX;
		break;
	case EFB_COPY_G8:
		colorMatrix = GGGG_MATRIX;
		break;
	case EFB_COPY_B8:
		colorMatrix = BBBB_MATRIX;
		break;
	case EFB_COPY_RG8:
		colorMatrix = RRRG_MATRIX;
		break;
	case EFB_COPY_GB8:
		colorMatrix = GGGB_MATRIX;
		break;
	default:
		ERROR_LOG(VIDEO, "Couldn't fake this EFB copy format 0x%X", dstFormat);
		SAFE_RELEASE(m_texture);
		return;
	}

	VirtualizeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
		srcRect, newVirtualW, newVirtualH,
		colorMatrix, colorAdd);

	m_realW = newRealW;
	m_realH = newRealH;
	m_virtualW = newVirtualW;
	m_virtualH = newVirtualH;
	m_dstFormat = dstFormat;
	m_dirty = true;
}

inline bool IsPaletted(u32 format) {
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

LPDIRECT3DTEXTURE9 VirtualEFBCopy::Virtualize(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool force)
{
	// FIXME: Check if encoded dstFormat and texture format are compatible,
	// reinterpret or fall back to RAM if necessary

	static const char* DST_FORMAT_NAMES[] = {
		"R4", "R8_1", "RA4", "RA8", "RGB565", "RGB5A3", "RGBA8", "A8",
		"R8", "G8", "B8", "RG8", "GB8", "0xD", "0xE", "0xF"
	};

	static const char* TEX_FORMAT_NAMES[] = {
		"I4", "I8", "IA4", "IA8", "RGB565", "RGB5A3", "RGBA8", "0x7",
		"C4", "C8", "C14X2", "0xB", "0xC", "0xD", "CMPR", "0xF"
	};

	if (!force && IsPaletted(format))
	{
		// TODO: Support interpreting virtual EFB copies with palettes
		INFO_LOG(VIDEO, "Paletted virtual EFB copies not implemented; falling back to RAM");
		return NULL;
	}
	
	INFO_LOG(VIDEO, "Interpreting dstFormat %s as tex format %s",
		DST_FORMAT_NAMES[m_dstFormat], TEX_FORMAT_NAMES[format]);

	return m_texture;
}

void VirtualEFBCopy::VirtualizeShade(LPDIRECT3DTEXTURE9 texSrc, unsigned int srcFormat,
	bool yuva, bool scale,
	const EFBRectangle& srcRect,
	unsigned int virtualW, unsigned int virtualH,
	const float* colorMatrix, const float* colorAdd)
{
	VirtualCopyShaderManager& shaderMan = ((TextureCache*)g_textureCache)->GetVirtShaderManager();
	LPDIRECT3DPIXELSHADER9 shader = shaderMan.GetShader(scale, srcFormat == PIXELFMT_Z24);
	if (!shader)
		return;

	INFO_LOG(VIDEO, "Doing efb virtual shader");

	g_renderer->ResetAPIState();

	D3D::dev->SetDepthStencilSurface(NULL);
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	m_texture->GetSurfaceLevel(0, &renderSurface);
	D3D::dev->SetRenderTarget(0, renderSurface);

	D3DVIEWPORT9 vp = { 0, 0, virtualW, virtualH, 0.f, 1.f };
	D3D::dev->SetViewport(&vp);

	// Set shader constants
	D3D::dev->SetPixelShaderConstantF(C_MATRIX_LOC, colorMatrix, 4);
	D3D::dev->SetPixelShaderConstantF(C_ADD_LOC, colorAdd, 1);
	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(srcRect);
	FLOAT cSourceRect[4] = {
		FLOAT(targetRect.left) / Renderer::GetTargetWidth(),
		FLOAT(targetRect.top) / Renderer::GetTargetHeight(),
		FLOAT(targetRect.right) / Renderer::GetTargetWidth(),
		FLOAT(targetRect.bottom) / Renderer::GetTargetHeight()
	};
	D3D::dev->SetPixelShaderConstantF(C_SOURCERECT_LOC, cSourceRect, 1);
	BOOL cDisableAlpha = (srcFormat != PIXELFMT_RGBA6_Z24) ? TRUE : FALSE;
	D3D::dev->SetPixelShaderConstantB(C_DISABLEALPHA_LOC, &cDisableAlpha, 1);

	// Encode!
	RECT rectSrc = { 0, 0, 1, 1 };
	D3D::drawShadedTexQuad(texSrc,
		&rectSrc,
		1, 1,
		virtualW, virtualH,
		shader,
		VertexShaderCache::GetSimpleVertexShader(0)
		);
	
	// Clean up
	g_renderer->RestoreAPIState();

	SAFE_RELEASE(renderSurface);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

}
