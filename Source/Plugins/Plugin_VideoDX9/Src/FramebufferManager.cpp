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

#include "D3DBase.h"
#include "Render.h"
#include "FramebufferManager.h"
#include "VideoConfig.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"
#include "TextureConverter.h"

// TODO: this is probably somewhere else
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

#undef CHECK
#define CHECK(hr, Message, ...) if (FAILED(hr)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

FramebufferManager::Efb FramebufferManager::s_efb;

FramebufferManager::FramebufferManager()
{
    s_efb.color_texture = NULL;
    s_efb.colorRead_texture = NULL;
    s_efb.depth_texture = NULL;
    s_efb.depthRead_texture = NULL;

    s_efb.depth_surface = NULL;
    s_efb.color_surface = NULL;
    s_efb.color_ReadBuffer = NULL;
    s_efb.depth_ReadBuffer = NULL;
    s_efb.color_OffScreenReadBuffer = NULL;
    s_efb.depth_OffScreenReadBuffer = NULL;

    s_efb.color_surface_Format = D3DFMT_FORCE_DWORD;
    s_efb.depth_surface_Format = D3DFMT_FORCE_DWORD;
    s_efb.depth_ReadBuffer_Format = D3DFMT_FORCE_DWORD;

	// Simplest possible setup to start with.
	int target_width = Renderer::GetFullTargetWidth();
	int target_height = Renderer::GetFullTargetHeight();

	s_efb.color_surface_Format = D3DFMT_A8R8G8B8;
	// Get the framebuffer texture
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format, 
										 D3DPOOL_DEFAULT, &s_efb.color_texture, NULL);
	if (s_efb.color_texture)
	{
		hr = s_efb.color_texture->GetSurfaceLevel(0, &s_efb.color_surface);
	}
	CHECK(hr, "Create color texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format, 
										  D3DPOOL_DEFAULT, &s_efb.colorRead_texture, NULL);
	CHECK(hr, "Create Color Read Texture (hr=%#x)", hr);
	if (s_efb.colorRead_texture)
	{
		s_efb.colorRead_texture->GetSurfaceLevel(0, &s_efb.color_ReadBuffer);
	}
	// Create an offscreen surface that we can lock to retrieve the data
	hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, s_efb.color_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb.color_OffScreenReadBuffer, NULL);
	CHECK(hr, "Create offscreen color surface (hr=%#x)", hr);

	// Select a Z-buffer format with hardware support
	D3DFORMAT DepthTexFormats[5] = {
		FOURCC_INTZ,
		FOURCC_DF24,
		FOURCC_RAWZ,
		FOURCC_DF16,
		D3DFMT_D24X8
	};

	for (int i = 0; i < 5; ++i)
	{
		s_efb.depth_surface_Format = DepthTexFormats[i];
		// Create the framebuffer depth texture
		hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb.depth_surface_Format, 
									 D3DPOOL_DEFAULT, &s_efb.depth_texture, NULL);
		if (!FAILED(hr))
			break;
	}

	CHECK(hr, "Framebuffer depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	// Get the Surface
	if (s_efb.depth_texture)
	{
		s_efb.depth_texture->GetSurfaceLevel(0, &s_efb.depth_surface);
	}

	// Create a 4x4 pixel texture to work as a buffer for peeking
	if (s_efb.depth_surface_Format == FOURCC_RAWZ || s_efb.depth_surface_Format == D3DFMT_D24X8)
	{
		DepthTexFormats[0] = D3DFMT_A8R8G8B8;
	}
	else
	{
		DepthTexFormats[0] = D3DFMT_R32F;
	}

	DepthTexFormats[1] = D3DFMT_A8R8G8B8;

	for (int i = 0; i < 2; ++i)
	{
		s_efb.depth_ReadBuffer_Format = DepthTexFormats[i];
		// Get the framebuffer Depth texture
		hr = D3D::dev->CreateTexture(4, 4, 1, D3DUSAGE_RENDERTARGET, s_efb.depth_ReadBuffer_Format, 
									  D3DPOOL_DEFAULT, &s_efb.depthRead_texture, NULL);
		if (!FAILED(hr))
			break;
	}
		
	CHECK(hr, "Create depth read texture (hr=%#x)", hr);
	if (s_efb.depthRead_texture)
	{
		s_efb.depthRead_texture->GetSurfaceLevel(0, &s_efb.depth_ReadBuffer);
	}

	// Create an offscreen surface that we can lock to retrieve the data
	hr = D3D::dev->CreateOffscreenPlainSurface(4, 4, s_efb.depth_ReadBuffer_Format, D3DPOOL_SYSTEMMEM, &s_efb.depth_OffScreenReadBuffer, NULL);
	CHECK(hr, "Create depth offscreen surface (hr=%#x)", hr);
}

FramebufferManager::~FramebufferManager()
{
	SAFE_RELEASE(s_efb.depth_surface);
	SAFE_RELEASE(s_efb.color_surface);
	SAFE_RELEASE(s_efb.color_ReadBuffer);
	SAFE_RELEASE(s_efb.depth_ReadBuffer);
	SAFE_RELEASE(s_efb.color_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb.depth_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb.color_texture);
	SAFE_RELEASE(s_efb.colorRead_texture);
	SAFE_RELEASE(s_efb.depth_texture);
	SAFE_RELEASE(s_efb.depthRead_texture);
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	LPDIRECT3DTEXTURE9 tex;
	D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET,
		s_efb.color_surface_Format, D3DPOOL_DEFAULT, &tex, NULL);

	return new XFBSource(tex);
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc)
{
	const float scaleX = Renderer::GetXFBScaleX();
	const float scaleY = Renderer::GetXFBScaleY();

	TargetRectangle targetSource;

	targetSource.top = (int)(sourceRc.top *scaleY);
	targetSource.bottom = (int)(sourceRc.bottom *scaleY);
	targetSource.left = (int)(sourceRc.left *scaleX);
	targetSource.right = (int)(sourceRc.right * scaleX);

	*width = targetSource.right - targetSource.left;
	*height = targetSource.bottom - targetSource.top;
}

void XFBSource::Draw(const MathUtil::Rectangle<float> &sourcerc,
	const MathUtil::Rectangle<float> &drawrc, int width, int height) const
{
	D3D::drawShadedTexSubQuad(texture, &sourcerc, texWidth, texHeight, &drawrc, width , height,
		PixelShaderCache::GetColorCopyProgram(0), VertexShaderCache::GetSimpleVertexShader(0));
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, texture);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	u8* xfb_in_ram = Memory_GetPtr(xfbAddr);
	if (!xfb_in_ram)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(sourceRc);
	TextureConverter::EncodeToRamYUYV(GetEFBColorTexture(), targetRc, xfb_in_ram, fbWidth, fbHeight);
}

void XFBSource::CopyEFB()
{
	// Copy EFB data to XFB and restore render target again
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	texture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = texWidth;
	vp.Height = texHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	RECT sourcerect;
	sourcerect.bottom = sourceRc.bottom;
	sourcerect.left = sourceRc.left;
	sourcerect.right = sourceRc.right;
	sourcerect.top = sourceRc.top;

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	D3D::drawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(), 
		&sourcerect, 
		Renderer::GetFullTargetWidth(), 
		Renderer::GetFullTargetHeight(), 
		texWidth, 
		texHeight, 
		PixelShaderCache::GetColorCopyProgram( g_ActiveConfig.iMultisampleMode), 
		VertexShaderCache::GetSimpleVertexShader( g_ActiveConfig.iMultisampleMode));

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());

	Rendersurf->Release();
}
