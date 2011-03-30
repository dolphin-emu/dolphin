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

#include "VideoConfig.h"

#include "D3DBase.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "Render.h"
#include "VertexShaderCache.h"
#include "HW/Memmap.h"

namespace DX11
{

FramebufferManager::Efb FramebufferManager::m_efb;

D3DTexture2D* FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisampleMode)
	{
		D3D::g_context->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), 0, m_efb.color_tex->GetTex(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		return m_efb.resolved_color_tex.get();
	}
	else
		return m_efb.color_tex.get();
}

D3DTexture2D* FramebufferManager::GetResolvedEFBDepthTexture()
{
	if (g_ActiveConfig.iMultisampleMode)
	{
		D3D::g_context->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), 0, m_efb.color_tex->GetTex(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		return m_efb.resolved_color_tex.get();
	}
	else
		return m_efb.depth_tex.get();
}

FramebufferManager::FramebufferManager()
{
	const unsigned int target_width = Renderer::GetFullTargetWidth();
	const unsigned int target_height = Renderer::GetFullTargetHeight();
	DXGI_SAMPLE_DESC sample_desc = D3D::GetAAMode(g_ActiveConfig.iMultisampleMode);

	// EFB color texture - primary render target
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height,
		1, 1, D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	auto const buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(buf, "create EFB color texture (size: %dx%d)", target_width, target_height);
	m_efb.color_tex.reset(new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1)));
	CHECK(m_efb.color_tex!=NULL, "create EFB color texture (size: %dx%d)", target_width, target_height);
	D3D::SetDebugObjectName(m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName(m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");
	D3D::SetDebugObjectName(m_efb.color_tex->GetRTV(), "EFB color texture render target view");
	}

	// Temporary EFB color texture - used in ReinterpretPixelData
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height,
		1, 1, D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0);
	auto const buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(buf, "create EFB color temp texture (size: %dx%d)", target_width, target_height);
	m_efb.color_temp_tex.reset(new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM));
	CHECK(m_efb.color_temp_tex!=NULL, "create EFB color temp texture (size: %dx%d)", target_width, target_height);
	D3D::SetDebugObjectName(m_efb.color_temp_tex->GetTex(), "EFB color temp texture");
	D3D::SetDebugObjectName(m_efb.color_temp_tex->GetSRV(), "EFB color temp texture shader resource view");
	D3D::SetDebugObjectName(m_efb.color_temp_tex->GetRTV(), "EFB color temp texture render target view");
	}

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_tex
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	m_efb.color_staging_buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(m_efb.color_staging_buf, "create EFB color staging buffer");
	D3D::SetDebugObjectName(m_efb.color_staging_buf, "EFB color staging texture (used for Renderer::AccessEFB)");
	}

	// EFB depth buffer - primary depth buffer
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width, target_height, 1, 1, D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	auto const buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(buf, "create EFB depth texture (size: %dx%d)", target_width, target_height);
	m_efb.depth_tex.reset(new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE),
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1)));
	D3D::SetDebugObjectName(m_efb.depth_tex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName(m_efb.depth_tex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName(m_efb.depth_tex->GetSRV(), "EFB depth texture shader resource view");
	}

	// Render buffer for AccessEFB (depth data)
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 1, D3D11_BIND_RENDER_TARGET);
	auto const buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(buf, "create EFB depth read texture");
	m_efb.depth_read_texture.reset(new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET));
	D3D::SetDebugObjectName(m_efb.depth_read_texture->GetTex(), "EFB depth read texture (used in Renderer::AccessEFB)");
	D3D::SetDebugObjectName(m_efb.depth_read_texture->GetRTV(), "EFB depth read texture render target view (used in Renderer::AccessEFB)");
	}

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	{
	auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	m_efb.depth_staging_buf = CreateTexture2DShared(&texdesc, NULL);
	CHECK(m_efb.depth_staging_buf, "create EFB depth staging buffer");
	D3D::SetDebugObjectName(m_efb.depth_staging_buf, "EFB depth staging texture (used for Renderer::AccessEFB)");
	}

	if (g_ActiveConfig.iMultisampleMode)
	{
		// Framebuffer resolve textures (color+depth)
		{
		auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height,
			1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);
		auto const buf = CreateTexture2DShared(&texdesc, NULL);
		m_efb.resolved_color_tex.reset(new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM));
		CHECK(m_efb.resolved_color_tex!=NULL, "create EFB color resolve texture (size: %dx%d)", target_width, target_height);
		D3D::SetDebugObjectName(m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture");
		D3D::SetDebugObjectName(m_efb.resolved_color_tex->GetSRV(), "EFB color resolve texture shader resource view");
		}

		{
		auto texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width, target_height, 1, 1, D3D11_BIND_SHADER_RESOURCE);
		auto const buf = CreateTexture2DShared(&texdesc, NULL);
		CHECK(buf, "create EFB depth resolve texture (size: %dx%d)", target_width, target_height);
		m_efb.resolved_depth_tex.reset(new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R24_UNORM_X8_TYPELESS));
		D3D::SetDebugObjectName(m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture");
		D3D::SetDebugObjectName(m_efb.resolved_depth_tex->GetSRV(), "EFB depth resolve texture shader resource view");
		}
	}
	else
	{
		m_efb.resolved_color_tex = NULL;
		m_efb.resolved_depth_tex = NULL;
	}
}

FramebufferManager::~FramebufferManager()
{
	m_efb.color_tex.reset();
	m_efb.color_staging_buf.reset();

	m_efb.depth_tex.reset();
	m_efb.depth_staging_buf.reset();
	m_efb.depth_read_texture.reset();

	m_efb.color_temp_tex.reset();

	m_efb.resolved_color_tex.reset();
	m_efb.resolved_depth_tex.reset();
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	m_xfbEncoder.Encode(dst, fbWidth, fbHeight, sourceRc, Gamma);
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	return new XFBSource(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM));
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
	D3D::drawShadedTexSubQuad(tex->GetSRV(), &sourcerc,
		texWidth, texHeight, &drawrc, PixelShaderCache::GetColorCopyProgram(false),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX11's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float Gamma)
{
	// Copy EFB data to XFB and restore render target again
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)texWidth, (float)texHeight);

	D3D::g_context->RSSetViewports(1, &vp);
	D3D::g_context->OMSetRenderTargets(1, &tex->GetRTV(), NULL);
	D3D::SetLinearCopySampler();

	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture()->GetSRV(), sourceRc.AsRECT(),
		Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(),
		PixelShaderCache::GetColorCopyProgram(true), VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(),Gamma);

	D3D::g_context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

}  // namespace DX11