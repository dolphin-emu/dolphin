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

namespace FBManager
{

static LPDIRECT3DTEXTURE9 s_efb_color_texture;
static LPDIRECT3DSURFACE9 s_efb_color_surface;
static LPDIRECT3DSURFACE9 s_efb_depth_surface;
static D3DFORMAT s_efb_color_surface_Format;
static D3DFORMAT s_efb_depth_surface_Format;
#undef CHECK
#define CHECK(hr) if (FAILED(hr)) { PanicAlert("FAIL: " __FUNCTION__); }

LPDIRECT3DSURFACE9 GetEFBColorRTSurface() { return s_efb_color_surface; }
LPDIRECT3DSURFACE9 GetEFBDepthRTSurface() { return s_efb_depth_surface; }
D3DFORMAT GetEFBDepthRTSurfaceFormat(){return s_efb_depth_surface_Format;}
D3DFORMAT GetEFBColorRTSurfaceFormat(){return s_efb_color_surface_Format;}

LPDIRECT3DTEXTURE9 GetEFBColorTexture(const EFBRectangle& sourceRc)
{
	return s_efb_color_texture;
}

LPDIRECT3DTEXTURE9 GetEFBDepthTexture(const EFBRectangle &sourceRc)
{
	// Depth textures not supported under DX9. We're gonna fake this
	// with a secondary render target later.
	return NULL;
}




void Create()
{
	// Simplest possible setup to start with.
	int target_width = Renderer::GetTargetWidth();
	int target_height = Renderer::GetTargetHeight();
	s_efb_color_surface_Format = D3DFMT_A8R8G8B8;
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &s_efb_color_texture, NULL);
	CHECK(hr);

  	hr = s_efb_color_texture->GetSurfaceLevel(0, &s_efb_color_surface);
	CHECK(hr);

	//Select Zbuffer format supported by hadware.
	s_efb_depth_surface_Format = D3DFMT_D32F_LOCKABLE;
	hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, D3DFMT_D32F_LOCKABLE,
		                                     D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb_depth_surface, NULL);
	if (FAILED(hr))
	{
		s_efb_depth_surface_Format = D3DFMT_D16_LOCKABLE;
		hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, D3DFMT_D16_LOCKABLE,
		                                     D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb_depth_surface, NULL);
		if(FAILED(hr))
		{
			s_efb_depth_surface_Format = D3DFMT_D24S8;
			hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, D3DFMT_D24S8,
		                                     D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb_depth_surface, NULL);
		}
	}
	CHECK(hr);
}

void Destroy()
{
	if(s_efb_depth_surface)
		s_efb_depth_surface->Release();
	s_efb_depth_surface = NULL;
	
	if(s_efb_color_surface)
		s_efb_color_surface->Release();
	s_efb_color_surface = NULL;
	
	if(s_efb_color_texture)
		s_efb_color_texture->Release();
	s_efb_color_texture = NULL;
}

}  // namespace