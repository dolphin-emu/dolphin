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

#undef CHECK
#define CHECK(hr,Message) if (FAILED(hr)) { PanicAlert(__FUNCTION__ " FAIL: %s" ,Message); }

FramebufferManager FBManager;

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

D3DFORMAT FramebufferManager::GetEFBDepthRTSurfaceFormat(){return s_efb_depth_surface_Format;}
D3DFORMAT FramebufferManager::GetEFBDepthReadSurfaceFormat(){return s_efb_depth_ReadBuffer_Format;}
D3DFORMAT FramebufferManager::GetEFBColorRTSurfaceFormat(){return s_efb_color_surface_Format;}


LPDIRECT3DTEXTURE9 FramebufferManager::GetEFBColorTexture(const EFBRectangle& sourceRc)
{
	return s_efb_color_texture;
}


LPDIRECT3DTEXTURE9 FramebufferManager::GetEFBDepthTexture(const EFBRectangle &sourceRc)
{
	return s_efb_depth_texture;
}



void FramebufferManager::Create()
{	
	// Simplest possible setup to start with.
	int target_width = Renderer::GetFullTargetWidth();
	int target_height = Renderer::GetFullTargetHeight();

	s_efb_color_surface_Format = D3DFMT_A8R8G8B8;
	//get the framebuffer texture
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format,
		                                 D3DPOOL_DEFAULT, &s_efb_color_texture, NULL);
	if(s_efb_color_texture)
	{
		hr = s_efb_color_texture->GetSurfaceLevel(0,&s_efb_color_surface);
	}	
	CHECK(hr,"Create Color Texture");
	hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format,
		                                  D3DPOOL_DEFAULT, &s_efb_colorRead_texture, NULL);
	CHECK(hr,"Create Color Read Texture");
	if(s_efb_colorRead_texture)
	{
		s_efb_colorRead_texture->GetSurfaceLevel(0,&s_efb_color_ReadBuffer);
	}
	//create an offscreen surface that we can lock to retrieve the data
	hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, s_efb_color_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb_color_OffScreenReadBuffer, NULL );
	CHECK(hr,"Create Color offScreen Surface");	
	
	//Select Zbuffer format supported by hadware.
	if (g_ActiveConfig.bEFBAccessEnable)
	{		
		D3DFORMAT *DepthTexFormats = new D3DFORMAT[5];
		DepthTexFormats[0] = FOURCC_INTZ;		
		DepthTexFormats[1] = FOURCC_DF24;
		DepthTexFormats[2] = FOURCC_RAWZ;
		DepthTexFormats[3] = FOURCC_DF16;
		DepthTexFormats[4] = D3DFMT_D24X8;

		for(int i = 0;i<5;i++)
		{
			s_efb_depth_surface_Format = DepthTexFormats[i];
			//get the framebuffer Depth texture
			hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb_depth_surface_Format,
											 D3DPOOL_DEFAULT, &s_efb_depth_texture, NULL);
			if (!FAILED(hr)) break;
		}			
		CHECK(hr,"Depth Color Texture");
		//get the Surface
		if(s_efb_depth_texture)
		{
			s_efb_depth_texture->GetSurfaceLevel(0,&s_efb_depth_surface);
		}
		//create a 4x4 pixel texture to work as a buffer for peeking
		if(s_efb_depth_surface_Format == FOURCC_RAWZ || s_efb_depth_surface_Format == D3DFMT_D24X8)
		{
			DepthTexFormats[0] = D3DFMT_A8R8G8B8;		
		}
		else
		{
			DepthTexFormats[0] = D3DFMT_R32F;
		}
		DepthTexFormats[1] = D3DFMT_A8R8G8B8;		

		for(int i = 0;i<2;i++)
		{
			s_efb_depth_ReadBuffer_Format = DepthTexFormats[i];
			//get the framebuffer Depth texture
			hr = D3D::dev->CreateTexture(4, 4, 1, D3DUSAGE_RENDERTARGET, s_efb_depth_ReadBuffer_Format,
											  D3DPOOL_DEFAULT, &s_efb_depthRead_texture, NULL);
			if (!FAILED(hr)) break;
		}
		
		CHECK(hr,"Create Depth Read texture");
		if(s_efb_depthRead_texture)
		{
			s_efb_depthRead_texture->GetSurfaceLevel(0,&s_efb_depth_ReadBuffer);
		}
		//create an offscreen surface that we can lock to retrieve the data
		hr = D3D::dev->CreateOffscreenPlainSurface(4, 4, s_efb_depth_ReadBuffer_Format, D3DPOOL_SYSTEMMEM, &s_efb_depth_OffScreenReadBuffer, NULL );
		CHECK(hr,"Create Depth offScreen Surface");
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

void FramebufferManager::Destroy()
{	
	if (s_efb_depth_surface)
		s_efb_depth_surface->Release();
	s_efb_depth_surface=NULL;

	if (s_efb_color_surface)
		s_efb_color_surface->Release();
	s_efb_color_surface=NULL;

	if (s_efb_color_ReadBuffer)
		s_efb_color_ReadBuffer->Release();
	s_efb_color_ReadBuffer=NULL;

	if (s_efb_depth_ReadBuffer)
		s_efb_depth_ReadBuffer->Release();
	s_efb_depth_ReadBuffer=NULL;

	if (s_efb_color_OffScreenReadBuffer)
		s_efb_color_OffScreenReadBuffer->Release();
	s_efb_color_OffScreenReadBuffer=NULL;

	if (s_efb_depth_OffScreenReadBuffer)
		s_efb_depth_OffScreenReadBuffer->Release();
	s_efb_depth_OffScreenReadBuffer=NULL;

	if (s_efb_color_texture)
		s_efb_color_texture->Release();
	s_efb_color_texture=NULL;

	if (s_efb_colorRead_texture)
		s_efb_colorRead_texture->Release();
	s_efb_colorRead_texture=NULL;

	if (s_efb_depth_texture)
		s_efb_depth_texture->Release();
	s_efb_depth_texture=NULL;

	if (s_efb_depthRead_texture)
		s_efb_depthRead_texture->Release();
	s_efb_depthRead_texture=NULL;

	for (VirtualXFBListType::iterator it = m_virtualXFBList.begin(); it != m_virtualXFBList.end(); ++it)
	{
		if(it->xfbSource.texture)
			it->xfbSource.texture->Release();
	}
	m_virtualXFBList.clear();
	if(m_realXFBSource.texture)
		m_realXFBSource.texture->Release();		
	m_realXFBSource.texture = NULL;

}

void FramebufferManager::CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if (g_ActiveConfig.bUseRealXFB)
		copyToRealXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
	else
		copyToVirtualXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
}

const XFBSource** FramebufferManager::GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	if (g_ActiveConfig.bUseRealXFB)
		return getRealXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	else
		return getVirtualXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
}

FramebufferManager::VirtualXFBListType::iterator FramebufferManager::findVirtualXFB(u32 xfbAddr, u32 width, u32 height)
{
	u32 srcLower = xfbAddr;
	u32 srcUpper = xfbAddr + 2 * width * height;

	VirtualXFBListType::iterator it;
	for (it = m_virtualXFBList.begin(); it != m_virtualXFBList.end(); ++it)
	{
		u32 dstLower = it->xfbAddr;
		u32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
			return it;
	}

	// That address is not in the Virtual XFB list.
	return m_virtualXFBList.end();
}

void FramebufferManager::replaceVirtualXFB()
{
	VirtualXFBListType::iterator it = m_virtualXFBList.begin();	

	s32 srcLower = it->xfbAddr;
	s32 srcUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;
	s32 lineSize = 2 * it->xfbWidth;

	it++;

	while (it != m_virtualXFBList.end())
	{
		s32 dstLower = it->xfbAddr;
		s32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
		{
			// invalidate the data
			it->xfbAddr = 0;
			it->xfbHeight = 0;
			it->xfbWidth = 0;
		}
		else if (addrRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{		
			s32 upperOverlap = (srcUpper - dstLower) / lineSize;
			s32 lowerOverlap = (dstUpper - srcLower) / lineSize;

			if (upperOverlap > 0 && lowerOverlap < 0)
			{
				it->xfbAddr += lineSize * upperOverlap;
				it->xfbHeight -= upperOverlap;
			}
			else if (lowerOverlap > 0)
			{
				it->xfbHeight -= lowerOverlap;
			}
		}

		it++;
	}
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	u8* xfb_in_ram = Memory_GetPtr(xfbAddr);
	if (!xfb_in_ram)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(sourceRc);
	TextureConverter::EncodeToRamYUYV(GetEFBColorTexture(sourceRc), targetRc, xfb_in_ram, fbWidth, fbHeight);
}

void FramebufferManager::copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	LPDIRECT3DTEXTURE9 xfbTexture;
	HRESULT hr = 0;


	VirtualXFBListType::iterator it = findVirtualXFB(xfbAddr, fbWidth, fbHeight);

	if (it == m_virtualXFBList.end() && (int)m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
	{
		// replace the last virtual XFB
		it--;
	}

	float MultiSampleCompensation = 1.0f;
	if(g_ActiveConfig.iMultisampleMode > 0 && g_ActiveConfig.iMultisampleMode < 4)
	{
		switch (g_ActiveConfig.iMultisampleMode)
		{
			case 1:
				MultiSampleCompensation = 2.0f/3.0f;				
				break;
			case 2:
				MultiSampleCompensation = 0.5f;
				break;
			case 3:
				MultiSampleCompensation = 1.0f/3.0f;
				break;
			default:
				break;
		};
	}

	float scaleX = Renderer::GetTargetScaleX() * MultiSampleCompensation ;
	float scaleY = Renderer::GetTargetScaleY() * MultiSampleCompensation;
	TargetRectangle targetSource,efbSource;
	efbSource = Renderer::ConvertEFBRectangle(sourceRc);
	targetSource.top = (sourceRc.top *scaleY);
	targetSource.bottom = (sourceRc.bottom *scaleY);
	targetSource.left = (sourceRc.left *scaleX);
	targetSource.right = (sourceRc.right * scaleX);
	int target_width = targetSource.right - targetSource.left;
	int target_height = targetSource.bottom - targetSource.top;
	if (it != m_virtualXFBList.end())
	{
		// Overwrite an existing Virtual XFB.

		it->xfbAddr = xfbAddr;
		it->xfbWidth = fbWidth;
		it->xfbHeight = fbHeight;

		it->xfbSource.srcAddr = xfbAddr;
		it->xfbSource.srcWidth = fbWidth;
		it->xfbSource.srcHeight = fbHeight;		

		if(it->xfbSource.texWidth != target_width || it->xfbSource.texHeight != target_height || !(it->xfbSource.texture))
		{
			if(it->xfbSource.texture)
				it->xfbSource.texture->Release();
			it->xfbSource.texture = NULL;
			hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format,
		                                 D3DPOOL_DEFAULT, &(it->xfbSource.texture), NULL);

		}

		xfbTexture = it->xfbSource.texture;

		it->xfbSource.texWidth = target_width;
		it->xfbSource.texHeight = target_height;

		// Move this Virtual XFB to the front of the list.
		m_virtualXFBList.splice(m_virtualXFBList.begin(), m_virtualXFBList, it);

		// Keep stale XFB data from being used
		replaceVirtualXFB();
	}
	else
	{
		// Create a new Virtual XFB and place it at the front of the list.

		D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format,
		                                 D3DPOOL_DEFAULT, &xfbTexture, NULL);
		VirtualXFB newVirt;

		newVirt.xfbAddr = xfbAddr;
		newVirt.xfbWidth = fbWidth;
		newVirt.xfbHeight = fbHeight;

		newVirt.xfbSource.texture = xfbTexture;
		newVirt.xfbSource.texWidth = target_width;
		newVirt.xfbSource.texHeight = target_height;		

		// Add the new Virtual XFB to the list

		if ((int)m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
		{
			// List overflowed; delete the oldest.
			m_virtualXFBList.back().xfbSource.texture->Release();
			m_virtualXFBList.pop_back();
		}

		m_virtualXFBList.push_front(newVirt);
	}

	// Copy EFB to XFB texture

	if(!xfbTexture)
		return;
	LPDIRECT3DTEXTURE9 read_texture = GetEFBColorTexture(sourceRc);
    
	Renderer::ResetAPIState(); // reset any game specific settings
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	
	xfbTexture->GetSurfaceLevel(0,&Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);		
    
	D3DVIEWPORT9 vp;

	vp.X = 0;
	vp.Y = 0;
	vp.Width  = target_width;
	vp.Height = target_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT sourcerect;
	sourcerect.bottom = efbSource.bottom;
	sourcerect.left = efbSource.left;
	sourcerect.right = efbSource.right;
	sourcerect.top = efbSource.top;

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		

	int SSAAMode = ( g_ActiveConfig.iMultisampleMode > 3 )? 0 : g_ActiveConfig.iMultisampleMode;
	D3D::drawShadedTexQuad(
		read_texture,
		&sourcerect, 
		Renderer::GetFullTargetWidth() , 
		Renderer::GetFullTargetHeight(),
		PixelShaderCache::GetColorCopyProgram(SSAAMode),
		(SSAAMode != 0)? VertexShaderCache::GetFSAAVertexShader() : VertexShaderCache::GetSimpleVertexShader());			
	
	
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0,NULL);
	D3D::dev->SetRenderTarget(0, GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(GetEFBDepthRTSurface());	
	Renderer::RestoreAPIState();
	Rendersurf->Release();		
	
}

const XFBSource** FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	xfbCount = 1;

	m_realXFBSource.texWidth = fbWidth;
	m_realXFBSource.texHeight = fbHeight;

	m_realXFBSource.srcAddr = xfbAddr;
	m_realXFBSource.srcWidth = fbWidth;
	m_realXFBSource.srcHeight = fbHeight;	

	if (!m_realXFBSource.texture)
	{
		D3D::dev->CreateTexture(fbWidth, fbHeight, 1, D3DUSAGE_RENDERTARGET, s_efb_color_surface_Format,
		                                 D3DPOOL_DEFAULT, &m_realXFBSource.texture, NULL);
	}

	// Decode YUYV data from GameCube RAM
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, m_realXFBSource.texture);

	m_overlappingXFBArray[0] = &m_realXFBSource;

	return &m_overlappingXFBArray[0];
}

const XFBSource** FramebufferManager::getVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	xfbCount = 0;

	if (m_virtualXFBList.size() == 0)
	{
		// No Virtual XFBs available.
		return NULL;
	}

	u32 srcLower = xfbAddr;
	u32 srcUpper = xfbAddr + 2 * fbWidth * fbHeight;

	VirtualXFBListType::iterator it;
	for (it = m_virtualXFBList.end(); it != m_virtualXFBList.begin();)
	{
		--it;

		u32 dstLower = it->xfbAddr;
		u32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (addrRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{
			m_overlappingXFBArray[xfbCount] = &(it->xfbSource);
			xfbCount++;
		}
	}

	return &m_overlappingXFBArray[0];
}