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

#include <d3dx9.h>

#include "Statistics.h"
#include "MemoryUtil.h"
#include "Hash.h"

#include "CommonPaths.h"
#include "FileUtil.h"

#include "DX9_D3DBase.h"
#include "DX9_D3DTexture.h"
#include "DX9_D3DUtil.h"
#include "DX9_FramebufferManager.h"
#include "DX9_PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "DX9_VertexShaderCache.h"
#include "VideoConfig.h"

#include "DX9_Render.h"

#include "TextureDecoder.h"
#include "DX9_TextureCache.h"
#include "HiresTextures.h"
#include "DX9_TextureConverter.h"

#include "../Main.h"

//#include "debugger/debugger.h"

namespace DX9
{

TextureCache::TCacheEntry::TCacheEntry(LPDIRECT3DTEXTURE9 _texture)
	: texture(_texture), isDynamic(false)
{
	
}

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	D3D::ReplaceTexture2D(texture, temp, width, height, expanded_width, d3d_fmt, swap_r_b, level);
}

void TextureCache::TCacheEntry::FromRenderTarget(bool bFromZBuffer, bool bScaleByHalf,
	unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect)
{
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	texture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);
	
	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = Scaledw;
	vp.Height = Scaledh;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT destrect;
	destrect.bottom = Scaledh;
	destrect.left = 0;
	destrect.right = Scaledw;
	destrect.top = 0;

	const float* const fConstAdd = colmat + 16;		// fConstAdd is the last 4 floats of colmat
	PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
	TargetRectangle targetSource = Renderer::ConvertEFBRectangle(source_rect);
	RECT sourcerect;
	sourcerect.bottom = targetSource.bottom;
	sourcerect.left = targetSource.left;
	sourcerect.right = targetSource.right;
	sourcerect.top = targetSource.top;

	if (bFromZBuffer)
	{
		if (bScaleByHalf || g_ActiveConfig.iMultisampleMode)
		{
			D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		}
		else
		{
			D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		}
	}
	else
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}
	
	const LPDIRECT3DTEXTURE9 read_texture = bFromZBuffer ?
		FramebufferManager::GetEFBDepthTexture(source_rect) :
		FramebufferManager::GetEFBColorTexture(source_rect);

	D3DFORMAT bformat = FramebufferManager::GetEFBDepthRTSurfaceFormat();
	int SSAAMode = g_ActiveConfig.iMultisampleMode;

	D3D::drawShadedTexQuad(read_texture, &sourcerect, 
		Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(),
		Scaledw, Scaledh,
		((bformat != FOURCC_RAWZ && bformat != D3DFMT_D24X8) && bFromZBuffer) ?
			PixelShaderCache::GetDepthMatrixProgram(SSAAMode) :
			PixelShaderCache::GetColorMatrixProgram(SSAAMode),
		VertexShaderCache::GetSimpleVertexShader(SSAAMode));

	Rendersurf->Release();
	
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0,NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::SetTexture(stage, texture);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt)
{
	D3DFORMAT d3d_fmt;
	bool swap_r_b = false;

	switch (pcfmt)
	{
	case PC_TEX_FMT_BGRA32:
		d3d_fmt = D3DFMT_A8R8G8B8;
		break;

	case PC_TEX_FMT_RGBA32:
		d3d_fmt = D3DFMT_A8R8G8B8;
		swap_r_b = true;
		break;

	case PC_TEX_FMT_RGB565:
		d3d_fmt = D3DFMT_R5G6B5;
		break;

	case PC_TEX_FMT_IA4_AS_IA8:
		d3d_fmt = D3DFMT_A8L8;
		break;

	case PC_TEX_FMT_I8:
	case PC_TEX_FMT_I4_AS_I8:
		// A hack which means the format is a packed
		// 8-bit intensity texture. It is unpacked
		// to A8L8 in D3DTexture.cpp
		d3d_fmt = D3DFMT_A8P8;
		break;

	case PC_TEX_FMT_IA8:
		d3d_fmt = D3DFMT_A8L8;
		break;

	case PC_TEX_FMT_DXT1:
		d3d_fmt = D3DFMT_DXT1;
		break;
	}

	TCacheEntry* entry = new TCacheEntry(D3D::CreateTexture2D(temp, width, height, expanded_width, d3d_fmt, swap_r_b, tex_levels));
	entry->swap_r_b = swap_r_b;
	entry->d3d_fmt = d3d_fmt;
	
	return entry;
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h)
{
	LPDIRECT3DTEXTURE9 texture;
	D3D::dev->CreateTexture(scaled_tex_w, scaled_tex_h, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, 0);
	
	return new TCacheEntry(texture);
}

}
