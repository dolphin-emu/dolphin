// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static XFBEncoder s_xfbEncoder;

FramebufferManager::Efb FramebufferManager::m_efb;
unsigned int FramebufferManager::m_target_width;
unsigned int FramebufferManager::m_target_height;

D3DTexture2D*& FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
D3DTexture2D*& FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }

D3DTexture2D*& FramebufferManager::GetEFBColorTempTexture() { return m_efb.color_temp_tex; }

void FramebufferManager::SwapReinterpretTexture()
{
	D3DTexture2D* swaptex = GetEFBColorTempTexture();
	m_efb.color_temp_tex = GetEFBColorTexture();
	m_efb.color_tex = swaptex;
}

D3DTexture2D*& FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		m_efb.resolved_color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

		for (int i = 0; i < m_efb.slices; i++)
		{
			D3D::current_command_list->ResolveSubresource(m_efb.resolved_color_tex->GetTex12(), D3D11CalcSubresource(0, i, 1), m_efb.color_tex->GetTex12(), D3D11CalcSubresource(0, i, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
		}

		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

		return m_efb.resolved_color_tex;
	}
	else
	{
		return m_efb.color_tex;
	}
}

D3DTexture2D*& FramebufferManager::GetResolvedEFBDepthTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		ResolveDepthTexture();

		return m_efb.resolved_depth_tex;
	}
	else
	{
		return m_efb.depth_tex;
	}
}

FramebufferManager::FramebufferManager()
{
	m_target_width = std::max(Renderer::GetTargetWidth(), 1);
	m_target_height = std::max(Renderer::GetTargetHeight(), 1);

	DXGI_SAMPLE_DESC sample_desc;
	sample_desc.Count = g_ActiveConfig.iMultisamples;
	sample_desc.Quality = 0;

	ID3D12Resource* buf12;
	D3D12_RESOURCE_DESC texdesc12;
	D3D12_CLEAR_VALUE optimized_clear_valueRTV = { DXGI_FORMAT_R8G8B8A8_UNORM, { 0.0f, 0.0f, 0.0f, 1.0f } };
	D3D12_CLEAR_VALUE optimized_clear_valueDSV = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);

	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// EFB color texture - primary render target
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12));

	m_efb.color_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);

	// Temporary EFB color texture - used in ReinterpretPixelData
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12)));
	m_efb.color_temp_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.color_temp_tex->GetTex12(), "EFB color temp texture");

	// EFB depth buffer - primary depth buffer
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueDSV, IID_PPV_ARGS(&buf12)));

	m_efb.depth_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.depth_tex->GetTex12(), "EFB depth texture");

	if (g_ActiveConfig.iMultisamples > 1)
	{
		// Framebuffer resolve textures (color+depth)
		texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1);
		hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buf12));
		CHECK(hr == S_OK, "create EFB color resolve texture (size: %dx%d)", m_target_width, m_target_height);
		m_efb.resolved_color_tex = new D3DTexture2D(buf12, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_COMMON);
		SAFE_RELEASE(buf12);
		D3D::SetDebugObjectName12(m_efb.resolved_color_tex->GetTex12(), "EFB color resolve texture shader resource view");

		texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, m_target_width, m_target_height, m_efb.slices, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buf12));
		CHECK(hr == S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_COMMON);
		SAFE_RELEASE(buf12);
		D3D::SetDebugObjectName12(m_efb.resolved_depth_tex->GetTex12(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
	}

	InitializeEFBAccessCopies();

	s_xfbEncoder.Init();
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
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	s_xfbEncoder.Encode(dst, fbStride/2, fbHeight, sourceRc, gamma);
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers)
{
	return std::make_unique<XFBSource>(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, layers), layers);
}

void FramebufferManager::GetTargetSize(unsigned int* width, unsigned int* height)
{
	*width = m_target_width;
	*height = m_target_height;
}

void FramebufferManager::ResolveDepthTexture()
{
	// ResolveSubresource does not work with depth textures.
	// Instead, we use a shader that selects the minimum depth from all samples.
	D3D::SetViewportAndScissor(0, 0, m_target_width, m_target_height);

	m_efb.resolved_depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(0, nullptr, FALSE, &m_efb.resolved_depth_tex->GetDSV12());

	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, m_target_width, m_target_height);
	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBDepthTexture(),
		&source_rect,
		m_target_width,
		m_target_height,
		StaticShaderCache::GetDepthResolveToColorPixelShader(),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0,
		0,
		DXGI_FORMAT_D32_FLOAT
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

u32 FramebufferManager::ReadEFBColorAccessCopy(u32 x, u32 y)
{
	if (!m_efb.color_access_readback_map)
		MapEFBColorAccessCopy();

	u32 color;
	size_t buffer_offset = y * m_efb.color_access_readback_pitch + x * sizeof(u32);
	memcpy(&color, &m_efb.color_access_readback_map[buffer_offset], sizeof(color));
	return color;
}

float FramebufferManager::ReadEFBDepthAccessCopy(u32 x, u32 y)
{
	if (!m_efb.depth_access_readback_map)
		MapEFBDepthAccessCopy();

	float depth;
	size_t buffer_offset = y * m_efb.depth_access_readback_pitch + x * sizeof(float);
	memcpy(&depth, &m_efb.depth_access_readback_map[buffer_offset], sizeof(depth));
	return depth;
}

void FramebufferManager::UpdateEFBColorAccessCopy(u32 x, u32 y, u32 color)
{
	if (!m_efb.color_access_readback_map)
		return;

	size_t buffer_offset = y * m_efb.color_access_readback_pitch + x * sizeof(u32);
	memcpy(&m_efb.color_access_readback_map[buffer_offset], &color, sizeof(color));
}

void FramebufferManager::UpdateEFBDepthAccessCopy(u32 x, u32 y, float depth)
{
	if (!m_efb.depth_access_readback_map)
		return;

	size_t buffer_offset = y * m_efb.depth_access_readback_pitch + x * sizeof(float);
	memcpy(&m_efb.depth_access_readback_map[buffer_offset], &depth, sizeof(depth));
}

void FramebufferManager::InitializeEFBAccessCopies()
{
	D3D12_CLEAR_VALUE optimized_color_clear_value = { DXGI_FORMAT_R8G8B8A8_UNORM, { 0.0f, 0.0f, 0.0f, 1.0f } };
	D3D12_CLEAR_VALUE optimized_depth_clear_value = { DXGI_FORMAT_R32_FLOAT, { 1.0f } };
	CD3DX12_RESOURCE_DESC texdesc12;
	ID3D12Resource* buf12;
	HRESULT hr;

	// EFB access - color resize buffer
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_color_clear_value, IID_PPV_ARGS(&buf12));
	CHECK(hr == S_OK, "create EFB access color resize buffer (hr=%#x)", hr);
	m_efb.color_access_resize_tex = new D3DTexture2D(buf12, D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_R8G8B8A8_UNORM);
	D3D::SetDebugObjectName12(m_efb.color_access_resize_tex->GetTex12(), "EFB access color resize buffer");
	buf12->Release();

	// EFB access - color staging/readback buffer
	m_efb.color_access_readback_pitch = D3D::AlignValue(EFB_WIDTH * sizeof(u32), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	texdesc12 = CD3DX12_RESOURCE_DESC::Buffer(m_efb.color_access_readback_pitch * EFB_HEIGHT);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_efb.color_access_readback_buffer));
	D3D::SetDebugObjectName12(m_efb.color_access_readback_buffer, "EFB access color readback buffer");

	// EFB access - depth resize buffer
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_depth_clear_value, IID_PPV_ARGS(&buf12));
	CHECK(hr == S_OK, "create EFB access depth resize buffer (hr=%#x)", hr);
	m_efb.depth_access_resize_tex = new D3DTexture2D(buf12, D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_R32_FLOAT);
	D3D::SetDebugObjectName12(m_efb.color_access_resize_tex->GetTex12(), "EFB access depth resize buffer");
	buf12->Release();

	// EFB access - depth staging/readback buffer
	m_efb.depth_access_readback_pitch = D3D::AlignValue(EFB_WIDTH * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	texdesc12 = CD3DX12_RESOURCE_DESC::Buffer(m_efb.depth_access_readback_pitch * EFB_HEIGHT);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_efb.depth_access_readback_buffer));
	D3D::SetDebugObjectName12(m_efb.color_access_readback_buffer, "EFB access depth readback buffer");
}

void FramebufferManager::MapEFBColorAccessCopy()
{
	D3D::command_list_mgr->CPUAccessNotify();

	ID3D12Resource* src_resource;
	if (m_target_width != EFB_WIDTH || m_target_height != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
	{
		// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
		m_efb.color_access_resize_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D::SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
		D3D::SetPointCopySampler();
		D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_access_resize_tex->GetRTV12(), FALSE, nullptr);

		CD3DX12_RECT src_rect(0, 0, m_target_width, m_target_height);
		D3D::DrawShadedTexQuad(m_efb.color_tex, &src_rect, m_target_width, m_target_height,
							   StaticShaderCache::GetColorCopyPixelShader(true),
							   StaticShaderCache::GetSimpleVertexShader(),
							   StaticShaderCache::GetSimpleVertexShaderInputLayout(),
							   {}, 1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);

		m_efb.color_access_resize_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		src_resource = m_efb.color_access_resize_tex->GetTex12();
	}
	else
	{
		// Can source the EFB buffer
		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		src_resource = m_efb.color_tex->GetTex12();
	}

	// Copy to staging resource
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT dst_footprint = { 0, { DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, m_efb.color_access_readback_pitch } };
	CD3DX12_TEXTURE_COPY_LOCATION dst_location(m_efb.color_access_readback_buffer, dst_footprint);
	CD3DX12_TEXTURE_COPY_LOCATION src_location(src_resource, 0);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

	// Block until completion
	D3D::command_list_mgr->ExecuteQueuedWork(true);

	// Restore EFB resource state if it was sourced from here
	if (src_resource == m_efb.color_tex->GetTex12())
		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Restore state after resetting command list
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
	g_renderer->RestoreAPIState();

	// Resource copy has finished, so safe to map now
	m_efb.color_access_readback_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_efb.color_access_readback_map));
}

void FramebufferManager::MapEFBDepthAccessCopy()
{
	D3D::command_list_mgr->CPUAccessNotify();

	ID3D12Resource* src_resource;
	if (m_target_width != EFB_WIDTH || m_target_height != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
	{
		// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
		m_efb.depth_access_resize_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D::SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
		D3D::SetPointCopySampler();
		D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_access_resize_tex->GetRTV12(), FALSE, nullptr);

		CD3DX12_RECT src_rect(0, 0, m_target_width, m_target_height);
		D3D::DrawShadedTexQuad(m_efb.depth_tex, &src_rect, m_target_width, m_target_height,
							   (g_ActiveConfig.iMultisamples > 1) ? StaticShaderCache::GetDepthResolveToColorPixelShader() : StaticShaderCache::GetColorCopyPixelShader(false),
							   StaticShaderCache::GetSimpleVertexShader(),
							   StaticShaderCache::GetSimpleVertexShaderInputLayout(),
							   {}, 1.0f, 0, DXGI_FORMAT_R32_FLOAT, false, false);

		m_efb.depth_access_resize_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		src_resource = m_efb.depth_access_resize_tex->GetTex12();
	}
	else
	{
		// Can source the EFB buffer
		m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		src_resource = m_efb.depth_tex->GetTex12();
	}

	// Copy to staging resource
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT dst_footprint = { 0,{ DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, m_efb.depth_access_readback_pitch } };
	CD3DX12_TEXTURE_COPY_LOCATION dst_location(m_efb.depth_access_readback_buffer, dst_footprint);
	CD3DX12_TEXTURE_COPY_LOCATION src_location(src_resource, 0);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

	// Block until completion
	D3D::command_list_mgr->ExecuteQueuedWork(true);

	// Restore EFB resource state if it was sourced from here
	if (src_resource == m_efb.depth_tex->GetTex12())
		m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// Restore state after resetting command list
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
	g_renderer->RestoreAPIState();

	// Resource copy has finished, so safe to map now
	m_efb.depth_access_readback_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_efb.depth_access_readback_map));
}

void FramebufferManager::InvalidateEFBAccessCopies()
{
	if (m_efb.color_access_readback_map)
	{
		m_efb.color_access_readback_buffer->Unmap(0, nullptr);
		m_efb.color_access_readback_map = nullptr;
	}

	if (m_efb.depth_access_readback_map)
	{
		m_efb.depth_access_readback_buffer->Unmap(0, nullptr);
		m_efb.depth_access_readback_map = nullptr;
	}
}

void FramebufferManager::DestroyEFBAccessCopies()
{
	InvalidateEFBAccessCopies();

	SAFE_RELEASE(m_efb.color_access_resize_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.color_access_readback_buffer);
	m_efb.color_access_readback_buffer = nullptr;

	SAFE_RELEASE(m_efb.depth_access_resize_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.depth_access_readback_buffer);
	m_efb.depth_access_readback_buffer = nullptr;
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX12's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float gamma)
{
	// Copy EFB data to XFB and restore render target again
	D3D::SetViewportAndScissor(0, 0, texWidth, texHeight);

	const D3D12_RECT rect = CD3DX12_RECT(0, 0, texWidth, texHeight);

	m_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_tex->GetRTV12(), FALSE, nullptr);

	D3D::SetPointCopySampler();

	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		&rect,
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		StaticShaderCache::GetColorCopyPixelShader(true),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		gamma,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		false,
		m_tex->GetMultisampled()
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

}  // namespace DX12
