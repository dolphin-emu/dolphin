// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "Render.h"
#include "FramebufferManager.h"
#include "VideoConfig.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"
#include "TextureConverter.h"
#include "HW/Memmap.h"

namespace DX9
{

// TODO: this is probably somewhere else
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

#undef CHECK
#define CHECK(hr, Message, ...) if (FAILED(hr)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

inline void GetSurface(IDirect3DTexture9* texture, IDirect3DSurface9** surface)
{
	if (!texture) return;
	texture->GetSurfaceLevel(0, surface);
}

FramebufferManager::Efb FramebufferManager::s_efb;

FramebufferManager::FramebufferManager()
{
	bool depth_textures_supported = true;
	int target_width = Renderer::GetTargetWidth();
	int target_height = Renderer::GetTargetHeight();
	s_efb.color_surface_Format = D3DFMT_A8R8G8B8;

	// EFB color texture - primary render target
	HRESULT hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format, 
										D3DPOOL_DEFAULT, &s_efb.color_texture, NULL);
	GetSurface(s_efb.color_texture, &s_efb.color_surface);
	CHECK(hr, "Create color texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);

	// Render buffer for AccessEFB (color data)
	hr = D3D::dev->CreateTexture(1, 1, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format, 
									D3DPOOL_DEFAULT, &s_efb.colorRead_texture, NULL);
	GetSurface(s_efb.colorRead_texture, &s_efb.color_ReadBuffer);
	CHECK(hr, "Create Color Read Texture (hr=%#x)", hr);

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_ReadBuffer
	hr = D3D::dev->CreateOffscreenPlainSurface(1, 1, s_efb.color_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb.color_OffScreenReadBuffer, NULL);
	CHECK(hr, "Create offscreen color surface (hr=%#x)", hr);

	// Select a Z-buffer texture format with hardware support
	s_efb.depth_surface_Format = D3D::GetSupportedDepthTextureFormat();
	if (s_efb.depth_surface_Format == D3DFMT_UNKNOWN)
	{
		// workaround for Intel GPUs etc: only create a depth _surface_
		depth_textures_supported = false;
		s_efb.depth_surface_Format = D3D::GetSupportedDepthSurfaceFormat(s_efb.color_surface_Format);
		ERROR_LOG(VIDEO, "No supported depth texture format found, disabling Z peeks for EFB access.");
	}

	if (depth_textures_supported)
	{
		// EFB depth buffer - primary depth buffer
		hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb.depth_surface_Format, 
									 D3DPOOL_DEFAULT, &s_efb.depth_texture, NULL);
		GetSurface(s_efb.depth_texture, &s_efb.depth_surface);
		CHECK(hr, "Framebuffer depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);

		// Render buffer for AccessEFB (depth data)
		D3DFORMAT DepthTexFormats[2];
		DepthTexFormats[0] = D3DFMT_D24X8;
		// This is expected to work on all hardware
		DepthTexFormats[1] = D3DFMT_A8R8G8B8;

		for (int i = 0; i < 2; ++i)
		{
			if (D3D::CheckTextureSupport(D3DUSAGE_RENDERTARGET, DepthTexFormats[i]))
			{
				s_efb.depth_ReadBuffer_Format = DepthTexFormats[i];
				break;
			}
		}
		hr = D3D::dev->CreateTexture(4, 4, 1, D3DUSAGE_RENDERTARGET, s_efb.depth_ReadBuffer_Format, 
									D3DPOOL_DEFAULT, &s_efb.depthRead_texture, NULL);
		GetSurface(s_efb.depthRead_texture, &s_efb.depth_ReadBuffer);
		CHECK(hr, "Create depth read texture (hr=%#x)", hr);

		// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_ReadBuffer
		hr = D3D::dev->CreateOffscreenPlainSurface(4, 4, s_efb.depth_ReadBuffer_Format, D3DPOOL_SYSTEMMEM, &s_efb.depth_OffScreenReadBuffer, NULL);
		CHECK(hr, "Create depth offscreen surface (hr=%#x)", hr);
	}
	else if (s_efb.depth_surface_Format)
	{
		// just create a depth surface
		hr = D3D::dev->CreateDepthStencilSurface(target_width, target_height, s_efb.depth_surface_Format, D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb.depth_surface, NULL);
		CHECK(hr, "Framebuffer depth surface (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	}

	// ReinterpretPixelData - EFB color data will be copy-converted to this texture and the buffers are swapped then
	hr = D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format,
										D3DPOOL_DEFAULT, &s_efb.color_reinterpret_texture, NULL);
	GetSurface(s_efb.color_reinterpret_texture, &s_efb.color_reinterpret_surface);
	CHECK(hr, "Create color reinterpret texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
}

FramebufferManager::~FramebufferManager()
{
	SAFE_RELEASE(s_efb.depth_surface);
	SAFE_RELEASE(s_efb.color_surface);
	SAFE_RELEASE(s_efb.color_ReadBuffer);
	SAFE_RELEASE(s_efb.depth_ReadBuffer);
	SAFE_RELEASE(s_efb.color_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb.depth_OffScreenReadBuffer);
	SAFE_RELEASE(s_efb.color_texture);
	SAFE_RELEASE(s_efb.colorRead_texture);
	SAFE_RELEASE(s_efb.depth_texture);
	SAFE_RELEASE(s_efb.depthRead_texture);
	SAFE_RELEASE(s_efb.color_reinterpret_texture);
	SAFE_RELEASE(s_efb.color_reinterpret_surface);
	s_efb.color_surface_Format = D3DFMT_UNKNOWN;
	s_efb.depth_surface_Format = D3DFMT_UNKNOWN;
	s_efb.depth_ReadBuffer_Format = D3DFMT_UNKNOWN;
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	LPDIRECT3DTEXTURE9 tex;
	D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET,
		s_efb.color_surface_Format, D3DPOOL_DEFAULT, &tex, NULL);

	return new XFBSource(tex);
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc)
{
	TargetRectangle targetSource;

	targetSource.top = ScaleToVirtualXfbHeight(sourceRc.top, Renderer::GetBackbufferHeight());
	targetSource.bottom = ScaleToVirtualXfbHeight(sourceRc.bottom, Renderer::GetBackbufferHeight());
	targetSource.left = ScaleToVirtualXfbWidth(sourceRc.left, Renderer::GetBackbufferWidth());
	targetSource.right = ScaleToVirtualXfbWidth(sourceRc.right, Renderer::GetBackbufferWidth());

	*width = targetSource.right - targetSource.left;
	*height = targetSource.bottom - targetSource.top;
}

void XFBSource::Draw(const MathUtil::Rectangle<float> &sourcerc,
	const MathUtil::Rectangle<float> &drawrc, int width, int height) const
{
	D3D::drawShadedTexSubQuad(texture, &sourcerc, texWidth, texHeight, &drawrc, width , height,
		PixelShaderCache::GetColorCopyProgram(0), VertexShaderCache::GetSimpleVertexShader(0));
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, texture);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* xfb_in_ram = Memory::GetPointer(xfbAddr);
	if (!xfb_in_ram)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
	TextureConverter::EncodeToRamYUYV(GetEFBColorTexture(), targetRc, xfb_in_ram, fbWidth, fbHeight,Gamma);
}

void XFBSource::CopyEFB(float Gamma)
{
	g_renderer->ResetAPIState(); // reset any game specific settings
	
	// Copy EFB data to XFB and restore render target again
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	texture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = texWidth;
	vp.Height = texHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	RECT sourcerect;
	sourcerect.bottom = sourceRc.bottom;
	sourcerect.left = sourceRc.left;
	sourcerect.right = sourceRc.right;
	sourcerect.top = sourceRc.top;

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	D3D::drawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(), 
		&sourcerect, 
		Renderer::GetTargetWidth(), 
		Renderer::GetTargetHeight(), 
		texWidth, 
		texHeight, 
		PixelShaderCache::GetColorCopyProgram( g_ActiveConfig.iMultisampleMode), 
		VertexShaderCache::GetSimpleVertexShader( g_ActiveConfig.iMultisampleMode),
		Gamma);

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());

	Rendersurf->Release();
	
	g_renderer->RestoreAPIState();
}

}  // namespace DX9
