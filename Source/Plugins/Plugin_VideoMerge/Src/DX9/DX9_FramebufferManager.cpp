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

#include "DX9_D3DBase.h"
#include "DX9_Render.h"
#include "DX9_FramebufferManager.h"
#include "VideoConfig.h"
#include "DX9_PixelShaderCache.h"
#include "DX9_VertexShaderCache.h"
#include "DX9_TextureConverter.h"

#include "../Main.h"

#undef CHECK
#define CHECK(hr, Message, ...) if (FAILED(hr)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

namespace DX9
{

XFBSource FramebufferManager::m_realXFBSource;

LPDIRECT3DTEXTURE9 FramebufferManager::s_efb_color_texture;
LPDIRECT3DTEXTURE9 FramebufferManager::s_efb_colorRead_texture;
LPDIRECT3DTEXTURE9 FramebufferManager::s_efb_depth_texture;
LPDIRECT3DTEXTURE9 FramebufferManager::s_efb_depthRead_texture;

LPDIRECT3DSURFACE9 FramebufferManager::s_efb_depth_surface;
LPDIRECT3DSURFACE9 FramebufferManager::s_efb_color_surface;
LPDIRECT3DSURFACE9 FramebufferManager::s_efb_color_ReadBuffer;
LPDIRECT3DSURFACE9 FramebufferManager::s_efb_depth_ReadBuffer;
LPDIRECT3DSURFACE9 FramebufferManager::s_efb_color_OffScreenReadBuffer;
LPDIRECT3DSURFACE9 FramebufferManager::s_efb_depth_OffScreenReadBuffer;

D3DFORMAT FramebufferManager::s_efb_color_surface_Format;
D3DFORMAT FramebufferManager::s_efb_depth_surface_Format;
D3DFORMAT FramebufferManager::s_efb_depth_ReadBuffer_Format;

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBColorRTSurface()
{
	return s_efb_color_surface;
}

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBDepthRTSurface()
{
	return s_efb_depth_surface;
}

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBColorOffScreenRTSurface()
{
	return s_efb_color_OffScreenReadBuffer;
}

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBDepthOffScreenRTSurface()
{
	return s_efb_depth_OffScreenReadBuffer;
}

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBColorReadSurface()
{
	return s_efb_color_ReadBuffer;
}

LPDIRECT3DSURFACE9 FramebufferManager::GetEFBDepthReadSurface()
{
	return s_efb_depth_ReadBuffer;
}

D3DFORMAT FramebufferManager::GetEFBDepthRTSurfaceFormat()
{
	return s_efb_depth_surface_Format;
}

D3DFORMAT FramebufferManager::GetEFBDepthReadSurfaceFormat()
{
	return s_efb_depth_ReadBuffer_Format;
}

D3DFORMAT FramebufferManager::GetEFBColorRTSurfaceFormat()
{
	return s_efb_color_surface_Format;
}

LPDIRECT3DTEXTURE9 FramebufferManager::GetEFBColorTexture(const EFBRectangle& sourceRc)
{
	return s_efb_color_texture;
}

LPDIRECT3DTEXTURE9 FramebufferManager::GetEFBDepthTexture(const EFBRectangle &sourceRc)
{
	return s_efb_depth_texture;
}

FramebufferManager::FramebufferManager()
{
	s_efb_color_texture = NULL;
	LPDIRECT3DTEXTURE9 s_efb_colorRead_texture = NULL;
	LPDIRECT3DTEXTURE9 s_efb_depth_texture = NULL;
	LPDIRECT3DTEXTURE9 s_efb_depthRead_texture = NULL;

	LPDIRECT3DSURFACE9 s_efb_depth_surface = NULL;
	LPDIRECT3DSURFACE9 s_efb_color_surface = NULL;
	LPDIRECT3DSURFACE9 s_efb_color_ReadBuffer = NULL;
	LPDIRECT3DSURFACE9 s_efb_depth_ReadBuffer = NULL;
	LPDIRECT3DSURFACE9 s_efb_color_OffScreenReadBuffer = NULL;
	LPDIRECT3DSURFACE9 s_efb_depth_OffScreenReadBuffer = NULL;

	D3DFORMAT s_efb_color_surface_Format = D3DFMT_FORCE_DWORD;
	D3DFORMAT s_efb_depth_surface_Format = D3DFMT_FORCE_DWORD;
	D3DFORMAT s_efb_depth_ReadBuffer_Format = D3DFMT_FORCE_DWORD;
	m_realXFBSource.texture = NULL;

	// Simplest possible setup to start with.
	int target_width = Renderer::GetFullTargetWidth();
	int target_height = Renderer::GetFullTargetHeight();

	s_efb_color_surface_Format = D3DFMT_A8R8G8B8;
	// Get the framebuffer texture
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format, 
										 D3DPOOL_DEFAULT, &s_efb_color_texture, NULL);
	if(s_efb_color_texture)
	{
		hr = s_efb_color_texture->GetSurfaceLevel(0, &s_efb_color_surface);
	}
	CHECK(hr, "Create color texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format, 
										  D3DPOOL_DEFAULT, &s_efb_colorRead_texture, NULL);
	CHECK(hr, "Create Color Read Texture (hr=%#x)", hr);
	if(s_efb_colorRead_texture)
	{
		s_efb_colorRead_texture->GetSurfaceLevel(0, &s_efb_color_ReadBuffer);
	}
	// Create an offscreen surface that we can lock to retrieve the data
	hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, s_efb_color_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb_color_OffScreenReadBuffer, NULL);
	CHECK(hr, "Create offscreen color surface (hr=%#x)", hr);

	// Select a Z-buffer format with hardware support
	D3DFORMAT *DepthTexFormats = new D3DFORMAT[5];
	DepthTexFormats[0] = FOURCC_INTZ;
	DepthTexFormats[1] = FOURCC_DF24;
	DepthTexFormats[2] = FOURCC_RAWZ;
	DepthTexFormats[3] = FOURCC_DF16;
	DepthTexFormats[4] = D3DFMT_D24X8;

	for(int i = 0; i < 5; i++)
	{
		s_efb_depth_surface_Format = DepthTexFormats[i];
		// Create the framebuffer depth texture
		hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb_depth_surface_Format, 
									 D3DPOOL_DEFAULT, &s_efb_depth_texture, NULL);
		if (!FAILED(hr))
			break;
	}
	CHECK(hr, "Framebuffer depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	// Get the Surface
	if(s_efb_depth_texture)
	{
		s_efb_depth_texture->GetSurfaceLevel(0, &s_efb_depth_surface);
	}
	// Create a 4x4 pixel texture to work as a buffer for peeking
	if(s_efb_depth_surface_Format == FOURCC_RAWZ || s_efb_depth_surface_Format == D3DFMT_D24X8)
	{
		DepthTexFormats[0] = D3DFMT_A8R8G8B8;
	}
	else
	{
		DepthTexFormats[0] = D3DFMT_R32F;
	}
	DepthTexFormats[1] = D3DFMT_A8R8G8B8;

	for(int i = 0; i < 2; i++)
	{
		s_efb_depth_ReadBuffer_Format = DepthTexFormats[i];
		// Get the framebuffer Depth texture
		hr = D3D::dev->CreateTexture(4, 4, 1, D3DUSAGE_RENDERTARGET, s_efb_depth_ReadBuffer_Format, 
									  D3DPOOL_DEFAULT, &s_efb_depthRead_texture, NULL);
		if (!FAILED(hr))
			break;
	}
		
	CHECK(hr, "Create depth read texture (hr=%#x)", hr);
	if(s_efb_depthRead_texture)
	{
		s_efb_depthRead_texture->GetSurfaceLevel(0, &s_efb_depth_ReadBuffer);
	}
	// Create an offscreen surface that we can lock to retrieve the data
	hr = D3D::dev->CreateOffscreenPlainSurface(4, 4, s_efb_depth_ReadBuffer_Format, D3DPOOL_SYSTEMMEM, &s_efb_depth_OffScreenReadBuffer, NULL);
	CHECK(hr, "Create depth offscreen surface (hr=%#x)", hr);
	delete [] DepthTexFormats;
}

FramebufferManager::~FramebufferManager()
{
	SAFE_RELEASE(s_efb_depth_surface);
	SAFE_RELEASE(s_efb_color_surface);
	SAFE_RELEASE(s_efb_color_ReadBuffer);
	SAFE_RELEASE(s_efb_depth_ReadBuffer);
	SAFE_RELEASE(s_efb_color_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb_depth_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb_color_texture);
	SAFE_RELEASE(s_efb_colorRead_texture);
	SAFE_RELEASE(s_efb_depth_texture);
	SAFE_RELEASE(s_efb_depthRead_texture);

	if (m_realXFBSource.texture)
		m_realXFBSource.texture->Release();
	m_realXFBSource.texture = NULL;
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	// TODO:

	//u8* xfb_in_ram = Memory_GetPtr(xfbAddr);
	//if (!xfb_in_ram)
	//{
	//	WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
	//	return;
	//}

	//TargetRectangle targetRc = Renderer::ConvertEFBRectangle(sourceRc);
	//TextureConverter::EncodeToRamYUYV(GetEFBColorTexture(sourceRc), targetRc, xfb_in_ram, fbWidth, fbHeight);
}

const XFBSource** FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	return NULL;

	// TODO:
	//xfbCount = 1;

	//m_realXFBSource.texWidth = fbWidth;
	//m_realXFBSource.texHeight = fbHeight;

	//m_realXFBSource.srcAddr = xfbAddr;
	//m_realXFBSource.srcWidth = fbWidth;
	//m_realXFBSource.srcHeight = fbHeight;

	//if (!m_realXFBSource.texture)
	//{
	//	D3D::dev->CreateTexture(fbWidth, fbHeight, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format, 
	//									 D3DPOOL_DEFAULT, &m_realXFBSource.texture, NULL);
	//}

	//// Decode YUYV data from GameCube RAM
	//TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, m_realXFBSource.texture);

	//m_overlappingXFBArray[0] = &m_realXFBSource;

	//return &m_overlappingXFBArray[0];
}

XFBSourceBase *FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	XFBSource* const xfbs = new XFBSource;
	D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format, 
		D3DPOOL_DEFAULT, &xfbs->texture, NULL);

	return xfbs;
}

XFBSource::~XFBSource()
{
	texture->Release();
}

void XFBSource::CopyEFB(const TargetRectangle& efbSource)
{

}

}
