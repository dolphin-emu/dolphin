// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoBackends/D3D/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static XFBEncoder s_xfbEncoder;

FramebufferManager::Efb FramebufferManager::m_efb;
unsigned int FramebufferManager::m_target_width;
unsigned int FramebufferManager::m_target_height;
ID3D11DepthStencilState* FramebufferManager::m_depth_resolve_depth_state;

D3DTexture2D* &FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
D3DTexture2D* &FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }

D3DTexture2D* &FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		for (int i = 0; i < m_efb.slices; i++)
			D3D::context->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), m_efb.color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
		return m_efb.resolved_color_tex;
	}
	else
		return m_efb.color_tex;
}

D3DTexture2D* &FramebufferManager::GetResolvedEFBDepthTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		// ResolveSubresource does not work with depth textures.
		// Instead, we use a shader that selects the minimum depth from all samples.

		// Clear render state, and enable depth writes.
		g_renderer->ResetAPIState();
		D3D::stateman->PushDepthState(m_depth_resolve_depth_state);

		// Set up to render to resolved depth texture.
		const D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.f, 0.f, (float)m_target_width, (float)m_target_height);
		D3D::context->RSSetViewports(1, &viewport);
		D3D::context->OMSetRenderTargets(0, nullptr, m_efb.resolved_depth_tex->GetDSV());

		// Render a quad covering the entire target, writing SV_Depth.
		const D3D11_RECT source_rect = CD3D11_RECT(0, 0, m_target_width, m_target_height);
		D3D::drawShadedTexQuad(m_efb.depth_tex->GetSRV(), &source_rect, m_target_width, m_target_height,
			PixelShaderCache::GetDepthResolveProgram(), VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader());

		// Restore render state.
		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
		D3D::stateman->PopDepthState();
		g_renderer->RestoreAPIState();

		return m_efb.resolved_depth_tex;
	}
	else
		return m_efb.depth_tex;
}

FramebufferManager::FramebufferManager()
{
	m_target_width = Renderer::GetTargetWidth();
	m_target_height = Renderer::GetTargetHeight();
	if (m_target_height < 1)
	{
		m_target_height = 1;
	}
	if (m_target_width < 1)
	{
		m_target_width = 1;
	}
	DXGI_SAMPLE_DESC sample_desc;
	sample_desc.Count = g_ActiveConfig.iMultisamples;
	sample_desc.Quality = 0;

	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// EFB color texture - primary render target
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB color texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetRTV(), "EFB color texture render target view");

	// Temporary EFB color texture - used in ReinterpretPixelData
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB color temp texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_temp_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetTex(), "EFB color temp texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetSRV(), "EFB color temp texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetRTV(), "EFB color temp texture render target view");

	// EFB depth buffer - primary depth buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetSRV(), "EFB depth texture shader resource view");

	if (g_ActiveConfig.iMultisamples > 1)
	{
		// Framebuffer resolve textures (color+depth)
		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);
		hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
		CHECK(hr==S_OK, "create EFB color resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_color_tex = new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetSRV(), "EFB color resolve texture shader resource view");

		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL);
		hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
		CHECK(hr==S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL), DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetSRV(), "EFB depth resolve texture shader resource view");

		// Depth state used when writing resolved depth texture
		D3D11_DEPTH_STENCIL_DESC depth_resolve_depth_state = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
		depth_resolve_depth_state.DepthEnable = TRUE;
		depth_resolve_depth_state.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depth_resolve_depth_state.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		hr = D3D::device->CreateDepthStencilState(&depth_resolve_depth_state, &m_depth_resolve_depth_state);
		CHECK(hr == S_OK, "create depth resolve depth stencil state");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_depth_resolve_depth_state, "depth resolve depth stencil state");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
		m_depth_resolve_depth_state = nullptr;
	}

	s_xfbEncoder.Init();

	InitializeEFBAccessCopies();
}

FramebufferManager::~FramebufferManager()
{
	s_xfbEncoder.Shutdown();

	DestroyEFBAccessCopies();

	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.color_temp_tex);
	SAFE_RELEASE(m_efb.resolved_color_tex);
	SAFE_RELEASE(m_efb.resolved_depth_tex);
	SAFE_RELEASE(m_depth_resolve_depth_state);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	// below div2 due to dx using pixel width
	s_xfbEncoder.Encode(dst, fbStride/2, fbHeight, sourceRc, Gamma);
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers)
{
	return std::make_unique<XFBSource>(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, layers), layers);
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height)
{
	*width = m_target_width;
	*height = m_target_height;
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX11's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float Gamma)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	// Copy EFB data to XFB and restore render target again
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)texWidth, (float)texHeight);
	const D3D11_RECT rect = CD3D11_RECT(0, 0, texWidth, texHeight);

	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &tex->GetRTV(), nullptr);
	D3D::SetPointCopySampler();

	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture()->GetSRV(), &rect,
		Renderer::GetTargetWidth(), Renderer::GetTargetHeight(), PixelShaderCache::GetColorCopyProgram(true),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
		GeometryShaderCache::GetCopyGeometryShader(), Gamma);

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

u32 FramebufferManager::ReadEFBColorAccessCopy(u32 x, u32 y)
{
	if (!m_efb.color_access_staging_map.pData)
		MapEFBColorAccessCopy();

	u32 color;
	size_t buffer_offset = y * m_efb.color_access_staging_map.RowPitch + x * sizeof(u32);
	memcpy(&color, reinterpret_cast<u8*>(m_efb.color_access_staging_map.pData) + buffer_offset, sizeof(color));
	return color;
}

float FramebufferManager::ReadEFBDepthAccessCopy(u32 x, u32 y)
{
	if (!m_efb.depth_access_staging_map.pData)
		MapEFBDepthAccessCopy();

	float depth;
	size_t buffer_offset = y * m_efb.depth_access_staging_map.RowPitch + x * sizeof(float);
	memcpy(&depth, reinterpret_cast<u8*>(m_efb.depth_access_staging_map.pData) + buffer_offset, sizeof(depth));
	return depth;
}

void FramebufferManager::UpdateEFBColorAccessCopy(u32 x, u32 y, u32 color)
{
	if (!m_efb.color_access_staging_map.pData)
		return;

	size_t buffer_offset = y * m_efb.color_access_staging_map.RowPitch + x * sizeof(u32);
	memcpy(reinterpret_cast<u8*>(m_efb.color_access_staging_map.pData) + buffer_offset, &color, sizeof(color));
}

void FramebufferManager::UpdateEFBDepthAccessCopy(u32 x, u32 y, float depth)
{
	if (!m_efb.depth_access_staging_map.pData)
		return;

	size_t buffer_offset = y * m_efb.depth_access_staging_map.RowPitch + x * sizeof(float);
	memcpy(reinterpret_cast<u8*>(m_efb.depth_access_staging_map.pData) + buffer_offset, &depth, sizeof(depth));
}

void FramebufferManager::InitializeEFBAccessCopies()
{
	CD3D11_TEXTURE2D_DESC texdesc;
	ID3D11Texture2D* buf;
	HRESULT hr;

	// EFB access - color resize buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB color read texture (hr=%#x)", hr);
	m_efb.color_access_resize_tex = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_efb.color_access_resize_tex->GetTex(), "EFB access color resize buffer");
	D3D::SetDebugObjectName(m_efb.color_access_resize_tex->GetRTV(), "EFB access color resize buffer render target view");

	// EFB access - color staging/readback buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &m_efb.color_access_staging_tex);
	CHECK(hr == S_OK, "create EFB color staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName(m_efb.color_access_staging_tex, "EFB access color staging buffer");

	// Render buffer for AccessEFB (depth data)
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB depth read texture (hr=%#x)", hr);
	m_efb.depth_access_resize_tex = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_efb.depth_access_resize_tex->GetTex(), "EFB access depth resize buffer");
	D3D::SetDebugObjectName(m_efb.depth_access_resize_tex->GetRTV(), "EFB access depth resize buffer render target view");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &m_efb.depth_access_staging_tex);
	CHECK(hr == S_OK, "create EFB depth staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName(m_efb.depth_access_staging_tex, "EFB access depth staging buffer");
}

void FramebufferManager::MapEFBColorAccessCopy()
{
	ID3D11Texture2D* src_texture;
	if (m_target_width != EFB_WIDTH || m_target_height != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
	{
		// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
		g_renderer->ResetAPIState();

		CD3D11_VIEWPORT vp(0.0f, 0.0f, EFB_WIDTH, EFB_HEIGHT);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->OMSetRenderTargets(1, &m_efb.color_access_resize_tex->GetRTV(), nullptr);
		D3D::SetPointCopySampler();

		CD3D11_RECT src_rect(0, 0, m_target_width, m_target_height);
		D3D::drawShadedTexQuad(m_efb.color_tex->GetSRV(), &src_rect, m_target_width, m_target_height,
							   PixelShaderCache::GetColorCopyProgram(true),
							   VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
							   nullptr, 1.0f, 0);

		D3D::context->OMSetRenderTargets(1, &GetEFBColorTexture()->GetRTV(), GetEFBDepthTexture()->GetDSV());
		g_renderer->RestoreAPIState();

		src_texture = m_efb.color_access_resize_tex->GetTex();
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.color_tex->GetTex();
	}

	D3D::context->CopySubresourceRegion(m_efb.color_access_staging_tex, 0, 0, 0, 0, src_texture, 0, nullptr);

	HRESULT hr = D3D::context->Map(m_efb.color_access_staging_tex, 0, D3D11_MAP_READ_WRITE, 0, &m_efb.color_access_staging_map);
	CHECK(SUCCEEDED(hr), "failed to map EFB access color texture (hr=%08X)", hr);
}

void FramebufferManager::MapEFBDepthAccessCopy()
{
	ID3D11Texture2D* src_texture;
	if (m_target_width != EFB_WIDTH || m_target_height != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
	{
		// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
		g_renderer->ResetAPIState();

		CD3D11_VIEWPORT vp(0.0f, 0.0f, EFB_WIDTH, EFB_HEIGHT);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->OMSetRenderTargets(1, &m_efb.depth_access_resize_tex->GetRTV(), nullptr);
		D3D::SetPointCopySampler();

		// MSAA has to go through a different path for depth
		CD3D11_RECT src_rect(0, 0, m_target_width, m_target_height);
		D3D::drawShadedTexQuad(m_efb.depth_tex->GetSRV(), &src_rect, m_target_width, m_target_height,
							   (g_ActiveConfig.iMultisamples > 1) ? PixelShaderCache::GetDepthResolveProgram() : PixelShaderCache::GetColorCopyProgram(false),
							   VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
							   nullptr, 1.0f, 0);

		D3D::context->OMSetRenderTargets(1, &GetEFBColorTexture()->GetRTV(), GetEFBDepthTexture()->GetDSV());
		g_renderer->RestoreAPIState();

		src_texture = m_efb.depth_access_resize_tex->GetTex();
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.depth_tex->GetTex();
	}

	D3D::context->CopySubresourceRegion(m_efb.depth_access_staging_tex, 0, 0, 0, 0, src_texture, 0, nullptr);

	HRESULT hr = D3D::context->Map(m_efb.depth_access_staging_tex, 0, D3D11_MAP_READ_WRITE, 0, &m_efb.depth_access_staging_map);
	CHECK(SUCCEEDED(hr), "failed to map EFB access depth texture (hr=%08X)", hr);
}

void FramebufferManager::InvalidateEFBAccessCopies()
{
	if (m_efb.color_access_staging_map.pData)
	{
		D3D::context->Unmap(m_efb.color_access_staging_tex, 0);
		m_efb.color_access_staging_map.pData = nullptr;
	}

	if (m_efb.depth_access_staging_map.pData)
	{
		D3D::context->Unmap(m_efb.depth_access_staging_tex, 0);
		m_efb.depth_access_staging_map.pData = nullptr;
	}
}

void FramebufferManager::DestroyEFBAccessCopies()
{
	InvalidateEFBAccessCopies();

	SAFE_RELEASE(m_efb.color_access_resize_tex);
	SAFE_RELEASE(m_efb.color_access_staging_tex);
	SAFE_RELEASE(m_efb.depth_access_resize_tex);
	SAFE_RELEASE(m_efb.depth_access_staging_tex);
}

}  // namespace DX11
