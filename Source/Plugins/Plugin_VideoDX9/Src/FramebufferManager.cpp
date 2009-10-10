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

namespace FBManager
{

static LPDIRECT3DTEXTURE9 s_efb_color_texture;
static LPDIRECT3DTEXTURE9 s_efb_colorBuffer_texture;
static LPDIRECT3DTEXTURE9 s_efb_depth_texture;
static LPDIRECT3DTEXTURE9 s_efb_depthBuffer_texture;
static LPDIRECT3DSURFACE9 s_efb_color_surface;
static LPDIRECT3DSURFACE9 s_efb_depth_surface;

static LPDIRECT3DSURFACE9 s_efb_color_ReadBuffer;
static LPDIRECT3DSURFACE9 s_efb_depth_ReadBuffer;

static LPDIRECT3DSURFACE9 s_efb_color_OffScreenReadBuffer;
static LPDIRECT3DSURFACE9 s_efb_depth_OffScreenReadBuffer;


static D3DFORMAT s_efb_color_surface_Format;
static D3DFORMAT s_efb_depth_surface_Format;
#undef CHECK
#define CHECK(hr,Message) if (FAILED(hr)) { PanicAlert(__FUNCTION__ " FAIL: %s" ,Message); }



LPDIRECT3DSURFACE9 GetEFBColorRTSurface() { return s_efb_color_surface; }
LPDIRECT3DSURFACE9 GetEFBDepthRTSurface() { return s_efb_depth_surface; }
LPDIRECT3DSURFACE9 GetEFBColorOffScreenRTSurface() { return s_efb_color_OffScreenReadBuffer; }
LPDIRECT3DSURFACE9 GetEFBDepthOffScreenRTSurface() { return s_efb_depth_OffScreenReadBuffer; }

LPDIRECT3DSURFACE9 GetEFBColorReadSurface() { return s_efb_color_ReadBuffer; }
LPDIRECT3DSURFACE9 GetEFBDepthReadSurface() { return s_efb_depth_ReadBuffer; }

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
	return s_efb_depth_texture;
}




void Create()
{
	
	// Simplest possible setup to start with.
	int target_width = Renderer::GetTargetWidth();
	int target_height = Renderer::GetTargetHeight();
	s_efb_color_surface_Format = D3DFMT_A8R8G8B8;
	//get the framebuffer texture
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &s_efb_color_texture, NULL);
	CHECK(hr,"Create Color Texture");
	//get the Surface
  	hr = s_efb_color_texture->GetSurfaceLevel(0, &s_efb_color_surface);
	CHECK(hr,"Get Color Surface");
	//create a one pixel texture to work as a buffer for peeking
	hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &s_efb_colorBuffer_texture, NULL);
	if (!FAILED(hr))
	{
		//get the surface for the peeking texture
		hr = s_efb_colorBuffer_texture->GetSurfaceLevel(0, &s_efb_color_ReadBuffer);
		CHECK(hr,"Get Color Pixel Surface");
		//create an offscreen surface that we can lock to retrieve the data
		hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s_efb_color_OffScreenReadBuffer, NULL );
		CHECK(hr,"Create Color offScreen Surface");
	}
	
	//Select Zbuffer format supported by hadware.
	if (g_ActiveConfig.bEFBAccessEnable)
	{
		//depth format in prefered order
		D3DFORMAT *DepthTexFormats = new D3DFORMAT[7];
		DepthTexFormats[0] = (D3DFORMAT)MAKEFOURCC('D','F','2','4');
		DepthTexFormats[1] = (D3DFORMAT)MAKEFOURCC('I','N','T','Z');
		DepthTexFormats[2] = (D3DFORMAT)MAKEFOURCC('R','A','W','Z');
		DepthTexFormats[3] = (D3DFORMAT)MAKEFOURCC('D','F','1','6');
		DepthTexFormats[4] = D3DFMT_D32F_LOCKABLE;		
		DepthTexFormats[5] = D3DFMT_D16_LOCKABLE;
		DepthTexFormats[6] = D3DFMT_D24X8;

		for(int i = 0;i<4;i++)
		{
			s_efb_depth_surface_Format = DepthTexFormats[i];
			hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb_depth_surface_Format,
		                                 D3DPOOL_DEFAULT, &s_efb_depth_texture, NULL);
			if (!FAILED(hr)) 
				break;
			
		}
		CHECK(hr,"Create Depth Texture");
		if (!FAILED(hr))
		{
			//we found a dept texture suported by hardware so  get the surface to draw to
			hr = s_efb_depth_texture->GetSurfaceLevel(0, &s_efb_depth_surface);
			CHECK(hr,"Get Depth Surface");
			//create a buffer texture for peeking
			hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_DEPTHSTENCIL, s_efb_depth_surface_Format,
		                                 D3DPOOL_DEFAULT, &s_efb_depthBuffer_texture, NULL);
			CHECK(hr,"Create Depth Pixel Texture");
			if (!FAILED(hr))
			{
				//texture create correctly so get the surface
				hr = s_efb_depthBuffer_texture->GetSurfaceLevel(0, &s_efb_depth_ReadBuffer);
				CHECK(hr,"Get Depth Pixel Surface");
				// create an ofscren surface to grab the data
				hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, s_efb_depth_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb_depth_OffScreenReadBuffer, NULL );
				CHECK(hr,"Create Depth offscreen Surface");
				if (FAILED(hr))
				{
					//no depth in system mem so try vista path to grab depth data
					//create a offscreen lockeable surface
					hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, D3DFMT_D32F_LOCKABLE, D3DPOOL_DEFAULT, &s_efb_depth_OffScreenReadBuffer, NULL );
					CHECK(hr,"Create Depth D3DFMT_D32F_LOCKABLE offscreen Surface");
					if(s_efb_depth_ReadBuffer)
						s_efb_depth_ReadBuffer->Release();
					//this is ugly but is a fast way to test wich path to proceed for peeking
					s_efb_depth_ReadBuffer = s_efb_depth_OffScreenReadBuffer;
					s_efb_depth_surface_Format = D3DFMT_D32F_LOCKABLE;
				}
			}
												
		}
		if (!FAILED(hr))
		{
			//so far so god, texture depth works so return
			delete [] DepthTexFormats;
			return;			
		}
		else
		{
			//no depth texture... cleanup
			if(s_efb_depth_ReadBuffer)
				s_efb_depth_ReadBuffer->Release();
			s_efb_depth_ReadBuffer = NULL;
			
			if(s_efb_depth_OffScreenReadBuffer)
				s_efb_depth_OffScreenReadBuffer->Release();

			if(s_efb_depth_surface)
				s_efb_depth_surface->Release();
			s_efb_depth_surface = NULL;
			
			if(s_efb_depthBuffer_texture)
				s_efb_depthBuffer_texture->Release();
			s_efb_depthBuffer_texture=NULL;

			if(s_efb_depth_texture)
				s_efb_depth_texture->Release();
			s_efb_depth_texture = NULL;
		}
		// no depth textures... try to create an lockable depth surface
		for(int i = 4;i<7;i++)
		{
			s_efb_depth_surface_Format = DepthTexFormats[i];
			hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, s_efb_depth_surface_Format,
											 D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb_depth_surface, NULL);
			if (!FAILED(hr)) break;
		}		
		s_efb_depth_ReadBuffer = s_efb_depth_surface;
		CHECK(hr,"CreateDepthStencilSurface");
		delete [] DepthTexFormats;
	}
	else
	{
		s_efb_depth_surface_Format = D3DFMT_D24X8;
		hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, s_efb_depth_surface_Format,
											 D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb_depth_surface, NULL);
		CHECK(hr,"CreateDepthStencilSurface");
	}	
}

void Destroy()
{
	if(s_efb_depth_ReadBuffer)
		s_efb_depth_ReadBuffer->Release();
	s_efb_depth_ReadBuffer = NULL;
	
	if(s_efb_depth_OffScreenReadBuffer)
		s_efb_depth_OffScreenReadBuffer->Release();

	if(s_efb_depth_surface)
		s_efb_depth_surface->Release();
	s_efb_depth_surface = NULL;
	
	if(s_efb_depthBuffer_texture)
		s_efb_depthBuffer_texture->Release();
	s_efb_depthBuffer_texture=NULL;

	if(s_efb_depth_texture)
		s_efb_depth_texture->Release();
	s_efb_depth_texture = NULL;

	if(s_efb_color_OffScreenReadBuffer )
		s_efb_color_OffScreenReadBuffer->Release();
	s_efb_color_OffScreenReadBuffer  = NULL;
	
	if(s_efb_color_ReadBuffer )
		s_efb_color_ReadBuffer->Release();
	s_efb_color_ReadBuffer  = NULL;

	if(s_efb_color_surface)
		s_efb_color_surface->Release();
	s_efb_color_surface = NULL;	
	
	if(s_efb_colorBuffer_texture)
		s_efb_colorBuffer_texture->Release();
	s_efb_colorBuffer_texture = NULL;
	
	if(s_efb_color_texture)
		s_efb_color_texture->Release();
	s_efb_color_texture = NULL;
}

}  // namespace