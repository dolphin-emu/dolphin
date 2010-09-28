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
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "Render.h"
#include "FramebufferManager.h"
#include "VideoConfig.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"

FramebufferManager g_framebufferManager;

D3DTexture2D* &FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
ID3D11Texture2D* &FramebufferManager::GetEFBColorStagingBuffer() { return m_efb.color_staging_buf; }

D3DTexture2D* &FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }
D3DTexture2D* &FramebufferManager::GetEFBDepthReadTexture() { return m_efb.depth_read_texture; }
ID3D11Texture2D* &FramebufferManager::GetEFBDepthStagingBuffer() { return m_efb.depth_staging_buf; }

void FramebufferManager::Create()
{
	unsigned int target_width = Renderer::GetFullTargetWidth();
	unsigned int target_height = Renderer::GetFullTargetHeight();
	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	// create framebuffer color texture
	m_efb.color_tex = D3DTexture2D::Create(target_width, target_height, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE), D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
	CHECK(m_efb.color_tex!=NULL, "create EFB color texture (size: %dx%d)", target_width, target_height);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetRTV(), "EFB color texture render target view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");

	// create a staging texture for Renderer::AccessEFB
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_WRITE|D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &m_efb.color_staging_buf);
	CHECK(hr==S_OK, "create EFB color staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_staging_buf, "EFB color staging texture (used for Renderer::AccessEFB)");

	// EFB depth buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width, target_height, 1, 1, D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	m_efb.depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetSRV(), "EFB depth texture shader resource view");

	// render target for depth buffer access in Renderer::AccessEFB
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 4, 4, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB depth read texture (hr=%#x)", hr);
	m_efb.depth_read_texture = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_read_texture->GetTex(), "EFB depth read texture (used in Renderer::AccessEFB)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_read_texture->GetRTV(), "EFB depth read texture render target view (used in Renderer::AccessEFB)");

	// staging texture to which we copy the data from m_efb.depth_read_texture
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 4, 4, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &m_efb.depth_staging_buf);
	CHECK(hr==S_OK, "create EFB depth staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_staging_buf, "EFB depth staging texture (used for Renderer::AccessEFB)");
}

void FramebufferManager::Destroy()
{
	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_staging_buf);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_staging_buf);
	SAFE_RELEASE(m_efb.depth_read_texture);

	for (VirtualXFBListType::iterator it = m_virtualXFBList.begin(); it != m_virtualXFBList.end(); ++it)
		SAFE_RELEASE(it->xfbSource.tex);

	m_virtualXFBList.clear();
	SAFE_RELEASE(m_realXFBSource.tex);
}

void FramebufferManager::CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	copyToVirtualXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
}

const XFBSource** FramebufferManager::GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
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

	++it;

	while (it != m_virtualXFBList.end())
	{
		s32 dstLower = it->xfbAddr;
		s32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
		{
			// Invalidate the data
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

		++it;
	}
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	// TODO
	PanicAlert("copyToRealXFB not implemented, yet\n");
}

void FramebufferManager::copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	D3DTexture2D* xfbTex;
	HRESULT hr = 0;

	VirtualXFBListType::iterator it = findVirtualXFB(xfbAddr, fbWidth, fbHeight);

	if (it == m_virtualXFBList.end() && (int)m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
	{
		PanicAlert("Requested creating a new virtual XFB although the maximum number has already been reached! Report this to the devs");
		return;
	}

	float scaleX = Renderer::GetTargetScaleX();
	float scaleY = Renderer::GetTargetScaleY();
	TargetRectangle targetSource,efbSource;
	efbSource = Renderer::ConvertEFBRectangle(sourceRc);
	targetSource.top = (int)(sourceRc.top * scaleY);
	targetSource.bottom = (int)(sourceRc.bottom * scaleY);
	targetSource.left = (int)(sourceRc.left * scaleX);
	targetSource.right = (int)(sourceRc.right * scaleX);
	unsigned int target_width = targetSource.right - targetSource.left;
	unsigned int target_height = targetSource.bottom - targetSource.top;
	if (it != m_virtualXFBList.end())
	{
		// Overwrite an existing Virtual XFB.

		it->xfbAddr = xfbAddr;
		it->xfbWidth = fbWidth;
		it->xfbHeight = fbHeight;

		it->xfbSource.srcAddr = xfbAddr;
		it->xfbSource.srcWidth = fbWidth;
		it->xfbSource.srcHeight = fbHeight;

		if (it->xfbSource.texWidth != target_width || it->xfbSource.texHeight != target_height || !(it->xfbSource.tex))
		{
			SAFE_RELEASE(it->xfbSource.tex);
			it->xfbSource.tex = D3DTexture2D::Create(target_width, target_height, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE), D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
			if (it->xfbSource.tex == NULL) PanicAlert("Failed to create XFB texture\n");
		}
		xfbTex = it->xfbSource.tex;

		it->xfbSource.texWidth = target_width;
		it->xfbSource.texHeight = target_height;

		// move this Virtual XFB to the front of the list.
		m_virtualXFBList.splice(m_virtualXFBList.begin(), m_virtualXFBList, it);

		// keep stale XFB data from being used
		replaceVirtualXFB();
	}
	else
	{
		// Create a new Virtual XFB and place it at the front of the list.
		VirtualXFB newVirt;

		newVirt.xfbSource.tex = D3DTexture2D::Create(target_width, target_height, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE), D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
		if (newVirt.xfbSource.tex == NULL) PanicAlert("Failed to create a new virtual XFB");

		newVirt.xfbAddr = xfbAddr;
		newVirt.xfbWidth = fbWidth;
		newVirt.xfbHeight = fbHeight;

		xfbTex = newVirt.xfbSource.tex;
		newVirt.xfbSource.texWidth = target_width;
		newVirt.xfbSource.texHeight = target_height;

		// Add the new Virtual XFB to the list
		if (m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
		{
			PanicAlert("Requested creating a new virtual XFB although the maximum number has already been reached! Report this to the devs");
			SAFE_RELEASE(newVirt.xfbSource.tex);
			return;
			// TODO, possible alternative to failing: just delete the oldest virtual XFB:
			// m_virtualXFBList.back().xfbSource.tex->Release();
			// m_virtualXFBList.pop_back();
		}
		m_virtualXFBList.push_front(newVirt);
	}
	if (!xfbTex->GetRTV()) return;
    
	Renderer::ResetAPIState(); // reset any game specific settings

	// Copy EFB data to XFB and restore render target again
	D3D11_RECT sourcerect = CD3D11_RECT(efbSource.left, efbSource.top, efbSource.right, efbSource.bottom);
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)target_width, (float)target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &xfbTex->GetRTV(), NULL);
	D3D::SetLinearCopySampler();
	D3D::drawShadedTexQuad(GetEFBColorTexture()->GetSRV(), &sourcerect,
							Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(),
							PixelShaderCache::GetColorCopyProgram(), VertexShaderCache::GetSimpleVertexShader(),
							VertexShaderCache::GetSimpleInputLayout());
	D3D::context->OMSetRenderTargets(1, &GetEFBColorTexture()->GetRTV(), GetEFBDepthTexture()->GetDSV());
	Renderer::RestoreAPIState();
}

const XFBSource** FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	PanicAlert("getRealXFBSource not implemented, yet\n");
	return NULL;
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
