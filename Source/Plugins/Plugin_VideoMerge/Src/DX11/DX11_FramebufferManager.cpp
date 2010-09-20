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

// VideoCommon
#include "VideoConfig.h"

// DX11
#include "DX11_Render.h"
#include "DX11_D3DBase.h"
#include "DX11_D3DTexture.h"
#include "DX11_D3DUtil.h"
#include "DX11_FramebufferManager.h"
#include "DX11_PixelShaderCache.h"
#include "DX11_VertexShaderCache.h"

#include "../Main.h"

namespace DX11
{

XFBSource FramebufferManager::m_realXFBSource;           // used in real XFB mode

FramebufferManager::EFB FramebufferManager::m_efb;

D3DTexture2D* &FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
ID3D11Texture2D* &FramebufferManager::GetEFBColorStagingBuffer() { return m_efb.color_staging_buf; }

D3DTexture2D* &FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }
D3DTexture2D* &FramebufferManager::GetEFBDepthReadTexture() { return m_efb.depth_read_texture; }
ID3D11Texture2D* &FramebufferManager::GetEFBDepthStagingBuffer() { return m_efb.depth_staging_buf; }

FramebufferManager::FramebufferManager()
{
	m_efb.color_tex = NULL;
	m_efb.color_staging_buf = NULL;
	m_efb.depth_tex = NULL;
	m_efb.depth_staging_buf = NULL;
	m_efb.depth_read_texture = NULL;

	m_realXFBSource.tex = NULL;

	unsigned int target_width = Renderer::GetFullTargetWidth();
	unsigned int target_height = Renderer::GetFullTargetHeight();
	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	// create framebuffer color texture
	m_efb.color_tex = D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
	CHECK(m_efb.color_tex, "create EFB color texture (size: %dx%d)", target_width, target_height);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetRTV(), "EFB color texture render target view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");

	// create a staging texture for Renderer::AccessEFB
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 0,
		D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_WRITE|D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &m_efb.color_staging_buf);
	CHECK(SUCCEEDED(hr), "create EFB color staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_staging_buf, "EFB color staging texture (used for Renderer::AccessEFB)");

	// EFB depth buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width,
		target_height, 1, 1, D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	m_efb.depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE),
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT);
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

FramebufferManager::~FramebufferManager()
{
	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_staging_buf);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_staging_buf);
	SAFE_RELEASE(m_efb.depth_read_texture);

	SAFE_RELEASE(m_realXFBSource.tex);
}

void XFBSource::CopyEFB(const TargetRectangle& efbSource)
{
	// copy EFB data to XFB and restore render target again
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)texWidth, (float)texHeight);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &tex->GetRTV(), NULL);
	D3D::SetLinearCopySampler();

	D3DTexture2D* const ctex = FramebufferManager::GetEFBColorTexture();

	D3D::drawShadedTexQuad(ctex->GetSRV(), efbSource.AsRECT(),
		Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(),
		PixelShaderCache::GetColorCopyProgram(), VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout());

	D3D::context->OMSetRenderTargets(1, &ctex->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

XFBSource::~XFBSource()
{
	tex->Release();
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	XFBSource* const xfbs = new XFBSource;
	xfbs->tex = D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);

	return xfbs;
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	// TODO
	PanicAlert("copyToRealXFB not implemented, yet\n");
}

const XFBSource** FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	PanicAlert("getRealXFBSource not implemented, yet\n");
	return NULL;
}

}
