// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32

#include "VideoCommon/VR920.h"
#endif

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D/AvatarDrawer.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoBackends/D3D/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

namespace DX11 {

static XFBEncoder s_xfbEncoder;
AvatarDrawer s_avatarDrawer;

FramebufferManager::Efb FramebufferManager::m_efb;
unsigned int FramebufferManager::m_target_width;
unsigned int FramebufferManager::m_target_height;

// Oculus Rift
#ifdef HAVE_OCULUSSDK
ovrD3D11Texture FramebufferManager::m_eye_texture[2];
#endif
bool FramebufferManager::m_stereo3d = false;
int FramebufferManager::m_eye_count = 1;

D3DTexture2D* &FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
ID3D11Texture2D* &FramebufferManager::GetEFBColorStagingBuffer() { return m_efb.color_staging_buf; }

D3DTexture2D* &FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }
D3DTexture2D* &FramebufferManager::GetEFBDepthReadTexture() { return m_efb.depth_read_texture; }
ID3D11Texture2D* &FramebufferManager::GetEFBDepthStagingBuffer() { return m_efb.depth_staging_buf; }

D3DTexture2D* &FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisampleMode)
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
	if (g_ActiveConfig.iMultisampleMode)
	{
		for (int i = 0; i < m_efb.slices; i++)
			D3D::context->ResolveSubresource(m_efb.resolved_depth_tex->GetTex(), D3D11CalcSubresource(0, i, 1), m_efb.depth_tex->GetTex(), D3D11CalcSubresource(0, i, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
		return m_efb.resolved_depth_tex;
	}
	else
		return m_efb.depth_tex;
}

FramebufferManager::FramebufferManager()
{
	for (int eye = 0; eye < 2; ++eye)
	{
		m_efb.m_frontBuffer[eye] = nullptr;
		// init to null
	}
	if (g_has_hmd)
	{
		m_stereo3d = true;
		m_eye_count = 2;
	}
	else
	{
		m_stereo3d = false;
		m_eye_count = 1;
	}

	m_target_width = Renderer::GetTargetWidth();
	m_target_height = Renderer::GetTargetHeight();
	DXGI_SAMPLE_DESC sample_desc = D3D::GetAAMode(g_ActiveConfig.iMultisampleMode);

#ifdef HAVE_OCULUSSDK
	if (g_has_rift)
	{
		ConfigureRift();
	}
#endif

	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

#ifdef HAVE_OCULUSSDK
	if (g_has_rift)
	{
		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, sample_desc.Quality);
		for (int eye=0; eye<2; ++eye)
		{
			hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
			CHECK(hr == S_OK, "create Oculus Rift eye texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
			m_efb.m_frontBuffer[eye] = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, false);
			CHECK(m_efb.m_frontBuffer[eye] != nullptr, "create Oculus Rift eye texture (size: %dx%d)", m_target_width, m_target_height);
			SAFE_RELEASE(buf);
		}
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[0]->GetTex(), "Left eye color texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[0]->GetSRV(), "Left eye color texture shader resource view");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[0]->GetRTV(), "Left eye color texture render target view");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[1]->GetTex(), "Right eye color texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[1]->GetSRV(), "Right eye color texture shader resource view");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.m_frontBuffer[1]->GetRTV(), "Right eye color texture render target view");
	}
#endif

	// EFB color texture - primary render target
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB color texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	CHECK(m_efb.color_tex!=nullptr, "create EFB color texture (size: %dx%d)", m_target_width, m_target_height);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetRTV(), "EFB color texture render target view");

	// Temporary EFB color texture - used in ReinterpretPixelData
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB color temp texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_temp_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	CHECK(m_efb.color_temp_tex!=nullptr, "create EFB color temp texture (size: %dx%d)", m_target_width, m_target_height);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetTex(), "EFB color temp texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetSRV(), "EFB color temp texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetRTV(), "EFB color temp texture render target view");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_tex
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, m_efb.slices, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &m_efb.color_staging_buf);
	CHECK(hr==S_OK, "create EFB color staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_staging_buf, "EFB color staging texture (used for Renderer::AccessEFB)");

	// EFB depth buffer - primary depth buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetSRV(), "EFB depth texture shader resource view");

	// Render buffer for AccessEFB (depth data)
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 1, 1, m_efb.slices, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
	CHECK(hr==S_OK, "create EFB depth read texture (hr=%#x)", hr);
	m_efb.depth_read_texture = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_read_texture->GetTex(), "EFB depth read texture (used in Renderer::AccessEFB)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_read_texture->GetRTV(), "EFB depth read texture render target view (used in Renderer::AccessEFB)");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 1, 1, m_efb.slices, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &m_efb.depth_staging_buf);
	CHECK(hr==S_OK, "create EFB depth staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_staging_buf, "EFB depth staging texture (used for Renderer::AccessEFB)");

	if (g_ActiveConfig.iMultisampleMode)
	{
		// Framebuffer resolve textures (color+depth)
		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);
		hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
		m_efb.resolved_color_tex = new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM);
		CHECK(m_efb.resolved_color_tex!=nullptr, "create EFB color resolve texture (size: %dx%d)", m_target_width, m_target_height);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetSRV(), "EFB color resolve texture shader resource view");

		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE);
		hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &buf);
		CHECK(hr==S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetSRV(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
	}
	if (g_has_vr920)
	{
#ifdef _WIN32
		VR920_StartStereo3D();
#endif
	}
	else if (g_has_rift)
	{
#ifdef HAVE_OCULUSSDK
		m_eye_texture[0].D3D11.Header.API = ovrRenderAPI_D3D11;
		m_eye_texture[0].D3D11.Header.TextureSize.w = Renderer::GetTargetWidth();
		m_eye_texture[0].D3D11.Header.TextureSize.h = Renderer::GetTargetHeight();
		m_eye_texture[0].D3D11.Header.RenderViewport.Pos.x = 0;
		m_eye_texture[0].D3D11.Header.RenderViewport.Pos.y = 0;
		m_eye_texture[0].D3D11.Header.RenderViewport.Size.w = Renderer::GetTargetWidth();
		m_eye_texture[0].D3D11.Header.RenderViewport.Size.h = Renderer::GetTargetHeight();
		m_eye_texture[0].D3D11.pTexture = m_efb.m_frontBuffer[0]->GetTex();
		m_eye_texture[0].D3D11.pSRView = m_efb.m_frontBuffer[0]->GetSRV();
		m_eye_texture[1] = m_eye_texture[0];
		if (g_ActiveConfig.iStereoMode == STEREO_OCULUS)
		{
			m_eye_texture[1].D3D11.pTexture = m_efb.m_frontBuffer[1]->GetTex();
			m_eye_texture[1].D3D11.pSRView = m_efb.m_frontBuffer[1]->GetSRV();
		}
#endif
	}

	s_xfbEncoder.Init();
	s_avatarDrawer.Init();
}

FramebufferManager::~FramebufferManager()
{
#ifdef _WIN32
	if (g_has_vr920)
	{
		VR920_StopStereo3D();
	}
#endif
#ifdef HAVE_OCULUSSDK
	// Shut down rendering and release resources (by passing NULL)
	if (g_has_rift)
	{
		ovrHmd_ConfigureRendering(hmd, nullptr, 0, g_eye_fov, g_eye_render_desc);
	}
#endif

	s_xfbEncoder.Shutdown();
	s_avatarDrawer.Shutdown();

	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_temp_tex);
	SAFE_RELEASE(m_efb.color_staging_buf);
	SAFE_RELEASE(m_efb.resolved_color_tex);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_staging_buf);
	SAFE_RELEASE(m_efb.depth_read_texture);
	SAFE_RELEASE(m_efb.resolved_depth_tex);
	SAFE_RELEASE(m_efb.m_frontBuffer[0]);
	SAFE_RELEASE(m_efb.m_frontBuffer[1]);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers)
{
	return new XFBSource(D3DTexture2D::Create(target_width, target_height,
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

	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &tex->GetRTV(), nullptr);
	D3D::SetLinearCopySampler();

	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture()->GetSRV(), sourceRc.AsRECT(),
		Renderer::GetTargetWidth(), Renderer::GetTargetHeight(), PixelShaderCache::GetColorCopyProgram(true),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
		GeometryShaderCache::GetCopyGeometryShader(), Gamma);

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

#ifdef HAVE_OCULUSSDK
void FramebufferManager::ConfigureRift()
{
	ovrD3D11Config cfg;
	cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
#ifdef OCULUSSDK044
	cfg.D3D11.Header.BackBufferSize.w = hmdDesc.Resolution.w;
	cfg.D3D11.Header.BackBufferSize.h = hmdDesc.Resolution.h;
#else
	cfg.D3D11.Header.RTSize.w = hmdDesc.Resolution.w;
	cfg.D3D11.Header.RTSize.h = hmdDesc.Resolution.h;
#endif
	cfg.D3D11.Header.Multisample = 0;
	cfg.D3D11.pDevice = D3D::device;
	cfg.D3D11.pDeviceContext = D3D::context;
	cfg.D3D11.pSwapChain = D3D::swapchain;
	cfg.D3D11.pBackBufferRT = D3D::GetBackBuffer()->GetRTV();
	if (g_is_direct_mode) //If Rift is in Direct Mode
	{
		//To do: This is a bit of a hack, but I haven't found any problems with this.  
		//If we don't want to do this, large changes will be needed to init sequence.
		DX11::D3D::UnloadDXGI();  //Unload CreateDXGIFactory() before ovrHmd_AttachToWindow, or else direct mode won't work.
		ovrHmd_AttachToWindow(hmd, D3D::hWnd, nullptr, nullptr); //Attach to Direct Mode.
		DX11::D3D::LoadDXGI();
	}
	int caps = 0;
	if (g_Config.bChromatic)
		caps |= ovrDistortionCap_Chromatic;
	if (g_Config.bTimewarp)
		caps |= ovrDistortionCap_TimeWarp;
	if (g_Config.bVignette)
		caps |= ovrDistortionCap_Vignette;
	if (g_Config.bNoRestore)
		caps |= ovrDistortionCap_NoRestore;
	if (g_Config.bFlipVertical)
		caps |= ovrDistortionCap_FlipInput;
	if (g_Config.bSRGB)
		caps |= ovrDistortionCap_SRGB;
	if (g_Config.bOverdrive)
		caps |= ovrDistortionCap_Overdrive;
	if (g_Config.bHqDistortion)
		caps |= ovrDistortionCap_HqDistortion;
	ovrHmd_ConfigureRendering(hmd, &cfg.Config, caps,
		g_eye_fov, g_eye_render_desc);
	ovrhmd_EnableHSWDisplaySDKRender(hmd, false); //Disable Health and Safety Warning.
}
#endif

}  // namespace DX11
