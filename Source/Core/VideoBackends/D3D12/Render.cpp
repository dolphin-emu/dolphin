// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <memory>
#include <string>
#include <strsafe.h>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/D3D12/BoundingBox.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/ShaderCache.h"
#include "VideoBackends/D3D12/ShaderConstantsManager.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/Television.h"
#include "VideoBackends/D3D12/TextureCache.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static u32 s_last_multisamples = 1;
static bool s_last_stereo_mode = false;
static bool s_last_xfb_mode = false;

static Television s_television;

static ID3D12Resource* s_access_efb_constant_buffer = nullptr;

enum CLEAR_BLEND_DESC
{
	CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED = 0,
	CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED = 1,
	CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED = 2,
	CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED = 3
};

static D3D12_BLEND_DESC s_clear_blend_descs[4] = {};

enum CLEAR_DEPTH_DESC
{
	CLEAR_DEPTH_DESC_DEPTH_DISABLED = 0,
	CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED = 1,
	CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED = 2,
};

static D3D12_DEPTH_STENCIL_DESC s_clear_depth_descs[3] = {};

// These are accessed in D3DUtil.
D3D12_BLEND_DESC g_reset_blend_desc = {};
D3D12_DEPTH_STENCIL_DESC g_reset_depth_desc = {};
D3D12_RASTERIZER_DESC g_reset_rast_desc = {};

static ID3D12Resource* s_screenshot_texture = nullptr;
static void* s_screenshot_texture_data = nullptr;

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

static void SetupDeviceObjects()
{
	s_television.Init();

	g_framebuffer_manager = std::make_unique<FramebufferManager>();

	float colmat[20] = { 0.0f };
	colmat[0] = colmat[5] = colmat[10] = 1.0f;

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(colmat)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&s_access_efb_constant_buffer)
			)
		);

	// Copy inital data to access_efb_cbuf12.
	void* access_efb_constant_buffer_data = nullptr;
	CheckHR(s_access_efb_constant_buffer->Map(0, nullptr, &access_efb_constant_buffer_data));
	memcpy(access_efb_constant_buffer_data, colmat, sizeof(colmat));

	D3D12_DEPTH_STENCIL_DESC depth_desc;
	depth_desc.DepthEnable      = FALSE;
	depth_desc.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth_desc.DepthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
	depth_desc.StencilEnable    = FALSE;
	depth_desc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_DISABLED] = depth_desc;

	depth_desc.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ALL;
	depth_desc.DepthEnable     = TRUE;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED] = depth_desc;

	depth_desc.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ZERO;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED] = depth_desc;

	D3D12_BLEND_DESC blend_desc;
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].LogicOpEnable = FALSE;
	blend_desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	g_reset_blend_desc = blend_desc;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED] = g_reset_blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN|D3D12_COLOR_WRITE_ENABLE_BLUE;
	s_clear_blend_descs[CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED] = blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED] = blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = 0;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED] = blend_desc;

	depth_desc.DepthEnable      = FALSE;
	depth_desc.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth_desc.DepthFunc        = D3D12_COMPARISON_FUNC_LESS;
	depth_desc.StencilEnable    = FALSE;
	depth_desc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	g_reset_depth_desc = depth_desc;

	D3D12_RASTERIZER_DESC rast_desc = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false, 0, 0.f, 0.f, false, false, false, 0, D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
	g_reset_rast_desc = rast_desc;

	s_screenshot_texture = nullptr;
	s_screenshot_texture_data = nullptr;
}

// Kill off all device objects
static void TeardownDeviceObjects()
{
	g_framebuffer_manager.reset();

	if (s_screenshot_texture)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_screenshot_texture);
		s_screenshot_texture = nullptr;
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_access_efb_constant_buffer);
	s_access_efb_constant_buffer = nullptr;

	s_television.Shutdown();

	gx_state_cache.Clear();
}

void CreateScreenshotTexture()
{
	// We can't render anything outside of the backbuffer anyway, so use the backbuffer size as the screenshot buffer size.
	// This texture is released to be recreated when the window is resized in Renderer::SwapImpl.

	const unsigned int screenshot_buffer_size =
		D3D::AlignValue(D3D::GetBackBufferWidth() * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) *
		D3D::GetBackBufferHeight();

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(screenshot_buffer_size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&s_screenshot_texture)
			)
		);

	CheckHR(s_screenshot_texture->Map(0, nullptr, &s_screenshot_texture_data));
}

static D3D12_BOX GetScreenshotSourceBox(const TargetRectangle& target_rc)
{
	// Since the screenshot buffer is copied back to the CPU, we can't access pixels that
	// fall outside the backbuffer bounds. Therefore, when crop is enabled and the target rect is
	// off-screen to the top/left, we clamp the origin at zero, as well as the bottom/right
	// coordinates at the backbuffer dimensions. This will result in a rectangle that can be
	// smaller than the backbuffer, but never larger.

	return CD3DX12_BOX(
		std::max(target_rc.left, 0),
		std::max(target_rc.top, 0),
		0,
		std::min(D3D::GetBackBufferWidth(), static_cast<unsigned int>(target_rc.right)),
		std::min(D3D::GetBackBufferHeight(), static_cast<unsigned int>(target_rc.bottom)),
		1);
}

static void Create3DVisionTexture(int width, int height)
{
	// D3D12TODO: 3D Vision not implemented on D3D12 backend.
}

Renderer::Renderer(void*& window_handle)
{
	if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		PanicAlert("3DVision not implemented on D3D12 backend.");
		return;
	}

	D3D::Create((HWND)window_handle);

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_last_multisamples = g_ActiveConfig.iMultisamples;
	s_last_efb_scale = g_ActiveConfig.iEFBScale;
	s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
	s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);
	PixelShaderManager::SetEfbScaleChanged();

	SetupDeviceObjects();

	// Setup GX pipeline state
	gx_state.blend.blend_enable = false;
	gx_state.blend.write_mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	gx_state.blend.src_blend = D3D12_BLEND_ONE;
	gx_state.blend.dst_blend = D3D12_BLEND_ZERO;
	gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
	gx_state.blend.use_dst_alpha = false;

	for (unsigned int k = 0; k < 8; k++)
	{
		gx_state.sampler[k].hex = 0;
	}

	gx_state.zmode.testenable = false;
	gx_state.zmode.updateenable = false;
	gx_state.zmode.func = ZMode::NEVER;

	gx_state.raster.cull_mode = D3D12_CULL_MODE_NONE;

	// Clear EFB textures
	float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV12(), clear_color, 0, nullptr);
	D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV12(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);

	D3D12_VIEWPORT vp = { 0.f, 0.f, static_cast<float>(s_target_width), static_cast<float>(s_target_height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp);

	// Already transitioned to appropriate states a few lines up for the clears.
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	D3D::BeginFrame();
}

Renderer::~Renderer()
{
	D3D::EndFrame();
	D3D::WaitForOutstandingRenderingToComplete();
	TeardownDeviceObjects();
	D3D::Close();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled(static_cast<float>(left + 1), static_cast<float>(top + 1), 20.f, 0.0f, color & 0xFF000000, text);
	D3D::font.DrawTextScaled(static_cast<float>(left), static_cast<float>(top), 20.f, 0.0f, color, text);
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
__declspec(noinline) bool Renderer::CheckForResize()
{
	RECT rc_window;
	GetClientRect(D3D::hWnd, &rc_window);
	int client_width = rc_window.right - rc_window.left;
	int client_height = rc_window.bottom - rc_window.top;

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
	TargetRectangle trc = ConvertEFBRectangle(rc);
	D3D::current_command_list->RSSetScissorRects(1, trc.AsRECT());
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	UINT8 color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			color_mask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	}
	gx_state.blend.write_mask = color_mask;

	D3D::command_list_mgr->m_dirty_pso = true;
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
	// EXISTINGD3D11TODO: This function currently is broken if anti-aliasing is enabled

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efb_pixel_rc;
	efb_pixel_rc.left = x;
	efb_pixel_rc.top = y;
	efb_pixel_rc.right = x + 1;
	efb_pixel_rc.bottom = y + 1;
	TargetRectangle target_pixel_rc = Renderer::ConvertEFBRectangle(efb_pixel_rc);

	// Take the mean of the resulting dimensions; TODO: Don't use the center pixel, compute the average color instead
	D3D12_RECT rect_to_lock;
	if (type == PEEK_COLOR || type == PEEK_Z)
	{
		rect_to_lock.left = (target_pixel_rc.left + target_pixel_rc.right) / 2;
		rect_to_lock.top = (target_pixel_rc.top + target_pixel_rc.bottom) / 2;
		rect_to_lock.right = rect_to_lock.left + 1;
		rect_to_lock.bottom = rect_to_lock.top + 1;
	}
	else
	{
		rect_to_lock.left = target_pixel_rc.left;
		rect_to_lock.right = target_pixel_rc.right;
		rect_to_lock.top = target_pixel_rc.top;
		rect_to_lock.bottom = target_pixel_rc.bottom;
	}

	if (type == PEEK_Z)
	{
		D3D::command_list_mgr->CPUAccessNotify();

		// depth buffers can only be completely CopySubresourceRegion'ed, so we're using DrawShadedTexQuad instead
		// D3D12TODO: Is above statement true on D3D12?
		D3D12_VIEWPORT vp12 = { 0.f, 0.f, 1.f, 1.f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, s_access_efb_constant_buffer->GetGPUVirtualAddress());
		D3D::command_list_mgr->m_dirty_ps_cbv = true;

		FramebufferManager::GetEFBDepthReadTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBDepthReadTexture()->GetRTV12(), FALSE, nullptr);

		D3D::SetPointCopySampler();

		D3D::DrawShadedTexQuad(
			FramebufferManager::GetEFBDepthTexture(),
			&rect_to_lock,
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			StaticShaderCache::GetColorCopyPixelShader(true),
			StaticShaderCache::GetSimpleVertexShader(),
			StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			D3D12_SHADER_BYTECODE(),
			1.0f,
			0,
			DXGI_FORMAT_R32_FLOAT,
			false,
			FramebufferManager::GetEFBDepthReadTexture()->GetMultisampled()
			);

		// copy to system memory
		D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, 1, 1, 1);
		ID3D12Resource* readback_buffer = FramebufferManager::GetEFBDepthStagingBuffer();

		D3D12_TEXTURE_COPY_LOCATION dst_location = {};
		dst_location.pResource = readback_buffer;
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_location.PlacedFootprint.Offset = 0;
		dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
		dst_location.PlacedFootprint.Footprint.Width = 1;
		dst_location.PlacedFootprint.Footprint.Height = 1;
		dst_location.PlacedFootprint.Footprint.Depth = 1;
		dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION src_location = {};
		src_location.pResource = FramebufferManager::GetEFBDepthReadTexture()->GetTex12();
		src_location.SubresourceIndex = 0;
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		FramebufferManager::GetEFBDepthReadTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

		// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
		D3D::command_list_mgr->ExecuteQueuedWork(true);

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

		// Restores proper viewport/scissor settings.
		g_renderer->RestoreAPIState();

		// read the data from system memory
		void* readback_buffer_data = nullptr;
		CheckHR(readback_buffer->Map(0, nullptr, &readback_buffer_data));

		// depth buffer is inverted in the d3d backend
		float val = 1.0f - reinterpret_cast<float*>(readback_buffer_data)[0];
		u32 ret = 0;

		if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 65536.0f), 0, 0xFFFF);
		}
		else
		{
			ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 16777216.0f), 0, 0xFFFFFF);
		}

		// EXISTINGD3D11TODO: in RE0 this value is often off by one in Video_DX9 (where this code is derived from), which causes lighting to disappear
		return ret;
	}
	else if (type == PEEK_COLOR)
	{
		D3D::command_list_mgr->CPUAccessNotify();

		ID3D12Resource* readback_buffer = FramebufferManager::GetEFBColorStagingBuffer();

		D3D12_BOX src_box = CD3DX12_BOX(rect_to_lock.left, rect_to_lock.top, 0, rect_to_lock.right, rect_to_lock.bottom, 1);

		D3D12_TEXTURE_COPY_LOCATION dst_location = {};
		dst_location.pResource = readback_buffer;
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_location.PlacedFootprint.Offset = 0;
		dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dst_location.PlacedFootprint.Footprint.Width = 1;
		dst_location.PlacedFootprint.Footprint.Height = 1;
		dst_location.PlacedFootprint.Footprint.Depth = 1;
		dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION src_location = {};
		src_location.pResource = FramebufferManager::GetResolvedEFBColorTexture()->GetTex12();
		src_location.SubresourceIndex = 0;
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		FramebufferManager::GetResolvedEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

		// read the data from system memory
		void* readback_buffer_data = nullptr;
		CheckHR(readback_buffer->Map(0, nullptr, &readback_buffer_data));

		u32 ret = reinterpret_cast<u32*>(readback_buffer_data)[0];

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

	return 0;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	D3D12_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(GetTargetWidth()), static_cast<float>(GetTargetHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };

	if (type == POKE_COLOR)
	{
		// In the D3D12 backend, the rt/db/viewport is passed into DrawEFBPokeQuads, and set there.
		D3D::DrawEFBPokeQuads(
			type,
			points,
			num_points,
			&g_reset_blend_desc,
			&g_reset_depth_desc,
			&vp,
			&FramebufferManager::GetEFBColorTexture()->GetRTV12(),
			nullptr,
			FramebufferManager::GetEFBColorTexture()->GetMultisampled()
			);
	}
	else // if (type == POKE_Z)
	{
		D3D::DrawEFBPokeQuads(
			type,
			points,
			num_points,
			&s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED],
			&s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED],
			&vp,
			&FramebufferManager::GetEFBColorTexture()->GetRTV12(),
			&FramebufferManager::GetEFBDepthTexture()->GetDSV12(),
			FramebufferManager::GetEFBColorTexture()->GetMultisampled()
			);
	}

	RestoreAPIState();
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

	int scissor_x_offset = bpmem.scissorOffset.x * 2;
	int scissor_y_offset = bpmem.scissorOffset.y * 2;

	float x = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissor_x_offset);
	float y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissor_y_offset);
	float width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (width < 0.0f)
	{
		x += width;
		width = -width;
	}
	if (height < 0.0f)
	{
		y += height;
		height = -height;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	x = (x >= 0.f) ? x : 0.f;
	y = (y >= 0.f) ? y : 0.f;
	width = (x + width <= GetTargetWidth()) ? width : (GetTargetWidth() - x);
	height = (y + height <= GetTargetHeight()) ? height : (GetTargetHeight() - y);

	D3D12_VIEWPORT vp = { x, y, width, height,
		1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f,
		1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777216.0f), 0.0f, 16777215.0f) / 16777216.0f };

	D3D::current_command_list->RSSetViewports(1, &vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable, u32 color, u32 z)
{
	D3D12_BLEND_DESC* blend_desc = nullptr;

	if (color_enable && alpha_enable) blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED];
	else if (color_enable) blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED];
	else if (alpha_enable) blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED];
	else blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED];

	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc = nullptr;

	// EXISTINGD3D11TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_DISABLED];
	else */if (z_enable) depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED];
	else /*if (!z_enable)*/ depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED];

	// Update the view port for clearing the picture
	TargetRectangle target_rc = Renderer::ConvertEFBRectangle(rc);

	D3D12_VIEWPORT vp = {
		static_cast<float>(target_rc.left),
		static_cast<float>(target_rc.top),
		static_cast<float>(target_rc.GetWidth()),
		static_cast<float>(target_rc.GetHeight()),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};

	D3D::current_command_list->RSSetViewports(1, &vp);

	// Color is passed in bgra mode so we need to convert it to rgba
	u32 rgba_color = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	D3D::DrawClearQuad(rgba_color, 1.0f - (z & 0xFFFFFF) / 16777216.0f, blend_desc, depth_stencil_desc, FramebufferManager::GetEFBColorTexture()->GetMultisampled());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// EXISTINGD3D11TODO: MSAA support..
	D3D12_RECT source = CD3DX12_RECT(0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	D3D12_SHADER_BYTECODE pixel_shader = {};

	if (convtype == 0)
	{
		pixel_shader = StaticShaderCache::GetReinterpRGB8ToRGBA6PixelShader(true);
	}
	else if (convtype == 2)
	{
		pixel_shader = StaticShaderCache::GetReinterpRGBA6ToRGB8PixelShader(true);
	}
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	D3D12_VIEWPORT vp = {
		0.f,
		0.f,
		static_cast<float>(g_renderer->GetTargetWidth()),
		static_cast<float>(g_renderer->GetTargetHeight()),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};

	D3D::current_command_list->RSSetViewports(1, &vp);

	FramebufferManager::GetEFBColorTempTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV12(), FALSE, nullptr);

	D3D::SetPointCopySampler();
	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		&source,
		g_renderer->GetTargetWidth(),
		g_renderer->GetTargetHeight(),
		pixel_shader,
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0f,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		false,
		FramebufferManager::GetEFBColorTempTexture()->GetMultisampled()
		);

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();

	FramebufferManager::SwapReinterpretTexture();

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
}

void Renderer::SetBlendMode(bool force_update)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const D3D12_BLEND d3d_src_factors[8] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ONE,
		D3D12_BLEND_DEST_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_SRC_ALPHA,
		D3D12_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_ONE,
		(target_has_alpha) ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_ZERO
	};
	const D3D12_BLEND d3d_dst_factors[8] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ONE,
		D3D12_BLEND_SRC_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_SRC_ALPHA,
		D3D12_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_ONE,
		(target_has_alpha) ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !force_update)
		return;

	if (bpmem.blendmode.subtract)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = D3D12_BLEND_OP_REV_SUBTRACT;
		gx_state.blend.src_blend = D3D12_BLEND_ONE;
		gx_state.blend.dst_blend = D3D12_BLEND_ONE;
	}
	else
	{
		gx_state.blend.blend_enable = static_cast<u32>(bpmem.blendmode.blendenable);
		if (bpmem.blendmode.blendenable)
		{
			gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
			gx_state.blend.src_blend = d3d_src_factors[bpmem.blendmode.srcfactor];
			gx_state.blend.dst_blend = d3d_dst_factors[bpmem.blendmode.dstfactor];
		}
	}

	D3D::command_list_mgr->m_dirty_pso = true;
}

bool Renderer::SaveScreenshot(const std::string& filename, const TargetRectangle& rc)
{
	if (!s_screenshot_texture)
		CreateScreenshotTexture();

	// copy back buffer to system memory
	bool saved_png = false;

	D3D12_BOX source_box = GetScreenshotSourceBox(rc);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = s_screenshot_texture;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dst_location.PlacedFootprint.Footprint.Width = D3D::GetBackBufferWidth();
	dst_location.PlacedFootprint.Footprint.Height = D3D::GetBackBufferHeight();
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = 0;
	src_location.pResource = D3D::GetBackBuffer()->GetTex12();

	D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &source_box);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	saved_png = TextureToPng(static_cast<u8*>(s_screenshot_texture_data), dst_location.PlacedFootprint.Footprint.RowPitch, filename, source_box.right - source_box.left, source_box.bottom - source_box.top, false);

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

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, float gamma)
{
	if (Fifo::g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fb_width || !fb_height)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fb_width, fb_height);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	u32 xfb_count = 0;
	const XFBSourceBase* const* xfb_source_list = FramebufferManager::GetXFBSource(xfb_addr, fb_stride, fb_height, &xfb_count);
	if ((!xfb_source_list || xfb_count == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fb_width, fb_height);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	// Prepare to copy the XFBs to our backbuffer
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	TargetRectangle target_rc = GetTargetRectangle();

	D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);

	float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::current_command_list->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV12(), clear_color, 0, nullptr);

	// D3D12: Because scissor-testing is always enabled, change scissor rect to backbuffer in case EFB is smaller
	// than swap chain back buffer.
	D3D12_RECT back_buffer_rect = { 0L, 0L, GetBackbufferWidth(), GetBackbufferHeight() };
	D3D::current_command_list->RSSetScissorRects(1, &back_buffer_rect);

	// activate linear filtering for the buffer copies
	D3D::SetLinearCopySampler();

	if (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB)
	{
		// EXISTINGD3D11TODO: Television should be used to render Virtual XFB mode as well.
		D3D12_VIEWPORT vp12 = {
			static_cast<float>(target_rc.left),
			static_cast<float>(target_rc.top),
			static_cast<float>(target_rc.GetWidth()),
			static_cast<float>(target_rc.GetHeight()),
			D3D12_MIN_DEPTH,
			D3D12_MAX_DEPTH
		};

		D3D::current_command_list->RSSetViewports(1, &vp12);

		s_television.Submit(xfb_addr, fb_stride, fb_width, fb_height);
		s_television.Render();
	}
	else if (g_ActiveConfig.bUseXFB)
	{
		const XFBSource* xfb_source;

		// draw each xfb source
		for (u32 i = 0; i < xfb_count; ++i)
		{
			xfb_source = static_cast<const XFBSource*>(xfb_source_list[i]);

			TargetRectangle drawRc;

			// use virtual xfb with offset
			int xfb_height = xfb_source->srcHeight;
			int xfb_width = xfb_source->srcWidth;
			int hOffset = (static_cast<s32>(xfb_source->srcAddr) - static_cast<s32>(xfb_addr)) / (static_cast<s32>(fb_stride) * 2);

			drawRc.top = target_rc.top + hOffset * target_rc.GetHeight() / static_cast<s32>(fb_height);
			drawRc.bottom = target_rc.top + (hOffset + xfb_height) * target_rc.GetHeight() / static_cast<s32>(fb_height);
			drawRc.left = target_rc.left + (target_rc.GetWidth() - xfb_width * target_rc.GetWidth() / static_cast<s32>(fb_stride)) / 2;
			drawRc.right = target_rc.left + (target_rc.GetWidth() + xfb_width * target_rc.GetWidth() / static_cast<s32>(fb_stride)) / 2;

			// The following code disables auto stretch.  Kept for reference.
			// scale draw area for a 1 to 1 pixel mapping with the draw target
			//float vScale = static_cast<float>(fbHeight) / static_cast<float>(s_backbuffer_height);
			//float hScale = static_cast<float>(fbWidth) / static_cast<float>(s_backbuffer_width);
			//drawRc.top *= vScale;
			//drawRc.bottom *= vScale;
			//drawRc.left *= hScale;
			//drawRc.right *= hScale;

			TargetRectangle source_rc;
			source_rc.left = xfb_source->sourceRc.left;
			source_rc.top = xfb_source->sourceRc.top;
			source_rc.right = xfb_source->sourceRc.right;
			source_rc.bottom = xfb_source->sourceRc.bottom;

			source_rc.right -= Renderer::EFBToScaledX(fb_stride - fb_width);

			BlitScreen(source_rc, drawRc, xfb_source->m_tex, xfb_source->texWidth, xfb_source->texHeight, gamma);
		}
	}
	else
	{
		TargetRectangle source_rc = Renderer::ConvertEFBRectangle(rc);

		// EXISTINGD3D11TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB texture as source
		D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();

		BlitScreen(source_rc, target_rc, read_texture, GetTargetWidth(), GetTargetHeight(), gamma);
	}

	// done with drawing the game stuff, good moment to save a screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> guard(s_criticalScreenshot);

		SaveScreenshot(s_sScreenshotName, GetTargetRectangle());
		s_sScreenshotName.clear();
		s_bScreenshot = false;
		s_screenshotCompleted.Set();
	}

	// Dump frames
	static int w = 0, h = 0;
	if (SConfig::GetInstance().m_DumpFrames)
	{
		static unsigned int s_record_width;
		static unsigned int s_record_height;

		if (!s_screenshot_texture)
			CreateScreenshotTexture();

		D3D12_BOX source_box = GetScreenshotSourceBox(target_rc);

		unsigned int source_width = source_box.right - source_box.left;
		unsigned int source_height = source_box.bottom - source_box.top;

		D3D12_TEXTURE_COPY_LOCATION dst_location = {};
		dst_location.pResource = s_screenshot_texture;
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_location.PlacedFootprint.Offset = 0;
		dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dst_location.PlacedFootprint.Footprint.Width = GetTargetRectangle().GetWidth();
		dst_location.PlacedFootprint.Footprint.Height = GetTargetRectangle().GetHeight();
		dst_location.PlacedFootprint.Footprint.Depth = 1;
		dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION src_location = {};
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_location.SubresourceIndex = 0;
		src_location.pResource = D3D::GetBackBuffer()->GetTex12();

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &source_box);

		D3D::command_list_mgr->ExecuteQueuedWork(true);

		if (!bLastFrameDumped)
		{
			s_record_width = source_width;
			s_record_height = source_height;
			bAVIDumping = AVIDump::Start(s_record_width, s_record_height);
			if (!bAVIDumping)
			{
				PanicAlert("Error dumping frames to AVI.");
			}
			else
			{
				std::string msg = StringFromFormat("Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
					File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_record_width, s_record_height);

				OSD::AddMessage(msg, 2000);
			}
		}
		if (bAVIDumping)
		{
			if (frame_data.empty() || w != s_record_width || h != s_record_height)
			{
				frame_data.resize(3 * s_record_width * s_record_height);
				w = s_record_width;
				h = s_record_height;
			}
			formatBufferDump(static_cast<u8*>(s_screenshot_texture_data), &frame_data[0], source_width, source_height, dst_location.PlacedFootprint.Footprint.RowPitch);
			FlipImageData(&frame_data[0], w, h);
			AVIDump::AddFrame(&frame_data[0], source_width, source_height);
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

	// Reset viewport for drawing text
	D3D12_VIEWPORT vp = {
		0.0f,
		0.0f,
		static_cast<float>(GetBackbufferWidth()),
		static_cast<float>(GetBackbufferHeight()),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};

	D3D::current_command_list->RSSetViewports(1, &vp);

	Renderer::DrawDebugText();

	OSD::DrawMessages();
	D3D::EndFrame();

	TextureCacheBase::Cleanup(frameCount);

	// Enable configuration changes
	UpdateActiveConfig();
	TextureCacheBase::OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fb_stride, fb_height);

	const bool window_resized = CheckForResize();
	const bool fullscreen = g_ActiveConfig.bFullscreen && !g_ActiveConfig.bBorderlessFullscreen &&
		!SConfig::GetInstance().bRenderToMain;

	bool xfb_changed = s_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

	if (FramebufferManagerBase::LastXfbWidth() != fb_stride || FramebufferManagerBase::LastXfbHeight() != fb_height)
	{
		xfb_changed = true;
		unsigned int xfb_w = (fb_stride < 1 || fb_stride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fb_stride;
		unsigned int xfb_h = (fb_height < 1 || fb_height > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fb_height;
		FramebufferManagerBase::SetLastXfbWidth(xfb_w);
		FramebufferManagerBase::SetLastXfbHeight(xfb_h);
	}

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();

	// Resize the back buffers NOW to avoid flickering
	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height) ||
		xfb_changed ||
		window_resized ||
		s_last_efb_scale != g_ActiveConfig.iEFBScale ||
		s_last_multisamples != g_ActiveConfig.iMultisamples ||
		s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0))
	{
		s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
		s_last_multisamples = g_ActiveConfig.iMultisamples;

		StaticShaderCache::InvalidateMSAAShaders();

		if (window_resized)
		{
			// TODO: Aren't we still holding a reference to the back buffer right now?
			D3D::Reset();

			if (s_screenshot_texture)
			{
				D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_screenshot_texture);
				s_screenshot_texture = nullptr;
			}

			s_backbuffer_width = D3D::GetBackBufferWidth();
			s_backbuffer_height = D3D::GetBackBufferHeight();
		}

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		s_last_efb_scale = g_ActiveConfig.iEFBScale;
		s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;

		PixelShaderManager::SetEfbScaleChanged();

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);

		g_framebuffer_manager.reset();
		g_framebuffer_manager = std::make_unique<FramebufferManager>();
		const float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV12(), clear_color, 0, nullptr);

		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV12(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);
	}

	// begin next frame
	RestoreAPIState();
	D3D::BeginFrame();

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	SetViewport();
}

void Renderer::ResetAPIState()
{
	CHECK(0, "This should never be called.. just required for inheritance.");
}

void Renderer::RestoreAPIState()
{
	// Restores viewport/scissor rects, which might have been
	// overwritten elsewhere (particularly the viewport).
	SetViewport();
	BPFunctions::SetScissor();
}

static bool s_previous_use_dst_alpha = false;
static D3DVertexFormat* s_previous_vertex_format = nullptr;

void Renderer::ApplyState(bool use_dst_alpha)
{
	if (use_dst_alpha != s_previous_use_dst_alpha)
	{
		s_previous_use_dst_alpha = use_dst_alpha;
		D3D::command_list_mgr->m_dirty_pso = true;
	}

	gx_state.blend.use_dst_alpha = use_dst_alpha;

	if (D3D::command_list_mgr->m_dirty_samplers)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE sample_group_gpu_handle;
		sample_group_gpu_handle = D3D::sampler_descriptor_heap_mgr->GetHandleForSamplerGroup(gx_state.sampler, 8);

		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, sample_group_gpu_handle);

		D3D::command_list_mgr->m_dirty_samplers = false;
	}

	// Uploads and binds required constant buffer data for all stages.
	ShaderConstantsManager::LoadAndSetGeometryShaderConstants();
	ShaderConstantsManager::LoadAndSetPixelShaderConstants();
	ShaderConstantsManager::LoadAndSetVertexShaderConstants();

	if (D3D::command_list_mgr->m_dirty_pso || s_previous_vertex_format != reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat()))
	{
		s_previous_vertex_format = reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat());

		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = ShaderCache::GetCurrentPrimitiveTopology();
		RasterizerState modifiableRastState = gx_state.raster;

		if (topologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
		{
			modifiableRastState.cull_mode = D3D12_CULL_MODE_NONE;
		}

		SmallPsoDesc pso_desc = {
			ShaderCache::GetActiveGeometryShaderBytecode(), // D3D12_SHADER_BYTECODE GS;
			ShaderCache::GetActivePixelShaderBytecode(),    // D3D12_SHADER_BYTECODE PS;
			ShaderCache::GetActiveVertexShaderBytecode(),   // D3D12_SHADER_BYTECODE VS;
			s_previous_vertex_format,                       // D3DVertexFormat* InputLayout;
			gx_state.blend,                                 // BlendState BlendState;
			modifiableRastState,                            // RasterizerState RasterizerState;
			gx_state.zmode,                                 // ZMode DepthStencilState;
		};

		if (use_dst_alpha)
		{
			// restore actual state
			SetBlendMode(false);
			SetLogicOpMode();
		}

		ID3D12PipelineState* pso = nullptr;
		CheckHR(
			gx_state_cache.GetPipelineStateObjectFromCache(
				&pso_desc,
				&pso,
				topologyType,
				ShaderCache::GetActiveGeometryShaderUid(),
				ShaderCache::GetActivePixelShaderUid(),
				ShaderCache::GetActiveVertexShaderUid()
				)
			);

		D3D::current_command_list->SetPipelineState(pso);

		D3D::command_list_mgr->m_dirty_pso = false;
	}
}

void Renderer::RestoreState()
{
}

void Renderer::ApplyCullDisable()
{
	// This functionality is handled directly in ApplyState.
}

void Renderer::RestoreCull()
{
	// This functionality is handled directly in ApplyState.
}

void Renderer::SetGenerationMode()
{
	const D3D12_CULL_MODE d3d_cull_modes[4] =
	{
		D3D12_CULL_MODE_NONE,
		D3D12_CULL_MODE_BACK,
		D3D12_CULL_MODE_FRONT,
		D3D12_CULL_MODE_BACK
	};

	// rastdc.FrontCounterClockwise must be false for this to work
	// EXISTINGD3D11TODO: GX_CULL_ALL not supported, yet!
	gx_state.raster.cull_mode = d3d_cull_modes[bpmem.genMode.cullmode];

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetDepthMode()
{
	gx_state.zmode.hex = bpmem.zmode.hex;

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetLogicOpMode()
{
	// D3D11 doesn't support logic blending, so this is a huge hack
	// EXISTINGD3D11TODO: Make use of D3D11.1's logic blending support
	// D3D12TODO: Obviously these are always available in D3D12..

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
	const D3D12_BLEND_OP d3d_logic_ops[16] =
	{
		D3D12_BLEND_OP_ADD,//0
		D3D12_BLEND_OP_ADD,//1
		D3D12_BLEND_OP_SUBTRACT,//2
		D3D12_BLEND_OP_ADD,//3
		D3D12_BLEND_OP_REV_SUBTRACT,//4
		D3D12_BLEND_OP_ADD,//5
		D3D12_BLEND_OP_MAX,//6
		D3D12_BLEND_OP_ADD,//7
		D3D12_BLEND_OP_MAX,//8
		D3D12_BLEND_OP_MAX,//9
		D3D12_BLEND_OP_ADD,//10
		D3D12_BLEND_OP_ADD,//11
		D3D12_BLEND_OP_ADD,//12
		D3D12_BLEND_OP_ADD,//13
		D3D12_BLEND_OP_ADD,//14
		D3D12_BLEND_OP_ADD//15
	};
	const D3D12_BLEND d3d_logic_op_src_factors[16] =
	{
		D3D12_BLEND_ZERO,//0
		D3D12_BLEND_DEST_COLOR,//1
		D3D12_BLEND_ONE,//2
		D3D12_BLEND_ONE,//3
		D3D12_BLEND_DEST_COLOR,//4
		D3D12_BLEND_ZERO,//5
		D3D12_BLEND_INV_DEST_COLOR,//6
		D3D12_BLEND_INV_DEST_COLOR,//7
		D3D12_BLEND_INV_SRC_COLOR,//8
		D3D12_BLEND_INV_SRC_COLOR,//9
		D3D12_BLEND_INV_DEST_COLOR,//10
		D3D12_BLEND_ONE,//11
		D3D12_BLEND_INV_SRC_COLOR,//12
		D3D12_BLEND_INV_SRC_COLOR,//13
		D3D12_BLEND_INV_DEST_COLOR,//14
		D3D12_BLEND_ONE//15
	};
	const D3D12_BLEND d3d_logic_op_dest_factors[16] =
	{
		D3D12_BLEND_ZERO,//0
		D3D12_BLEND_ZERO,//1
		D3D12_BLEND_INV_SRC_COLOR,//2
		D3D12_BLEND_ZERO,//3
		D3D12_BLEND_ONE,//4
		D3D12_BLEND_ONE,//5
		D3D12_BLEND_INV_SRC_COLOR,//6
		D3D12_BLEND_ONE,//7
		D3D12_BLEND_INV_DEST_COLOR,//8
		D3D12_BLEND_SRC_COLOR,//9
		D3D12_BLEND_INV_DEST_COLOR,//10
		D3D12_BLEND_INV_DEST_COLOR,//11
		D3D12_BLEND_INV_SRC_COLOR,//12
		D3D12_BLEND_ONE,//13
		D3D12_BLEND_INV_SRC_COLOR,//14
		D3D12_BLEND_ONE//15
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = d3d_logic_ops[bpmem.blendmode.logicmode];
		gx_state.blend.src_blend = d3d_logic_op_src_factors[bpmem.blendmode.logicmode];
		gx_state.blend.dst_blend = d3d_logic_op_dest_factors[bpmem.blendmode.logicmode];
	}
	else
	{
		SetBlendMode(true);
	}

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetDitherMode()
{
	// EXISTINGD3D11TODO: Set dither mode to bpmem.blendmode.dither
}

void Renderer::SetSamplerState(int stage, int tex_index, bool custom_tex)
{
	SamplerState s_previous_sampler_state[8];

	const FourTexUnits& tex = bpmem.tex[tex_index];
	const TexMode0& tm0 = tex.texMode0[stage];
	const TexMode1& tm1 = tex.texMode1[stage];

	if (tex_index)
		stage += 4;

	if (g_ActiveConfig.bForceFiltering)
	{
		gx_state.sampler[stage].min_filter = 6; // 4 (linear mip) | 2 (linear min)
		gx_state.sampler[stage].mag_filter = 1; // linear mag
	}
	else
	{
		gx_state.sampler[stage].min_filter = static_cast<u32>(tm0.min_filter);
		gx_state.sampler[stage].mag_filter = static_cast<u32>(tm0.mag_filter);
	}

	gx_state.sampler[stage].wrap_s = static_cast<u32>(tm0.wrap_s);
	gx_state.sampler[stage].wrap_t = static_cast<u32>(tm0.wrap_t);
	gx_state.sampler[stage].max_lod = static_cast<u32>(tm1.max_lod);
	gx_state.sampler[stage].min_lod = static_cast<u32>(tm1.min_lod);
	gx_state.sampler[stage].lod_bias = static_cast<s32>(tm0.lod_bias);

	// custom textures may have higher resolution, so disable the max_lod
	if (custom_tex)
	{
		gx_state.sampler[stage].max_lod = 255;
	}

	if (gx_state.sampler[stage].hex != s_previous_sampler_state[stage].hex)
	{
		D3D::command_list_mgr->m_dirty_samplers = true;
		s_previous_sampler_state[stage].hex = gx_state.sampler[stage].hex;
	}
}

void Renderer::SetInterlacingMode()
{
	// EXISTINGD3D11TODO
}

int Renderer::GetMaxTextureSize()
{
	return DX12::D3D::GetMaxTextureSize();
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

void Renderer::BBoxWrite(int index, u16 value)
{
	int local_value = value; // u16 isn't enough to multiply by the efb width
	if (index & 1)
		local_value--;
	if (index < 2)
	{
		local_value = local_value * s_target_width / EFB_WIDTH;
	}
	else
	{
		local_value = local_value * s_target_height / EFB_HEIGHT;
	}

	BBox::Set(index, local_value);
}

void Renderer::BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture, u32 src_width, u32 src_height, float gamma)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle left_rc, right_rc;
		ConvertStereoRectangle(dst, left_rc, right_rc);

		D3D12_VIEWPORT left_vp = {
			static_cast<float>(left_rc.left),
			static_cast<float>(left_rc.top),
			static_cast<float>(left_rc.GetWidth()),
			static_cast<float>(left_rc.GetHeight()),
			D3D12_MIN_DEPTH,
			D3D12_MAX_DEPTH
		};

		D3D12_VIEWPORT right_vp = {
			static_cast<float>(right_rc.left),
			static_cast<float>(right_rc.top),
			static_cast<float>(right_rc.GetWidth()),
			static_cast<float>(right_rc.GetHeight()),
			D3D12_MIN_DEPTH,
			D3D12_MAX_DEPTH
		};

		// Swap chain backbuffer is never multisampled..

		D3D::current_command_list->RSSetViewports(1, &left_vp);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);

		D3D::current_command_list->RSSetViewports(1, &right_vp);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);
	}
	else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		// D3D12TODO
		// Not currently supported on D3D12 backend. Implemented (but untested) code kept for reference.

		//if (!s_3d_vision_texture)
		//	Create3DVisionTexture(s_backbuffer_width, s_backbuffer_height);

		//D3D12_VIEWPORT leftVp12 = { static_cast<float>(dst.left), static_cast<float>(dst.top), static_cast<float>(dst.GetWidth()), static_cast<float>(dst.GetHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		//D3D12_VIEWPORT rightVp12 = { static_cast<float>(dst.left + s_backbuffer_width), static_cast<float>(dst.top), static_cast<float>(dst.GetWidth()), static_cast<float>(dst.GetHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };

		//// Render to staging texture which is double the width of the backbuffer
		//s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//D3D::current_command_list->OMSetRenderTargets(1, &s_3d_vision_texture->GetRTV12(), FALSE, nullptr);

		//D3D::current_command_list->RSSetViewports(1, &leftVp12);
		//D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		//D3D::current_command_list->RSSetViewports(1, &rightVp12);
		//D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		//// Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
		//// recognize the signature and automatically include the right eye frame.
		//// D3D12TODO: Does this work on D3D12?

		//D3D12_BOX box = CD3DX12_BOX(0, 0, 0, s_backbuffer_width, s_backbuffer_height, 1);
		//D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(D3D::GetBackBuffer()->GetTex12(), 0);
		//D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(s_3d_vision_texture->GetTex12(), 0);

		//D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		//s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		//D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

		//// Restore render target to backbuffer
		//D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);
	}
	else
	{
		D3D12_VIEWPORT vp = { static_cast<float>(dst.left), static_cast<float>(dst.top), static_cast<float>(dst.GetWidth()), static_cast<float>(dst.GetHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp);

		D3D::DrawShadedTexQuad(
			src_texture,
			src.AsRECT(),
			src_width,
			src_height,
			(g_Config.iStereoMode == STEREO_ANAGLYPH) ? StaticShaderCache::GetAnaglyphPixelShader() : StaticShaderCache::GetColorCopyPixelShader(false),
			StaticShaderCache::GetSimpleVertexShader(),
			StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			D3D12_SHADER_BYTECODE(),
			gamma,
			0,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			false,
			false // Backbuffer never multisampled.
			);
	}
}

}  // namespace DX12
