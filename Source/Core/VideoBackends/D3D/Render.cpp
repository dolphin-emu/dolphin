// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <unordered_map>

#include "Common/Atomic.h"
#include "Common/MathUtil.h"
#include "Common/Timer.h"

#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/Television.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoBackends/D3D/VRD3D.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

static bool g_first_rift_frame = true;

namespace DX11
{

static u32 s_last_multisample_mode = 0;
static bool s_last_stereo_mode = false;
static bool s_last_xfb_mode = false;

static Television s_television;

ID3D11Buffer* access_efb_cbuf = nullptr;
ID3D11BlendState* clearblendstates[4] = {nullptr};
ID3D11DepthStencilState* cleardepthstates[3] = {nullptr};
ID3D11BlendState* resetblendstate = nullptr;
ID3D11DepthStencilState* resetdepthstate = nullptr;
ID3D11RasterizerState* resetraststate = nullptr;

static ID3D11Texture2D* s_screenshot_texture = nullptr;
static D3DTexture2D* s_3d_vision_texture = nullptr;

// Nvidia stereo blitting struct defined in "nvstereo.h" from the Nvidia SDK
typedef struct _Nv_Stereo_Image_Header
{
	unsigned int    dwSignature;
	unsigned int    dwWidth;
	unsigned int    dwHeight;
	unsigned int    dwBPP;
	unsigned int    dwFlags;
} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e

// GX pipeline state
struct
{
	SamplerState sampler[8];
	BlendState blend;
	ZMode zmode;
	RasterizerState raster;

} gx_state;

StateCache gx_state_cache;

void SetupDeviceObjects()
{
	s_television.Init();

	g_framebuffer_manager = new FramebufferManager;

	HRESULT hr;
	float colmat[20]= {0.0f};
	colmat[0] = colmat[5] = colmat[10] = 1.0f;
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(20*sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = colmat;
	hr = D3D::device->CreateBuffer(&cbdesc, &data, &access_efb_cbuf);
	CHECK(hr==S_OK, "Create constant buffer for Renderer::AccessEFB");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)access_efb_cbuf, "constant buffer for Renderer::AccessEFB");

	D3D11_DEPTH_STENCIL_DESC ddesc;
	ddesc.DepthEnable      = FALSE;
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc        = D3D11_COMPARISON_ALWAYS;
	ddesc.StencilEnable    = FALSE;
	ddesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[0]);
	CHECK(hr==S_OK, "Create depth state for Renderer::ClearScreen");
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
	ddesc.DepthEnable      = TRUE;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[1]);
	CHECK(hr==S_OK, "Create depth state for Renderer::ClearScreen");
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[2]);
	CHECK(hr==S_OK, "Create depth state for Renderer::ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[0], "depth state for Renderer::ClearScreen (depth buffer disabled)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[1], "depth state for Renderer::ClearScreen (depth buffer enabled, writing enabled)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[2], "depth state for Renderer::ClearScreen (depth buffer enabled, writing disabled)");

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = FALSE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = D3D::device->CreateBlendState(&blenddesc, &resetblendstate);
	CHECK(hr==S_OK, "Create blend state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetblendstate, "blend state for Renderer::ResetAPIState");

	clearblendstates[0] = resetblendstate;
	resetblendstate->AddRef();

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED|D3D11_COLOR_WRITE_ENABLE_GREEN|D3D11_COLOR_WRITE_ENABLE_BLUE;
	hr = D3D::device->CreateBlendState(&blenddesc, &clearblendstates[1]);
	CHECK(hr==S_OK, "Create blend state for Renderer::ClearScreen");

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	hr = D3D::device->CreateBlendState(&blenddesc, &clearblendstates[2]);
	CHECK(hr==S_OK, "Create blend state for Renderer::ClearScreen");

	blenddesc.RenderTarget[0].RenderTargetWriteMask = 0;
	hr = D3D::device->CreateBlendState(&blenddesc, &clearblendstates[3]);
	CHECK(hr==S_OK, "Create blend state for Renderer::ClearScreen");

	ddesc.DepthEnable      = FALSE;
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc        = D3D11_COMPARISON_LESS;
	ddesc.StencilEnable    = FALSE;
	ddesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &resetdepthstate);
	CHECK(hr==S_OK, "Create depth state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetdepthstate, "depth stencil state for Renderer::ResetAPIState");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, &resetraststate);
	CHECK(hr==S_OK, "Create rasterizer state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetraststate, "rasterizer state for Renderer::ResetAPIState");

	s_screenshot_texture = nullptr;
}

// Kill off all device objects
void TeardownDeviceObjects()
{
	delete g_framebuffer_manager;

	SAFE_RELEASE(access_efb_cbuf);
	SAFE_RELEASE(clearblendstates[0]);
	SAFE_RELEASE(clearblendstates[1]);
	SAFE_RELEASE(clearblendstates[2]);
	SAFE_RELEASE(clearblendstates[3]);
	SAFE_RELEASE(cleardepthstates[0]);
	SAFE_RELEASE(cleardepthstates[1]);
	SAFE_RELEASE(cleardepthstates[2]);
	SAFE_RELEASE(resetblendstate);
	SAFE_RELEASE(resetdepthstate);
	SAFE_RELEASE(resetraststate);
	SAFE_RELEASE(s_screenshot_texture);
	SAFE_RELEASE(s_3d_vision_texture);

	s_television.Shutdown();

	gx_state_cache.Clear();
}

void CreateScreenshotTexture(const TargetRectangle& rc)
{
	D3D11_TEXTURE2D_DESC scrtex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, rc.GetWidth(), rc.GetHeight(), 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateTexture2D(&scrtex_desc, nullptr, &s_screenshot_texture);
	if (hr != S_OK && ARBruteForcer::ch_bruteforce)
	{
		Core::KillDolphinAndRestart();
	}
	CHECK(hr==S_OK, "Create screenshot staging texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_screenshot_texture, "staging screenshot texture");
}

void Create3DVisionTexture(int width, int height)
{
	// Create a staging texture for 3D vision with signature information in the last row.
	// Nvidia 3D Vision supports full SBS, so there is no loss in resolution during this process.
	D3D11_SUBRESOURCE_DATA sysData;
	sysData.SysMemPitch = 4 * width * 2;
	sysData.pSysMem = new u8[(height + 1) * sysData.SysMemPitch];
	LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((u8*)sysData.pSysMem + height * sysData.SysMemPitch);
	header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
	header->dwWidth = width * 2;
	header->dwHeight = height + 1;
	header->dwBPP = 32;
	header->dwFlags = 0;

	s_3d_vision_texture = D3DTexture2D::Create(width * 2, height + 1, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &sysData);
	delete[] sysData.pSysMem;
}

Renderer::Renderer(void *&window_handle)
{
	g_first_rift_frame = true;
	D3D::Create((HWND)window_handle);

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_last_multisample_mode = g_ActiveConfig.iMultisampleMode;
	s_last_efb_scale = g_ActiveConfig.iEFBScale;
	s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
	s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);
	PixelShaderManager::SetEfbScaleChanged();

	SetupDeviceObjects();

	// Setup GX pipeline state
	gx_state.blend.blend_enable = false;
	gx_state.blend.write_mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	gx_state.blend.src_blend = D3D11_BLEND_ONE;
	gx_state.blend.dst_blend = D3D11_BLEND_ZERO;
	gx_state.blend.blend_op = D3D11_BLEND_OP_ADD;
	gx_state.blend.use_dst_alpha = false;

	for (unsigned int k = 0;k < 8;k++)
	{
		gx_state.sampler[k].packed = 0;
	}

	gx_state.zmode.testenable = false;
	gx_state.zmode.updateenable = false;
	gx_state.zmode.func = ZMode::NEVER;

	gx_state.raster.cull_mode = D3D11_CULL_NONE;
	gx_state.raster.depth_clip_enable = !g_ActiveConfig.bDisableNearClipping;

	// Clear EFB textures
	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), ClearColor);
	D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 0.f, 0);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)s_target_width, (float)s_target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	D3D::BeginFrame();

	// Action Replay culling code brute-forcing
	// begin searching
	if (ARBruteForcer::ch_bruteforce)
		ARBruteForcer::ch_begin_search = true;
}

Renderer::~Renderer()
{
	// On Oculus SDK 0.5 and below, we called BeginFrame so we need a matching EndFrame, but on 0.6 this crashes
#if defined(OVR_MAJOR_VERSION) && OVR_MAJOR_VERSION <= 5
	if (g_has_rift && !g_first_rift_frame && g_ActiveConfig.bEnableVR && !g_ActiveConfig.bAsynchronousTimewarp)
	{
		VR_PresentHMDFrame();
	}
#endif
	g_first_rift_frame = true;

	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)(left+1), (float)(top+1), 20.f, 0.0f, color & 0xFF000000, text);
	D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = EFBToScaledX(rc.left);
	result.top    = EFBToScaledY(rc.top);
	result.right  = EFBToScaledX(rc.right);
	result.bottom = EFBToScaledY(rc.bottom);
	return result;
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
bool Renderer::CheckForResize()
{
	RECT rcWindow;
	GetClientRect(D3D::hWnd, &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;

	// Sanity check
	if ((client_width != Renderer::GetBackbufferWidth() ||
		client_height != Renderer::GetBackbufferHeight()) &&
		client_width >= 4 && client_height >= 4)
	{
		return true;
	}

	return false;
}

void Renderer::SetScissorRect(const EFBRectangle& rc)
{
	TargetRectangle trc;
	// In VR we use the whole EFB instead of just the bpmem.copyTexSrc rectangle passed to this function. 
	if (g_has_hmd)
	{
		EFBRectangle sourceRc;
		sourceRc.left = 0;
		sourceRc.right = EFB_WIDTH - 1;
		sourceRc.top = 0;
		sourceRc.bottom = EFB_HEIGHT - 1;
		trc = g_renderer->ConvertEFBRectangle(sourceRc);
		D3D::context->RSSetScissorRects(1, trc.AsRECT());
	}
	else
	{
		trc = ConvertEFBRectangle(rc);
		D3D::context->RSSetScissorRects(1, trc.AsRECT());
	}
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	UINT8 color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			color_mask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	}
	gx_state.blend.write_mask = color_mask;
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//  - GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//  - GX_PokeAlphaMode (TODO)
//  - GX_PokeAlphaUpdate (TODO)
//  - GX_PokeBlendMode (TODO)
//  - GX_PokeColorUpdate (TODO)
//  - GX_PokeDither (TODO)
//  - GX_PokeDstAlpha (TODO)
//  - GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	// TODO: This function currently is broken if anti-aliasing is enabled
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Texture2D* read_tex;

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;
	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// Take the mean of the resulting dimensions; TODO: Don't use the center pixel, compute the average color instead
	D3D11_RECT RectToLock;
	if (type == PEEK_COLOR || type == PEEK_Z)
	{
		RectToLock.left = (targetPixelRc.left + targetPixelRc.right) / 2;
		RectToLock.top = (targetPixelRc.top + targetPixelRc.bottom) / 2;
		RectToLock.right = RectToLock.left + 1;
		RectToLock.bottom = RectToLock.top + 1;
	}
	else
	{
		RectToLock.left = targetPixelRc.left;
		RectToLock.right = targetPixelRc.right;
		RectToLock.top = targetPixelRc.top;
		RectToLock.bottom = targetPixelRc.bottom;
	}

	if (type == PEEK_Z)
	{
		ResetAPIState(); // Reset any game specific settings

		// depth buffers can only be completely CopySubresourceRegion'ed, so we're using drawShadedTexQuad instead
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, 1.f, 1.f);
		D3D::context->RSSetViewports(1, &vp);
		D3D::stateman->SetPixelConstants(0, access_efb_cbuf);
		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBDepthReadTexture()->GetRTV(), nullptr);
		D3D::SetPointCopySampler();
		D3D::drawShadedTexQuad(FramebufferManager::GetEFBDepthTexture()->GetSRV(),
			&RectToLock,
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			PixelShaderCache::GetColorCopyProgram(true),
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

		// copy to system memory
		D3D11_BOX box = CD3D11_BOX(0, 0, 0, 1, 1, 1);
		read_tex = FramebufferManager::GetEFBDepthStagingBuffer();
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FramebufferManager::GetEFBDepthReadTexture()->GetTex(), 0, &box);

		RestoreAPIState(); // restore game state

		// read the data from system memory
		D3D::context->Map(read_tex, 0, D3D11_MAP_READ, 0, &map);

		// depth buffer is inverted in the d3d backend
		float val = 1.0f - *(float*)map.pData;
		u32 ret = 0;
		if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret = MathUtil::Clamp<u32>((u32)(val * 65536.0f), 0, 0xFFFF);
		}
		else
		{
			ret = MathUtil::Clamp<u32>((u32)(val * 16777216.0f), 0, 0xFFFFFF);
		}
		D3D::context->Unmap(read_tex, 0);

		return ret;
	}
	else if (type == PEEK_COLOR)
	{
		// we can directly copy to system memory here
		read_tex = FramebufferManager::GetEFBColorStagingBuffer();
		D3D11_BOX box = CD3D11_BOX(RectToLock.left, RectToLock.top, 0, RectToLock.right, RectToLock.bottom, 1);
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FramebufferManager::GetEFBColorTexture()->GetTex(), 0, &box);

		// read the data from system memory
		D3D::context->Map(read_tex, 0, D3D11_MAP_READ, 0, &map);
		u32 ret = 0;
		if (map.pData)
			ret = *(u32*)map.pData;
		D3D::context->Unmap(read_tex, 0);

		// check what to do with the alpha channel (GX_PokeAlphaRead)
		PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

		if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
		{
			ret = RGBA8ToRGBA6ToRGBA8(ret);
		}
		else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			ret = RGBA8ToRGB565ToRGBA8(ret);
		}
		if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
		{
			ret |= 0xFF000000;
		}

		if (alpha_read_mode.ReadMode == 2) return ret; // GX_READ_NONE
		else if (alpha_read_mode.ReadMode == 1) return (ret | 0xFF000000); // GX_READ_FF
		else /*if(alpha_read_mode.ReadMode == 0)*/ return (ret & 0x00FFFFFF); // GX_READ_00
	}
	else if (type == POKE_COLOR)
	{
		u32 rgbaColor = (poke_data & 0xFF00FF00) | ((poke_data >> 16) & 0xFF) | ((poke_data << 16) & 0xFF0000);

		// TODO: The first five PE registers may change behavior of EFB pokes, this isn't implemented, yet.
		ResetAPIState();

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight());
		D3D::context->RSSetViewports(1, &vp);

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), nullptr);
		D3D::drawColorQuad(rgbaColor, 0.f,
			(float)RectToLock.left   * 2.f / GetTargetWidth() - 1.f,
			-(float)RectToLock.top    * 2.f / GetTargetHeight() + 1.f,
			(float)RectToLock.right  * 2.f / GetTargetWidth() - 1.f,
			-(float)RectToLock.bottom * 2.f / GetTargetHeight() + 1.f);

		RestoreAPIState();
	}
	else // if (type == POKE_Z)
	{
		// TODO: The first five PE registers may change behavior of EFB pokes, this isn't implemented, yet.
		ResetAPIState();

		D3D::stateman->PushBlendState(clearblendstates[3]);
		D3D::stateman->PushDepthState(cleardepthstates[1]);

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight(),
			1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f,
			1.0f - MathUtil::Clamp<float>((xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f)), 0.0f, 16777215.0f) / 16777216.0f);
		D3D::context->RSSetViewports(1, &vp);

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
			FramebufferManager::GetEFBDepthTexture()->GetDSV());
		D3D::drawColorQuad(0, 1.0f - float(poke_data & 0xFFFFFF) / 16777216.0f,
			(float)RectToLock.left   * 2.f / GetTargetWidth() - 1.f,
			-(float)RectToLock.top    * 2.f / GetTargetHeight() + 1.f,
			(float)RectToLock.right  * 2.f / GetTargetWidth() - 1.f,
			-(float)RectToLock.bottom * 2.f / GetTargetHeight() + 1.f);

		D3D::stateman->PopDepthState();
		D3D::stateman->PopBlendState();

		RestoreAPIState();
	}

	return 0;
}


void Renderer::SetViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz

	// D3D crashes for zero viewports
	if (xfmem.viewport.wd == 0 || xfmem.viewport.ht == 0)
		return;

	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float X, Y, Wd, Ht;
	X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff);
	Y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff);
	Wd = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	Ht = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (Wd < 0.0f)
	{
		X += Wd;
		Wd = -Wd;
	}
	if (Ht < 0.0f)
	{
		Y += Ht;
		Ht = -Ht;
	}
	g_requested_viewport = EFBRectangle((int)X, (int)Y, (int)Wd, (int)Ht);

	if (g_viewport_type != VIEW_RENDER_TO_TEXTURE && g_has_hmd && g_ActiveConfig.bEnableVR)
	{
		// In VR we must use the entire EFB, not just the copyTexSrc area that is normally used.
		// So scale from copyTexSrc to entire EFB, and we won't use copyTexSrc during rendering.
		//X = (xfmem.viewport.xOrig - xfmem.viewport.wd - bpmem.copyTexSrcXY.x - (float)scissorXOff) * (float)GetTargetWidth() / (float)bpmem.copyTexSrcWH.x;
		//Y = (xfmem.viewport.yOrig + xfmem.viewport.ht - bpmem.copyTexSrcXY.y - (float)scissorYOff) * (float)GetTargetHeight() / (float)bpmem.copyTexSrcWH.y;
		//Wd = (2.0f * xfmem.viewport.wd) * (float)GetTargetWidth() / (float)bpmem.copyTexSrcWH.x;
		//Ht = (-2.0f * xfmem.viewport.ht) * (float)GetTargetHeight() / (float)bpmem.copyTexSrcWH.y;
		X = 0.0f; Y = 0.0f; Wd = (float)GetTargetWidth(); Ht = (float)GetTargetHeight();
	}

	// In D3D, the viewport rectangle must fit within the render target.
	X = (X >= 0.f) ? X : 0.f;
	Y = (Y >= 0.f) ? Y : 0.f;
	Wd = (X + Wd <= GetTargetWidth()) ? Wd : (GetTargetWidth() - X);
	Ht = (Y + Ht <= GetTargetHeight()) ? Ht : (GetTargetHeight() - Y);
	g_rendered_viewport = EFBRectangle((int)X, (int)Y, (int)Wd, (int)Ht);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(X, Y, Wd, Ht,
		1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f,
		1.0f - MathUtil::Clamp<float>((xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f)), 0.0f, 16777215.0f) / 16777216.0f);
	D3D::context->RSSetViewports(1, &vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	ResetAPIState();

	if (colorEnable && alphaEnable) D3D::stateman->PushBlendState(clearblendstates[0]);
	else if (colorEnable) D3D::stateman->PushBlendState(clearblendstates[1]);
	else if (alphaEnable) D3D::stateman->PushBlendState(clearblendstates[2]);
	else D3D::stateman->PushBlendState(clearblendstates[3]);

	// TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) D3D::stateman->PushDepthState(cleardepthstates[0]);
	else */if (zEnable) D3D::stateman->PushDepthState(cleardepthstates[1]);
	else /*if (!zEnable)*/ D3D::stateman->PushDepthState(cleardepthstates[2]);

	// Update the view port for clearing the picture
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight(), 0.f, 1.f);
	D3D::context->RSSetViewports(1, &vp);

	// Color is passed in bgra mode so we need to convert it to rgba
	u32 rgbaColor = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	D3D::drawClearQuad(rgbaColor, 1.0f - (z & 0xFFFFFF) / 16777216.0f);

	D3D::stateman->PopDepthState();
	D3D::stateman->PopBlendState();

	RestoreAPIState();
}

void Renderer::SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable)
{
	ResetAPIState();

	if (colorEnable && alphaEnable) D3D::stateman->PushBlendState(clearblendstates[0]);
	else if (colorEnable) D3D::stateman->PushBlendState(clearblendstates[1]);
	else if (alphaEnable) D3D::stateman->PushBlendState(clearblendstates[2]);
	else D3D::stateman->PushBlendState(clearblendstates[3]);

	// TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) D3D::stateman->PushDepthState(cleardepthstates[0]);
	else */if (zEnable) D3D::stateman->PushDepthState(cleardepthstates[1]);
	else /*if (!zEnable)*/ D3D::stateman->PushDepthState(cleardepthstates[2]);

	//To Do: Not needed?
	//D3D::context->VSSetShader(VertexShaderCache::GetClearVertexShader(), nullptr, 0);
	//D3D::context->PSSetShader(PixelShaderCache::GetClearProgram(), nullptr, 0);
	//D3D::context->IASetInputLayout(VertexShaderCache::GetClearInputLayout());

	D3D::stateman->Apply();

	D3D::stateman->PopDepthState();
	D3D::stateman->PopBlendState();

	RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// TODO: MSAA support..
	D3D11_RECT source = CD3D11_RECT(0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	ID3D11PixelShader* pixel_shader;
	if (convtype == 0) pixel_shader = PixelShaderCache::ReinterpRGB8ToRGBA6(true);
	else if (convtype == 2) pixel_shader = PixelShaderCache::ReinterpRGBA6ToRGB8(true);
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	// convert data and set the target texture as our new EFB
	g_renderer->ResetAPIState();

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)g_renderer->GetTargetWidth(), (float)g_renderer->GetTargetHeight());
	D3D::context->RSSetViewports(1, &vp);

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV(), nullptr);
	D3D::SetPointCopySampler();
	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture()->GetSRV(), &source, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		pixel_shader, VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader());

	g_renderer->RestoreAPIState();

	FramebufferManager::SwapReinterpretTexture();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const D3D11_BLEND d3dSrcFactors[8] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_DEST_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_SRC_ALPHA,
		D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_ONE,
		(target_has_alpha) ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_ZERO
	};
	const D3D11_BLEND d3dDestFactors[8] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_SRC_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_SRC_ALPHA,
		D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_ONE,
		(target_has_alpha) ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !forceUpdate)
		return;

	if (bpmem.blendmode.subtract)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = D3D11_BLEND_OP_REV_SUBTRACT;
		gx_state.blend.src_blend = D3D11_BLEND_ONE;
		gx_state.blend.dst_blend = D3D11_BLEND_ONE;
	}
	else
	{
		gx_state.blend.blend_enable = (u32)bpmem.blendmode.blendenable;
		if (bpmem.blendmode.blendenable)
		{
			gx_state.blend.blend_op = D3D11_BLEND_OP_ADD;
			gx_state.blend.src_blend = d3dSrcFactors[bpmem.blendmode.srcfactor];
			gx_state.blend.dst_blend = d3dDestFactors[bpmem.blendmode.dstfactor];
		}
	}
}

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle& rc)
{
	if (!s_screenshot_texture)
		CreateScreenshotTexture(rc);

	// copy back buffer to system memory
	D3D11_BOX box = CD3D11_BOX(rc.left, rc.top, 0, rc.right, rc.bottom, 1);
	D3D::context->CopySubresourceRegion(s_screenshot_texture, 0, 0, 0, 0, (ID3D11Resource*)D3D::GetBackBuffer()->GetTex(), 0, &box);

	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(s_screenshot_texture, 0, D3D11_MAP_READ_WRITE, 0, &map);

	bool saved_png = TextureToPng((u8*)map.pData, map.RowPitch, filename, rc.GetWidth(), rc.GetHeight(), false);

	D3D::context->Unmap(s_screenshot_texture, 0);


	if (saved_png)
	{
		OSD::AddMessage(StringFromFormat("Saved %i x %i %s", rc.GetWidth(),
		                                 rc.GetHeight(), filename.c_str()));
	}
	else
	{
		OSD::AddMessage(StringFromFormat("Error saving %s", filename.c_str()));
	}

	return saved_png;
}

void formatBufferDump(const u8* in, u8* out, int w, int h, int p)
{
	for (int y = 0; y < h; ++y)
	{
		auto line = (in + (h - y - 1) * p);
		for (int x = 0; x < w; ++x)
		{
			out[0] = line[2];
			out[1] = line[1];
			out[2] = line[0];
			out += 3;
			line += 4;
		}
	}
}

void Renderer::AsyncTimewarpDraw()
{
	//TODO: D3D11 Asynchronous Timewarp
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	//rafa
	if (ARBruteForcer::ch_bruteforce)
		ARBruteForcer::ch_last_search = true;

	// VR - before the first frame we need BeginFrame, and we need to configure the tracking
	if (g_first_rift_frame && g_has_hmd && g_ActiveConfig.bEnableVR)
	{
		if (!g_ActiveConfig.bAsynchronousTimewarp)
		{
			VR_BeginFrame();
			VR_GetEyePoses();
		}
		g_first_rift_frame = false;

		VR_ConfigureHMDTracking();
	}

	// With frame skipping (or if the GC/Wii didn't draw anything), return without drawing anything
	// but if we are recording an .avi file, save the frame to disk again to maintain the right frame rate.
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	// Check what XFB we are supposed to draw
	// If we are using virtual XFB, but there is no XFB, then count it as an empty frame and exit
	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbStride, fbHeight, &xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	ResetAPIState();

	// Prepare to copy the XFBs to our backbuffer
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	TargetRectangle targetRc = GetTargetRectangle();

	// TODO: Do we still need to set and clear the backbuffer like this in VR mode with the Oculus Rift?
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);

	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), ClearColor);

	// activate linear filtering for the buffer copies
	D3D::SetLinearCopySampler();

	if (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB)
	{
		// TODO: Television should be used to render Virtual XFB mode as well.
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight());
		D3D::context->RSSetViewports(1, &vp);

		s_television.Submit(xfbAddr, fbStride, fbWidth, fbHeight);
		s_television.Render();
	}
	else if (g_has_hmd && g_ActiveConfig.bEnableVR)
	{
		// Draw our Razer Hydra
		if (!g_ActiveConfig.bUseXFB)
			DX11::s_avatarDrawer.Draw();

		EFBRectangle sourceRc;
		// In VR we use the whole EFB instead of just the bpmem.copyTexSrc rectangle passed to this function. 
		sourceRc.left = 0;
		sourceRc.right = EFB_WIDTH;
		sourceRc.top = 0;
		sourceRc.bottom = EFB_HEIGHT;
		TargetRectangle targetRc = ConvertEFBRectangle(sourceRc);

		if (g_ActiveConfig.bUseXFB)
		{
			const XFBSource* xfbSource;

			// draw each xfb source
			for (u32 i = 0; i < xfbCount; ++i)
			{
				xfbSource = (const XFBSource*)xfbSourceList[i];

				TargetRectangle drawRc;

				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

				drawRc.top = targetRc.top + hOffset * targetRc.GetHeight() / (s32)fbHeight;
				drawRc.bottom = targetRc.top + (hOffset + xfbHeight) * targetRc.GetHeight() / (s32)fbHeight;
				drawRc.left = targetRc.left + (targetRc.GetWidth() - xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;
				drawRc.right = targetRc.left + (targetRc.GetWidth() + xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;

				TargetRectangle sourceRc;
				sourceRc.left = 0;
				sourceRc.top = 0;
				sourceRc.right = (int)xfbSource->texWidth;
				sourceRc.bottom = (int)xfbSource->texHeight;

				sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);

				D3DTexture2D* read_texture = xfbSource->tex;

				D3D11_VIEWPORT Vp = CD3D11_VIEWPORT((float)drawRc.left, (float)drawRc.top, (float)drawRc.GetWidth(), (float)drawRc.GetHeight());

				// Render to left eye
				VR_RenderToEyebuffer(0);
				D3D::context->RSSetViewports(1, &Vp);
				D3D::drawShadedTexQuad(read_texture->GetSRV(), sourceRc.AsRECT(), xfbSource->texWidth, xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

				// Render to right eye
				VR_RenderToEyebuffer(1);
				D3D::drawShadedTexQuad(read_texture->GetSRV(), sourceRc.AsRECT(), xfbSource->texWidth, xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
			}
		}
		else
		{
			// TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB texture as source
			D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();

			D3D11_VIEWPORT Vp = CD3D11_VIEWPORT((float)0, (float)0, (float)Renderer::GetTargetWidth(), (float)Renderer::GetTargetHeight());

			// Render to left eye
			VR_RenderToEyebuffer(0);
			D3D::context->RSSetViewports(1, &Vp);
			D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetTargetWidth(), Renderer::GetTargetHeight(), PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

			// Render to right eye
			VR_RenderToEyebuffer(1);
			D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetTargetWidth(), Renderer::GetTargetHeight(), PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
		}
		// Reset viewport for drawing text
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(400.0f, 400.0f, Renderer::GetTargetWidth() - 400.0f, Renderer::GetTargetHeight() - 400.0f);
		D3D::context->RSSetViewports(1, &vp);
		Renderer::DrawDebugText();
		OSD::DrawMessages();

		if (!g_ActiveConfig.bAsynchronousTimewarp)
		{
			VR_PresentHMDFrame();
			g_drawn_vr++;

			// VR Synchronous Timewarp
			static int real_frame_count_for_timewarp = 0;
			SConfig& startup_parameter = SConfig::GetInstance();

			if (g_ActiveConfig.bSynchronousTimewarp)
			{
				if ((startup_parameter.bSkipIdle && startup_parameter.bSyncGPUOnSkipIdleHack) ||
					startup_parameter.bSyncGPU || startup_parameter.m_GPUDeterminismMode == GPU_DETERMINISM_FAKE_COMPLETION ||
					!startup_parameter.bCPUThread || SConfig::GetInstance().m_Framelimit == 13)
					SConfig::GetInstance().m_AudioSlowDown = 1.00;
				else
					SConfig::GetInstance().m_AudioSlowDown = 1.25;

				if (g_ActiveConfig.bPullUp20fpsTimewarp)
				{
					if (real_frame_count_for_timewarp % 4 == 1)
						g_ActiveConfig.iExtraTimewarpedFrames = 2;
					else
						g_ActiveConfig.iExtraTimewarpedFrames = 3;
				}
				else if (g_ActiveConfig.bPullUp30fpsTimewarp)
				{
					if (real_frame_count_for_timewarp % 2 == 1)
						g_ActiveConfig.iExtraTimewarpedFrames = 1;
					else
						g_ActiveConfig.iExtraTimewarpedFrames = 2;
				}
				else if (g_ActiveConfig.bPullUp60fpsTimewarp)
				{
					if (real_frame_count_for_timewarp % 4 == 0)
						g_ActiveConfig.iExtraTimewarpedFrames = 1;
					else
						g_ActiveConfig.iExtraTimewarpedFrames = 0;
				}
				++real_frame_count_for_timewarp;
			}
			else if (g_opcode_replay_enabled)
			{
				if ((startup_parameter.bSkipIdle && startup_parameter.bSyncGPUOnSkipIdleHack) ||
					startup_parameter.bSyncGPU || startup_parameter.m_GPUDeterminismMode == GPU_DETERMINISM_FAKE_COMPLETION ||
					!startup_parameter.bCPUThread || SConfig::GetInstance().m_Framelimit == 13)
					SConfig::GetInstance().m_AudioSlowDown = 1.00;
				else
					SConfig::GetInstance().m_AudioSlowDown = 1.25;
				g_ActiveConfig.iExtraTimewarpedFrames = 0;
			}
			else
			{
				SConfig::GetInstance().m_AudioSlowDown = startup_parameter.fAudioSlowDown;
			}

			// If 30fps loop once, if 20fps (Zelda: OoT for instance) loop twice.
			for (int i = 0; i < (int)g_ActiveConfig.iExtraTimewarpedFrames; ++i)
			{
				VR_DrawTimewarpFrame();
				g_drawn_vr++;
			}

		}
		else
		{
			// VR TODO - Direct3D Asynchronous timewarp
		}
	}
	else if (g_ActiveConfig.bUseXFB)
	{
		const XFBSource* xfbSource;

		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = (const XFBSource*)xfbSourceList[i];

			TargetRectangle drawRc;

			// use virtual xfb with offset
			int xfbHeight = xfbSource->srcHeight;
			int xfbWidth = xfbSource->srcWidth;
			int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

			drawRc.top = targetRc.top + hOffset * targetRc.GetHeight() / (s32)fbHeight;
			drawRc.bottom = targetRc.top + (hOffset + xfbHeight) * targetRc.GetHeight() / (s32)fbHeight;
			drawRc.left = targetRc.left + (targetRc.GetWidth() - xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;
			drawRc.right = targetRc.left + (targetRc.GetWidth() + xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;

			// The following code disables auto stretch.  Kept for reference.
			// scale draw area for a 1 to 1 pixel mapping with the draw target
			//float vScale = (float)fbHeight / (float)s_backbuffer_height;
			//float hScale = (float)fbWidth / (float)s_backbuffer_width;
			//drawRc.top *= vScale;
			//drawRc.bottom *= vScale;
			//drawRc.left *= hScale;
			//drawRc.right *= hScale;

			TargetRectangle sourceRc;
			sourceRc.left = 0;
			sourceRc.top = 0;
			sourceRc.right = (int)xfbSource->texWidth;
			sourceRc.bottom = (int)xfbSource->texHeight;

			sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);

			BlitScreen(sourceRc, drawRc, xfbSource->tex, xfbSource->texWidth, xfbSource->texHeight, Gamma);
		}
	}
	else
	{
		TargetRectangle sourceRc = Renderer::ConvertEFBRectangle(rc);

		// TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB texture as source
		D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();
		BlitScreen(sourceRc, targetRc, read_texture, GetTargetWidth(), GetTargetHeight(), Gamma);
	}

	// Enable screenshot and write csv if bruteforcing is on
	if (ARBruteForcer::ch_bruteforce && ARBruteForcer::ch_take_screenshot > 0)
		ARBruteForcer::SetupScreenshotAndWriteCSV(&s_bScreenshot, &s_sScreenshotName);

	// done with drawing the game stuff, good moment to save a screenshot
	if (s_bScreenshot && !g_ActiveConfig.bAsynchronousTimewarp)
	{
		SaveScreenshot(s_sScreenshotName, GetTargetRectangle());
		s_bScreenshot = false;
	}

	// Dump frames
	static int w = 0, h = 0;
	if (SConfig::GetInstance().m_DumpFrames && !g_ActiveConfig.bAsynchronousTimewarp)
	{
		static int s_recordWidth;
		static int s_recordHeight;

		if (!s_screenshot_texture)
			CreateScreenshotTexture(GetTargetRectangle());

		D3D11_BOX box = CD3D11_BOX(GetTargetRectangle().left, GetTargetRectangle().top, 0, GetTargetRectangle().right, GetTargetRectangle().bottom, 1);
		D3D::context->CopySubresourceRegion(s_screenshot_texture, 0, 0, 0, 0, (ID3D11Resource*)D3D::GetBackBuffer()->GetTex(), 0, &box);
		if (!bLastFrameDumped)
		{
			s_recordWidth = GetTargetRectangle().GetWidth();
			s_recordHeight = GetTargetRectangle().GetHeight();
			bAVIDumping = AVIDump::Start(D3D::hWnd, s_recordWidth, s_recordHeight);
			if (!bAVIDumping)
			{
				PanicAlert("Error dumping frames to AVI.");
			}
			else
			{
				std::string msg = StringFromFormat("Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
					File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_recordWidth, s_recordHeight);

				OSD::AddMessage(msg, 2000);
			}
		}
		if (bAVIDumping)
		{
			D3D11_MAPPED_SUBRESOURCE map;
			D3D::context->Map(s_screenshot_texture, 0, D3D11_MAP_READ, 0, &map);

			if (frame_data.empty() || w != s_recordWidth || h != s_recordHeight)
			{
				frame_data.resize(3 * s_recordWidth * s_recordHeight);
				w = s_recordWidth;
				h = s_recordHeight;
			}
			formatBufferDump((u8*)map.pData, &frame_data[0], s_recordWidth, s_recordHeight, map.RowPitch);
			AVIDump::AddFrame(&frame_data[0], GetTargetRectangle().GetWidth(), GetTargetRectangle().GetHeight());
			D3D::context->Unmap(s_screenshot_texture, 0);
		}
		bLastFrameDumped = true;
	}
	else
	{
		if (bLastFrameDumped && bAVIDumping)
		{
			std::vector<u8>().swap(frame_data);
			w = h = 0;

			AVIDump::Stop();
			bAVIDumping = false;
			OSD::AddMessage("Stop dumping frames to AVI", 2000);
		}
		bLastFrameDumped = false;
	}

	if (!g_has_hmd)
	{
		// Reset viewport for drawing text
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetBackbufferWidth(), (float)GetBackbufferHeight());
		D3D::context->RSSetViewports(1, &vp);

		Renderer::DrawDebugText();

		OSD::DrawMessages();
	}
	D3D::EndFrame();

	TextureCache::Cleanup(frameCount);

	if (g_has_hmd)
	{
		if (g_Config.bLowPersistence != g_ActiveConfig.bLowPersistence ||
			g_Config.bDynamicPrediction != g_ActiveConfig.bDynamicPrediction ||
			g_Config.bNoMirrorToWindow != g_ActiveConfig.bNoMirrorToWindow)
		{
			VR_ConfigureHMDPrediction();
		}

		if (g_Config.bOrientationTracking != g_ActiveConfig.bOrientationTracking ||
			g_Config.bMagYawCorrection != g_ActiveConfig.bMagYawCorrection ||
			g_Config.bPositionTracking != g_ActiveConfig.bPositionTracking)
		{
			VR_ConfigureHMDTracking();
		}

		if (g_Config.bChromatic != g_ActiveConfig.bChromatic ||
			g_Config.bTimewarp != g_ActiveConfig.bTimewarp ||
			g_Config.bVignette != g_ActiveConfig.bVignette ||
			g_Config.bNoRestore != g_ActiveConfig.bNoRestore ||
			g_Config.bFlipVertical != g_ActiveConfig.bFlipVertical ||
			g_Config.bSRGB != g_ActiveConfig.bSRGB ||
			g_Config.bOverdrive != g_ActiveConfig.bOverdrive ||
			g_Config.bHqDistortion != g_ActiveConfig.bHqDistortion ||
			g_fov_changed)
		{
			VR_ConfigureHMD();
		}
	}

	// VR layer debugging, sometimes layers need to flash.
	g_Config.iFlashState++;
	if (g_Config.iFlashState >= 10)
		g_Config.iFlashState = 0;

	// Enable configuration changes
	UpdateActiveConfig();
	// VR Real XFB isn't implemented yet, so always disable it for VR
	if (g_has_hmd && g_ActiveConfig.bEnableVR)
	{
		g_ActiveConfig.bUseRealXFB = false;
		// always stretch to fit
		g_ActiveConfig.iAspectRatio = 3;
	}
	TextureCache::OnConfigChanged(g_ActiveConfig);
	if (g_has_hmd && g_ActiveConfig.bEnableVR && !g_ActiveConfig.bAsynchronousTimewarp)
		VR_BeginFrame();

	SetWindowSize(fbStride, fbHeight);

	bool windowResized = CheckForResize();
	bool fullscreen = g_ActiveConfig.bFullscreen && g_ActiveConfig.ExclusiveFullscreenEnabled() &&
		!SConfig::GetInstance().bRenderToMain;

	bool xfbchanged = s_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

	if (FramebufferManagerBase::LastXfbWidth() != fbStride || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int xfb_w = (fbStride < 1 || fbStride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbStride;
		unsigned int xfb_h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(xfb_w);
		FramebufferManagerBase::SetLastXfbHeight(xfb_h);
	}

	// Flip/present backbuffer to frontbuffer here
	if (!g_has_hmd)
		D3D::Present();

	// Check exclusive fullscreen state
	bool exclusive_mode, fullscreen_changed = false;
	if (g_is_direct_mode)
	{
		windowResized = false;
		fullscreen = false;
		fullscreen_changed = false;
	}
	else if (SUCCEEDED(D3D::GetFullscreenState(&exclusive_mode)))
	{
		if (fullscreen && !exclusive_mode)
		{
			if (g_Config.bExclusiveMode)
				OSD::AddMessage("Lost exclusive fullscreen.");

			// Exclusive fullscreen is enabled in the configuration, but we're
			// not in exclusive mode. Either exclusive fullscreen was turned on
			// or the render frame lost focus. When the render frame is in focus
			// we can apply exclusive mode.
			fullscreen_changed = Host_RendererHasFocus();

			g_Config.bExclusiveMode = false;
		}
		else if (!fullscreen && exclusive_mode)
		{
			// Exclusive fullscreen is disabled, but we're still in exclusive mode.
			fullscreen_changed = true;
		}
	}

	NewVRFrame();

	// Resize the back buffers NOW to avoid flickering
	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height) ||
		xfbchanged ||
		windowResized ||
		fullscreen_changed ||
		s_last_efb_scale != g_ActiveConfig.iEFBScale ||
		s_last_multisample_mode != g_ActiveConfig.iMultisampleMode ||
		s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0))
	{
		s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
		s_last_multisample_mode = g_ActiveConfig.iMultisampleMode;
		PixelShaderCache::InvalidateMSAAShaders();

		if (windowResized || fullscreen_changed)
		{
			// Apply fullscreen state
			if (fullscreen_changed)
			{
				g_Config.bExclusiveMode = fullscreen;

				if (fullscreen)
					OSD::AddMessage("Entered exclusive fullscreen.");

				D3D::SetFullscreenState(fullscreen);

				// If fullscreen is disabled we can safely notify the UI to exit fullscreen.
				if (!g_ActiveConfig.bFullscreen)
					Host_RequestFullscreen(false);
			}

			// TODO: Aren't we still holding a reference to the back buffer right now?
			D3D::Reset();
			SAFE_RELEASE(s_screenshot_texture);
			SAFE_RELEASE(s_3d_vision_texture);
			s_backbuffer_width = D3D::GetBackBufferWidth();
			s_backbuffer_height = D3D::GetBackBufferHeight();
		}

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		s_last_efb_scale = g_ActiveConfig.iEFBScale;
		s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;

		PixelShaderManager::SetEfbScaleChanged();

		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);

		if (g_ActiveConfig.bAsynchronousTimewarp)
			g_vr_lock.lock();
		delete g_framebuffer_manager;
		g_framebuffer_manager = new FramebufferManager;
		float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };
		D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_col);
		D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 0.f, 0);
		if (g_ActiveConfig.bAsynchronousTimewarp)
			g_vr_lock.unlock();
	}
	else if (g_has_hmd && !g_ActiveConfig.bDontClearScreen)
	{
		// cegli - clearing the screen here causes flickering in games that fake 60fps by only actually updating
		// the entire screen once every 2 frames.  They rely on the fact that nothing is cleared on the fake frame.
		// An example of this is Beyond Good and Evil. Removing it aligns D3D with OGL, but adds the same smearing
		// problem OGL has in the BG&E menu.  Without clearing the screen, some games like PM: TTYD have smearing
		// around the edges.  How does OGL handle this gracefully?
		// To Do: Figure out the best thing to do here.

		// VR Clear screen before every frame
		float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };
		D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_col);
		D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 0.f, 0);
	}

	// begin next frame
	RestoreAPIState();
	D3D::BeginFrame();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	SetViewport();
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::stateman->PushBlendState(resetblendstate);
	D3D::stateman->PushDepthState(resetdepthstate);
	D3D::stateman->PushRasterizerState(resetraststate);
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
	SetViewport();
	BPFunctions::SetScissor();
	gx_state.raster.depth_clip_enable = !g_ActiveConfig.bDisableNearClipping;
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
	gx_state.blend.use_dst_alpha = bUseDstAlpha;
	D3D::stateman->PushBlendState(gx_state_cache.Get(gx_state.blend));
	D3D::stateman->PushDepthState(gx_state_cache.Get(gx_state.zmode));
	D3D::stateman->PushRasterizerState(gx_state_cache.Get(gx_state.raster));

	for (unsigned int stage = 0; stage < 8; stage++)
	{
		// TODO: cache SamplerState directly, not d3d object
		gx_state.sampler[stage].max_anisotropy = 1 << g_ActiveConfig.iMaxAnisotropy;
		D3D::stateman->SetSampler(stage, gx_state_cache.Get(gx_state.sampler[stage]));
	}

	if (bUseDstAlpha)
	{
		// restore actual state
		SetBlendMode(false);
		SetLogicOpMode();
	}

	ID3D11Buffer* vertexConstants = VertexShaderCache::GetConstantBuffer();

	D3D::stateman->SetPixelConstants(PixelShaderCache::GetConstantBuffer(), g_ActiveConfig.bEnablePixelLighting ? vertexConstants : nullptr);
	D3D::stateman->SetVertexConstants(vertexConstants);
	D3D::stateman->SetGeometryConstants(GeometryShaderCache::GetConstantBuffer());

	D3D::stateman->SetPixelShader(PixelShaderCache::GetActiveShader());
	D3D::stateman->SetVertexShader(VertexShaderCache::GetActiveShader());
	D3D::stateman->SetGeometryShader(GeometryShaderCache::GetActiveShader());
}

void Renderer::RestoreState()
{
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
}

void Renderer::ApplyCullDisable()
{
	RasterizerState rast = gx_state.raster;
	rast.cull_mode = D3D11_CULL_NONE;

	ID3D11RasterizerState* raststate = gx_state_cache.Get(rast);
	D3D::stateman->PushRasterizerState(raststate);
}

void Renderer::RestoreCull()
{
	D3D::stateman->PopRasterizerState();
}

void Renderer::SetGenerationMode()
{
	const D3D11_CULL_MODE d3dCullModes[4] =
	{
		D3D11_CULL_NONE,
		D3D11_CULL_BACK,
		D3D11_CULL_FRONT,
		D3D11_CULL_BACK
	};

	// rastdc.FrontCounterClockwise must be false for this to work
	// TODO: GX_CULL_ALL not supported, yet!
	gx_state.raster.cull_mode = d3dCullModes[bpmem.genMode.cullmode];
}

void Renderer::SetDepthMode()
{
	gx_state.zmode = bpmem.zmode;
}

void Renderer::SetLogicOpMode()
{
	// D3D11 doesn't support logic blending, so this is a huge hack
	// TODO: Make use of D3D11.1's logic blending support

	// 0   0x00
	// 1   Source & destination
	// 2   Source & ~destination
	// 3   Source
	// 4   ~Source & destination
	// 5   Destination
	// 6   Source ^ destination =  Source & ~destination | ~Source & destination
	// 7   Source | destination
	// 8   ~(Source | destination)
	// 9   ~(Source ^ destination) = ~Source & ~destination | Source & destination
	// 10  ~Destination
	// 11  Source | ~destination
	// 12  ~Source
	// 13  ~Source | destination
	// 14  ~(Source & destination)
	// 15  0xff
	const D3D11_BLEND_OP d3dLogicOps[16] =
	{
		D3D11_BLEND_OP_ADD,//0
		D3D11_BLEND_OP_ADD,//1
		D3D11_BLEND_OP_SUBTRACT,//2
		D3D11_BLEND_OP_ADD,//3
		D3D11_BLEND_OP_REV_SUBTRACT,//4
		D3D11_BLEND_OP_ADD,//5
		D3D11_BLEND_OP_MAX,//6
		D3D11_BLEND_OP_ADD,//7
		D3D11_BLEND_OP_MAX,//8
		D3D11_BLEND_OP_MAX,//9
		D3D11_BLEND_OP_ADD,//10
		D3D11_BLEND_OP_ADD,//11
		D3D11_BLEND_OP_ADD,//12
		D3D11_BLEND_OP_ADD,//13
		D3D11_BLEND_OP_ADD,//14
		D3D11_BLEND_OP_ADD//15
	};
	const D3D11_BLEND d3dLogicOpSrcFactors[16] =
	{
		D3D11_BLEND_ZERO,//0
		D3D11_BLEND_DEST_COLOR,//1
		D3D11_BLEND_ONE,//2
		D3D11_BLEND_ONE,//3
		D3D11_BLEND_DEST_COLOR,//4
		D3D11_BLEND_ZERO,//5
		D3D11_BLEND_INV_DEST_COLOR,//6
		D3D11_BLEND_INV_DEST_COLOR,//7
		D3D11_BLEND_INV_SRC_COLOR,//8
		D3D11_BLEND_INV_SRC_COLOR,//9
		D3D11_BLEND_INV_DEST_COLOR,//10
		D3D11_BLEND_ONE,//11
		D3D11_BLEND_INV_SRC_COLOR,//12
		D3D11_BLEND_INV_SRC_COLOR,//13
		D3D11_BLEND_INV_DEST_COLOR,//14
		D3D11_BLEND_ONE//15
	};
	const D3D11_BLEND d3dLogicOpDestFactors[16] =
	{
		D3D11_BLEND_ZERO,//0
		D3D11_BLEND_ZERO,//1
		D3D11_BLEND_INV_SRC_COLOR,//2
		D3D11_BLEND_ZERO,//3
		D3D11_BLEND_ONE,//4
		D3D11_BLEND_ONE,//5
		D3D11_BLEND_INV_SRC_COLOR,//6
		D3D11_BLEND_ONE,//7
		D3D11_BLEND_INV_DEST_COLOR,//8
		D3D11_BLEND_SRC_COLOR,//9
		D3D11_BLEND_INV_DEST_COLOR,//10
		D3D11_BLEND_INV_DEST_COLOR,//11
		D3D11_BLEND_INV_SRC_COLOR,//12
		D3D11_BLEND_ONE,//13
		D3D11_BLEND_INV_SRC_COLOR,//14
		D3D11_BLEND_ONE//15
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = d3dLogicOps[bpmem.blendmode.logicmode];
		gx_state.blend.src_blend = d3dLogicOpSrcFactors[bpmem.blendmode.logicmode];
		gx_state.blend.dst_blend = d3dLogicOpDestFactors[bpmem.blendmode.logicmode];
	}
	else
	{
		SetBlendMode(true);
	}
}

void Renderer::SetDitherMode()
{
	// TODO: Set dither mode to bpmem.blendmode.dither
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];

	if (texindex)
		stage += 4;

	if (g_ActiveConfig.bForceFiltering)
	{
		gx_state.sampler[stage].min_filter = 6; // 4 (linear mip) | 2 (linear min)
		gx_state.sampler[stage].mag_filter = 1; // linear mag
	}
	else
	{
		gx_state.sampler[stage].min_filter = (u32)tm0.min_filter;
		gx_state.sampler[stage].mag_filter = (u32)tm0.mag_filter;
	}

	gx_state.sampler[stage].wrap_s = (u32)tm0.wrap_s;
	gx_state.sampler[stage].wrap_t = (u32)tm0.wrap_t;
	gx_state.sampler[stage].max_lod = (u32)tm1.max_lod;
	gx_state.sampler[stage].min_lod = (u32)tm1.min_lod;
	gx_state.sampler[stage].lod_bias = (s32)tm0.lod_bias;

	// custom textures may have higher resolution, so disable the max_lod
	if (custom_tex)
	{
		gx_state.sampler[stage].max_lod = 255;
	}
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

int Renderer::GetMaxTextureSize()
{
	return DX11::D3D::GetMaxTextureSize();
}

u16 Renderer::BBoxRead(int index)
{
	// Here we get the min/max value of the truncated position of the upscaled framebuffer.
	// So we have to correct them to the unscaled EFB sizes.
	int value = BBox::Get(index);

	if (index < 2)
	{
		// left/right
		value = value * EFB_WIDTH / s_target_width;
	}
	else
	{
		// up/down
		value = value * EFB_HEIGHT / s_target_height;
	}
	if (index & 1)
		value++; // fix max values to describe the outer border

	return value;
}

void Renderer::BBoxWrite(int index, u16 _value)
{
	int value = _value; // u16 isn't enough to multiply by the efb width
	if (index & 1)
		value--;
	if (index < 2)
	{
		value = value * s_target_width / EFB_WIDTH;
	}
	else
	{
		value = value * s_target_height / EFB_HEIGHT;
	}

	BBox::Set(index, value);
}

void Renderer::BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture, u32 src_width, u32 src_height, float Gamma)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle leftRc, rightRc;
		ConvertStereoRectangle(dst, leftRc, rightRc);

		D3D11_VIEWPORT leftVp = CD3D11_VIEWPORT((float)leftRc.left, (float)leftRc.top, (float)leftRc.GetWidth(), (float)leftRc.GetHeight());
		D3D11_VIEWPORT rightVp = CD3D11_VIEWPORT((float)rightRc.left, (float)rightRc.top, (float)rightRc.GetWidth(), (float)rightRc.GetHeight());

		D3D::context->RSSetViewports(1, &leftVp);
		D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

		D3D::context->RSSetViewports(1, &rightVp);
		D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
	}
	else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		if (!s_3d_vision_texture)
			Create3DVisionTexture(s_backbuffer_width, s_backbuffer_height);

		D3D11_VIEWPORT leftVp = CD3D11_VIEWPORT((float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight());
		D3D11_VIEWPORT rightVp = CD3D11_VIEWPORT((float)(dst.left + s_backbuffer_width), (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight());

		// Render to staging texture which is double the width of the backbuffer
		D3D::context->OMSetRenderTargets(1, &s_3d_vision_texture->GetRTV(), nullptr);

		D3D::context->RSSetViewports(1, &leftVp);
		D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

		D3D::context->RSSetViewports(1, &rightVp);
		D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);

		// Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
		// recognize the signature and automatically include the right eye frame.
		D3D11_BOX box = CD3D11_BOX(0, 0, 0, s_backbuffer_width, s_backbuffer_height, 1);
		D3D::context->CopySubresourceRegion(D3D::GetBackBuffer()->GetTex(), 0, 0, 0, 0, s_3d_vision_texture->GetTex(), 0, &box);

		// Restore render target to backbuffer
		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
	}
	else
	{
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight());
		D3D::context->RSSetViewports(1, &vp);
		D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, (g_Config.iStereoMode == STEREO_ANAGLYPH) ? PixelShaderCache::GetAnaglyphProgram() : PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma);
	}
}

}  // namespace DX11
