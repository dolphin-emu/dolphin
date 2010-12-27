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

#include <list>

#include "StringUtil.h"
#include "Common.h"
#include "Atomic.h"
#include "FileUtil.h"
#include "Thread.h"
#include "Timer.h"
#include "Statistics.h"

#include "VideoConfig.h"
#include "main.h"
#include "VertexManager.h"
#include "PixelEngine.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "D3DUtil.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "VertexLoaderManager.h"
#include "TextureCache.h"
#include "EmuWindow.h"
#include "AVIDump.h"
#include "OnScreenDisplay.h"
#include "FramebufferManager.h"
#include "Fifo.h"
#include "DLCache.h"

#include "Debugger.h"

#include <strsafe.h>

static int s_fps = 0;

static u32 s_LastAA = 0;

static u32 s_blendMode;

ID3D11Buffer* access_efb_cbuf = NULL;
ID3D11BlendState* clearblendstates[4] = {NULL};
ID3D11DepthStencilState* cleardepthstates[3] = {NULL};
ID3D11BlendState* resetblendstate = NULL;
ID3D11DepthStencilState* resetdepthstate = NULL;
ID3D11RasterizerState* resetraststate = NULL;

bool reset_called = false;

// State translation lookup tables
static const D3D11_BLEND d3dSrcFactors[8] =
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA
};

static const D3D11_BLEND d3dDestFactors[8] =
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA
};

//		0	0x00
//		1	Source & destination
//		2	Source & ~destination
//		3	Source
//		4	~Source & destination
//		5	Destination
//		6	Source ^ destination =  Source & ~destination | ~Source & destination
//		7	Source | destination

//		8	~(Source | destination)
//		9	~(Source ^ destination) = ~Source & ~destination | Source & destination
//		10	~Destination
//		11	Source | ~destination
//		12	~Source
//		13	~Source | destination
//		14	~(Source & destination)
//		15	0xff

static const D3D11_BLEND_OP d3dLogicOps[16] =
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

static const D3D11_BLEND d3dLogicOpSrcFactors[16] =
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

static const D3D11_BLEND d3dLogicOpDestFactors[16] =
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

static const D3D11_CULL_MODE d3dCullModes[4] =
{
	D3D11_CULL_NONE,
	D3D11_CULL_BACK,
	D3D11_CULL_FRONT,
	D3D11_CULL_BACK
};

static const D3D11_COMPARISON_FUNC d3dCmpFuncs[8] =
{
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS
};

#define TEXF_NONE   0
#define TEXF_POINT  1
#define TEXF_LINEAR 2
static const unsigned int d3dMipFilters[4] =
{
	TEXF_NONE,
	TEXF_POINT,
	TEXF_LINEAR,
	TEXF_NONE, //reserved
};

static const D3D11_TEXTURE_ADDRESS_MODE d3dClamps[4] =
{
	D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_MIRROR,
	D3D11_TEXTURE_ADDRESS_WRAP //reserved
};

void SetupDeviceObjects()
{
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
	ddesc.DepthEnable	  = FALSE;
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc		= D3D11_COMPARISON_ALWAYS;
	ddesc.StencilEnable	= FALSE;
	ddesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[0]);
	CHECK(hr==S_OK, "Create depth state for Renderer::ClearScreen");
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
	ddesc.DepthEnable	  = TRUE;
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

	ddesc.DepthEnable	   = FALSE;
	ddesc.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc		 = D3D11_COMPARISON_LESS;
	ddesc.StencilEnable	 = FALSE;
	ddesc.StencilReadMask   = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask  = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &resetdepthstate);
	CHECK(hr==S_OK, "Create depth state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetdepthstate, "depth stencil state for Renderer::ResetAPIState");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, &resetraststate);
	CHECK(hr==S_OK, "Create rasterizer state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetraststate, "rasterizer state for Renderer::ResetAPIState");
}

// Kill off all POOL_DEFAULT device objects.
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
}

namespace DX11
{

Renderer::Renderer()
{
	int x, y, w_temp, h_temp;
	s_blendMode = 0;

	g_VideoInitialize.pRequestWindowSize(x, y, w_temp, h_temp);

	D3D::Create(EmuWindow::GetWnd());

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	s_XFB_width = MAX_XFB_WIDTH;
	s_XFB_height = MAX_XFB_HEIGHT;

	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	CalculateXYScale(dst_rect);

	s_LastAA = g_ActiveConfig.iMultisampleMode;
	s_LastEFBScale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize();

	SetupDeviceObjects();

	for (unsigned int stage = 0; stage < 8; stage++)
		D3D::gfxstate->samplerdesc[stage].MaxAnisotropy = g_ActiveConfig.iMaxAnisotropy;

	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), ClearColor);
	D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)(s_Fulltarget_width - s_target_width) / 2.f,
						(float)(s_Fulltarget_height - s_target_height) / 2.f,
						(float)s_target_width, (float)s_target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	D3D::BeginFrame();
	D3D::gfxstate->rastdesc.ScissorEnable = TRUE;

	reset_called = false;
}

Renderer::~Renderer()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = EFBToScaledX(rc.left) + TargetStrideX();
	result.top    = EFBToScaledY(rc.top) + TargetStrideY();
	result.right  = EFBToScaledX(rc.right) + TargetStrideX();
	result.bottom = EFBToScaledY(rc.bottom) + TargetStrideY();
	return result;
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
bool Renderer::CheckForResize()
{
	while (EmuWindow::IsSizing())
		Sleep(10);

	if (EmuWindow::GetParentWnd())
	{
		// Re-stretch window to parent window size again, if it has a parent window.
		RECT rcParentWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcParentWindow);
		int width = rcParentWindow.right - rcParentWindow.left;
		int height = rcParentWindow.bottom - rcParentWindow.top;
		if (width != Renderer::GetBackbufferWidth() || height != Renderer::GetBackbufferHeight())
			MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
	}
	RECT rcWindow;
	GetClientRect(EmuWindow::GetWnd(), &rcWindow);
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

bool Renderer::SetScissorRect()
{
	TargetRectangle rc;
	GetScissorRect(rc);

	if (rc.left < 0) rc.left = 0;
	if (rc.right < 0) rc.right = 0;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom < 0) rc.bottom = 0;

	if (rc.left > EFB_WIDTH) rc.left = EFB_WIDTH;
	if (rc.right > EFB_WIDTH) rc.right = EFB_WIDTH;
	if (rc.top > EFB_HEIGHT) rc.top = EFB_HEIGHT;
	if (rc.bottom > EFB_HEIGHT) rc.bottom = EFB_HEIGHT;

	rc.left = EFBToScaledX(rc.left) + TargetStrideX();
	rc.right = EFBToScaledX(rc.right) + TargetStrideX();
	rc.top = EFBToScaledY(rc.top) + TargetStrideY();
	rc.bottom = EFBToScaledY(rc.bottom) + TargetStrideY();

	if (rc.left > rc.right)
	{
		int temp = rc.right;
		rc.right = rc.left;
		rc.left = temp;
	}
	if (rc.top > rc.bottom)
	{
		int temp = rc.bottom;
		rc.bottom = rc.top;
		rc.top = temp;
	}

	if (rc.right >= rc.left && rc.bottom >= rc.top)
	{
		D3D::context->RSSetScissorRects(1, rc.AsRECT());
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		*rc.AsRECT() = CD3D11_RECT(TargetStrideX(), TargetStrideY(),
									TargetStrideX() + s_target_width, TargetStrideY() + s_target_height);
		D3D::context->RSSetScissorRects(1, rc.AsRECT());
		return false;
	}
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	UINT8 color_mask = 0;
	if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24))
		color_mask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	if (bpmem.blendmode.colorupdate)
		color_mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	D3D::gfxstate->SetRenderTargetWriteMask(color_mask);
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//	- GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//	- GX_PokeAlphaMode (TODO)
//	- GX_PokeAlphaUpdate (TODO)
//	- GX_PokeBlendMode (TODO)
//	- GX_PokeColorUpdate (TODO)
//	- GX_PokeDither (TODO)
//	- GX_PokeDstAlpha (TODO)
//	- GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	// TODO: This function currently is broken if anti-aliasing is enabled
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Texture2D* read_tex;

	if (!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	if (type == POKE_Z)
	{
		static bool alert_only_once = true;
		if (!alert_only_once) return 0;
		PanicAlert("EFB: Poke Z not implemented (tried to poke z value %#x at (%d,%d))", poke_data, x, y);
		alert_only_once = false;
		return 0;
	}

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;
	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// Take the mean of the resulting dimensions; TODO: Don't use the center pixel, compute the average color instead
	D3D11_RECT RectToLock;
	if(type == PEEK_COLOR || type == PEEK_Z)
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
		D3D::context->PSSetConstantBuffers(0, 1, &access_efb_cbuf);
		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBDepthReadTexture()->GetRTV(), NULL);
		D3D::SetPointCopySampler();
		D3D::drawShadedTexQuad(FramebufferManager::GetEFBDepthTexture()->GetSRV(),
								&RectToLock,
								Renderer::GetFullTargetWidth(),
								Renderer::GetFullTargetHeight(),
								PixelShaderCache::GetDepthMatrixProgram(true),
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

		float val = *(float*)map.pData;
		u32 ret = 0;
		if(bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret = ((u32)(val * 0xffff));
		}
		else
		{
			ret = ((u32)(val * 0xffffff));
		}
		D3D::context->Unmap(read_tex, 0);

		// TODO: in RE0 this value is often off by one in Video_DX9 (where this code is derived from), which causes lighting to disappear
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
		if(map.pData)
			ret = *(u32*)map.pData;
		D3D::context->Unmap(read_tex, 0);

		// check what to do with the alpha channel (GX_PokeAlphaRead)
		PixelEngine::UPEAlphaReadReg alpha_read_mode;
		PixelEngine::Read16((u16&)alpha_read_mode, PE_DSTALPHACONF);

		if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		{
			ret = RGBA8ToRGBA6ToRGBA8(ret);
		}
		else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		{
			ret = RGBA8ToRGB565ToRGBA8(ret);
		}			
		if(bpmem.zcontrol.pixel_format != PIXELFMT_RGBA6_Z24)
		{
			ret |= 0xFF000000;
		}

		if(alpha_read_mode.ReadMode == 2) return ret; // GX_READ_NONE
		else if(alpha_read_mode.ReadMode == 1) return (ret | 0xFF000000); // GX_READ_FF
		else /*if(alpha_read_mode.ReadMode == 0)*/ return (ret & 0x00FFFFFF); // GX_READ_00
	}
	else //if(type == POKE_COLOR)
	{
		u32 rgbaColor = (poke_data & 0xFF00FF00) | ((poke_data >> 16) & 0xFF) | ((poke_data << 16) & 0xFF0000);

		// TODO: The first five PE registers may change behavior of EFB pokes, this isn't implemented, yet.
		ResetAPIState();

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), NULL);
		D3D::drawColorQuad(rgbaColor, (float)RectToLock.left   * 2.f / (float)Renderer::GetFullTargetWidth()  - 1.f,
									- (float)RectToLock.top	* 2.f / (float)Renderer::GetFullTargetHeight() + 1.f,
									  (float)RectToLock.right  * 2.f / (float)Renderer::GetFullTargetWidth()  - 1.f,
									- (float)RectToLock.bottom * 2.f / (float)Renderer::GetFullTargetHeight() + 1.f);

		RestoreAPIState();
		return 0;
	}
}

// Called from VertexShaderManager
void Renderer::UpdateViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz
	const int old_fulltarget_w = Renderer::GetFullTargetWidth();
	const int old_fulltarget_h = Renderer::GetFullTargetHeight();

	int scissorXOff = bpmem.scissorOffset.x << 1;
	int scissorYOff = bpmem.scissorOffset.y << 1;

	// TODO: ceil, floor or just cast to int?
	// TODO: Directly use the floats instead of rounding them?
	int X = Renderer::EFBToScaledX((int)ceil(xfregs.rawViewport[3] - xfregs.rawViewport[0] - scissorXOff)) + Renderer::TargetStrideX();
	int Y = Renderer::EFBToScaledY((int)ceil(xfregs.rawViewport[4] + xfregs.rawViewport[1] - scissorYOff)) + Renderer::TargetStrideY();
	int Width = Renderer::EFBToScaledX((int)ceil(2.0f * xfregs.rawViewport[0]));
	int Height = Renderer::EFBToScaledY((int)ceil(-2.0f * xfregs.rawViewport[1]));
	if (Width < 0)
	{
		X += Width;
		Width *= -1;
	}
	if (Height < 0)
	{
		Y += Height;
		Height *= -1;
	}
	bool sizeChanged = false;
	if (X < 0)
	{
		s_Fulltarget_width -= 2 * X;
		X = 0;
		sizeChanged = true;
	}
	if (Y < 0)
	{
		s_Fulltarget_height -= 2 * Y;
		Y = 0;
		sizeChanged = true;
	}

	float newx = (float)X;
	float newy = (float)Y;
	float newwidth = (float)Width;
	float newheight = (float)Height;
	if (sizeChanged)
	{
		// Make sure that the requested size is actually supported by the GFX driver
		if (Renderer::GetFullTargetWidth() > (int)D3D::GetMaxTextureSize() || Renderer::GetFullTargetHeight() > (int)D3D::GetMaxTextureSize())
		{
			// Skip EFB recreation and viewport setting. Most likely causes glitches in this case, but prevents crashes at least
			ERROR_LOG(VIDEO, "Tried to set a viewport which is too wide to emulate with Direct3D11. Requested EFB size is %dx%d\n", Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight());

			// Fix the viewport to fit to the old EFB size, TODO: Check this for off-by-one errors
			newx *= (float)(old_fulltarget_w-1) / (float)(Renderer::GetFullTargetWidth()-1);
			newy *= (float)(old_fulltarget_h-1) / (float)(Renderer::GetFullTargetHeight()-1);
			newwidth *= (float)(old_fulltarget_w-1) / (float)(Renderer::GetFullTargetWidth()-1);
			newheight *= (float)(old_fulltarget_h-1) / (float)(Renderer::GetFullTargetHeight()-1);

			s_Fulltarget_width = old_fulltarget_w;
			s_Fulltarget_height = old_fulltarget_h;
		}
		else
		{
			D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
			
			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager;

			D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
			float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };
			D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_col);
			D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);
		}
	}

	// Some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(newx, newy, newwidth, newheight,
										0.f,	// (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
										1.f);   //  xfregs.rawViewport[5] / 16777216.0f;
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
	D3D::drawClearQuad(rgbaColor, (z & 0xFFFFFF) / float(0xFFFFFF), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader(), VertexShaderCache::GetClearInputLayout());

	D3D::stateman->PopDepthState();
	D3D::stateman->PopBlendState();

	RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// TODO
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	if (bpmem.blendmode.logicopenable && !forceUpdate)
		return;

	if (bpmem.blendmode.subtract)  // enable blending src 1 dst 1
	{
		D3D::gfxstate->SetAlphaBlendEnable(true);
		D3D::gfxstate->SetBlendOp(D3D11_BLEND_OP_REV_SUBTRACT);
		D3D::gfxstate->SetSrcBlend(d3dSrcFactors[1]);
		D3D::gfxstate->SetDestBlend(d3dDestFactors[1]);
	}
	else
	{
		D3D::gfxstate->SetAlphaBlendEnable(bpmem.blendmode.blendenable && (!( bpmem.blendmode.srcfactor == 1 && bpmem.blendmode.dstfactor == 0)));
		if (bpmem.blendmode.blendenable && (!( bpmem.blendmode.srcfactor == 1 && bpmem.blendmode.dstfactor == 0)))
		{
			D3D::gfxstate->SetBlendOp(D3D11_BLEND_OP_ADD);
			D3D::gfxstate->SetSrcBlend(d3dSrcFactors[bpmem.blendmode.srcfactor]);
			D3D::gfxstate->SetDestBlend(d3dDestFactors[bpmem.blendmode.dstfactor]);
		}
	}
}

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle &rc)
{
	// copy back buffer to system memory
	ID3D11Texture2D* buftex;
	D3D11_TEXTURE2D_DESC tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, D3D::GetBackBufferWidth(), D3D::GetBackBufferHeight(), 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateTexture2D(&tex_desc, NULL, &buftex);
	if (FAILED(hr)) PanicAlert("Failed to create screenshot buffer texture");
	D3D::context->CopyResource(buftex, (ID3D11Resource*)D3D::GetBackBuffer()->GetTex());

	// D3DX11SaveTextureToFileA doesn't allow us to ignore the alpha channel, so we need to strip it out ourselves
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(buftex, 0, D3D11_MAP_READ_WRITE, 0, &map);
	for (unsigned int y = 0; y < D3D::GetBackBufferHeight(); ++y)
	{
		u8* ptr = (u8*)map.pData + y * map.RowPitch + 3;
		for (unsigned int x = 0; x < D3D::GetBackBufferWidth(); ++x)
		{
			*ptr = 0xFF;
			ptr += 4;
		}
	}
	D3D::context->Unmap(buftex, 0);

	// ready to be saved
	hr = PD3DX11SaveTextureToFileA(D3D::context, buftex, D3DX11_IFF_PNG, filename.c_str());
	buftex->Release();

	return SUCCEEDED(hr);
}


// This function has the final picture. We adjust the aspect ratio here.
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc,float Gamma)
{
	if (g_bSkipCurrentFrame || (!XFBWrited && (!g_ActiveConfig.bUseXFB || !g_ActiveConfig.bUseRealXFB)) || !fbWidth || !fbHeight)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}
	// this function is called after the XFB field is changed, not after
	// EFB is copied to XFB. In this way, flickering is reduced in games
	// and seems to also give more FPS in ZTP

	if (field == FIELD_LOWER) xfbAddr -= fbWidth * 2;
	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}

	ResetAPIState();

	// Prepare to copy the XFBs to our backbuffer
	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)s_backbuffer_width, (float)s_backbuffer_height);
	D3D::context->RSSetViewports(1, &vp);
	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), ClearColor);

	int X = dst_rect.left;
	int Y = dst_rect.top;
	int Width  = dst_rect.right - dst_rect.left;
	int Height = dst_rect.bottom - dst_rect.top;

	if (X < 0) X = 0;
	if (Y < 0) Y = 0;
	if (X > s_backbuffer_width) X = s_backbuffer_width;
	if (Y > s_backbuffer_height) Y = s_backbuffer_height;
	if (Width < 0) Width = 0;
	if (Height < 0) Height = 0;
	if (Width > (s_backbuffer_width - X)) Width = s_backbuffer_width - X;
	if (Height > (s_backbuffer_height - Y)) Height = s_backbuffer_height - Y;
	vp = CD3D11_VIEWPORT((float)X, (float)Y, (float)Width, (float)Height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);

	// activate linear filtering for the buffer copies
	D3D::SetLinearCopySampler();

	if(g_ActiveConfig.bUseXFB)
	{
		const XFBSourceBase* xfbSource;

		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = xfbSourceList[i];
			MathUtil::Rectangle<float> sourceRc;
			
			sourceRc.left = 0;
			sourceRc.top = 0;
			sourceRc.right = (float)xfbSource->texWidth;
			sourceRc.bottom = (float)xfbSource->texHeight;

			MathUtil::Rectangle<float> drawRc;

			if (g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbWidth * 2);

				drawRc.bottom = 1.0f - (2.0f * (hOffset) / (float)fbHeight);
				drawRc.top = 1.0f - (2.0f * (hOffset + xfbHeight) / (float)fbHeight);
				drawRc.left = -(xfbWidth / (float)fbWidth);
				drawRc.right = (xfbWidth / (float)fbWidth);

				// The following code disables auto stretch.  Kept for reference.
				// scale draw area for a 1 to 1 pixel mapping with the draw target
				//float vScale = (float)fbHeight / (float)s_backbuffer_height;
				//float hScale = (float)fbWidth / (float)s_backbuffer_width;
				//drawRc.top *= vScale;
				//drawRc.bottom *= vScale;
				//drawRc.left *= hScale;
				//drawRc.right *= hScale;
			}
			else
			{
				drawRc.top = -1;
				drawRc.bottom = 1;
				drawRc.left = -1;
				drawRc.right = 1;
			}

			xfbSource->Draw(sourceRc, drawRc, 0, 0);
		}
	}
	else
	{
		TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);

		// TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB texture as source
		D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();
		D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(), PixelShaderCache::GetColorCopyProgram(false),VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(), Gamma);
	}
	// done with drawing the game stuff, good moment to save a screenshot
	if (s_bScreenshot)
	{
		SaveScreenshot(s_sScreenshotName, dst_rect);
		s_bScreenshot = false;
	}

	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bShowFPS)
	{
		char fps[20];
		StringCchPrintfA(fps, 20, "FPS: %d\n", s_fps);
		D3D::font.DrawTextScaled(0, 30, 20, 0.0f, 0xFF00FFFF, fps);
	}
	Renderer::DrawDebugText();

	if (g_ActiveConfig.bOverlayStats)
	{
		char buf[32768];
		Statistics::ToString(buf);
		D3D::font.DrawTextScaled(0, 30, 20, 0.0f, 0xFF00FFFF, buf);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		char buf[32768];
		Statistics::ToStringProj(buf);
		D3D::font.DrawTextScaled(0, 30, 20, 0.0f, 0xFF00FFFF, buf);
	}

	OSD::DrawMessages();
	D3D::EndFrame();
	frameCount++;

	GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);

	DLCache::ProgressiveCleanup();
	TextureCache::Cleanup();

	// Enable any configuration changes
	UpdateActiveConfig();
	const bool WindowResized = CheckForResize();

	bool xfbchanged = false;

	if (s_XFB_width != fbWidth || s_XFB_height != fbHeight)
	{
		xfbchanged = true;
		s_XFB_width = fbWidth;
		s_XFB_height = fbHeight;
		if (s_XFB_width < 1) s_XFB_width = MAX_XFB_WIDTH;
		if (s_XFB_width > MAX_XFB_WIDTH) s_XFB_width = MAX_XFB_WIDTH;
		if (s_XFB_height < 1) s_XFB_height = MAX_XFB_HEIGHT;
		if (s_XFB_height > MAX_XFB_HEIGHT) s_XFB_height = MAX_XFB_HEIGHT;
	}

	// update FPS counter
	static int fpscount = 0;
	static unsigned long lasttime = 0;
	if (Common::Timer::GetTimeMs() - lasttime >= 1000)
	{
		lasttime = Common::Timer::GetTimeMs();
		s_fps = fpscount;
		fpscount = 0;
	}
	if (XFBWrited)
		++fpscount;

	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	// New frame
	stats.ResetFrame();

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();

	// resize the back buffers NOW to avoid flickering
	if (xfbchanged || WindowResized ||
		s_LastEFBScale != g_ActiveConfig.iEFBScale ||
		s_LastAA != g_ActiveConfig.iMultisampleMode)
	{
		s_LastAA = g_ActiveConfig.iMultisampleMode;
		PixelShaderCache::InvalidateMSAAShaders();

		// TODO: Aren't we still holding a reference to the back buffer right now?
		D3D::Reset();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();

		ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

		CalculateXYScale(dst_rect);

		s_LastEFBScale = g_ActiveConfig.iEFBScale;
		CalculateTargetSize();

		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
		
		delete g_framebuffer_manager;
		g_framebuffer_manager = new FramebufferManager;
		float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };
		D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_col);
		D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);
	}

	// begin next frame
	Renderer::RestoreAPIState();
	D3D::BeginFrame();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	UpdateViewport();
	VertexShaderManager::SetViewportChanged();

	g_VideoInitialize.pCopiedToXFB(XFBWrited || (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
	XFBWrited = false;
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::stateman->PushBlendState(resetblendstate);
	D3D::stateman->PushDepthState(resetdepthstate);
	D3D::stateman->PushRasterizerState(resetraststate);
	reset_called = true;
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	if (reset_called)
	{
		D3D::stateman->PopBlendState();
		D3D::stateman->PopDepthState();
		D3D::stateman->PopRasterizerState();
	}
	UpdateViewport();
	SetScissorRect();
	reset_called = false;
}

void Renderer::SetGenerationMode()
{
	// rastdesc.FrontCounterClockwise must be false for this to work
	D3D::gfxstate->rastdesc.CullMode = d3dCullModes[bpmem.genMode.cullmode];
}

void Renderer::SetDepthMode()
{
	if (bpmem.zmode.testenable)
	{
		D3D::gfxstate->depthdesc.DepthEnable = TRUE;
		D3D::gfxstate->depthdesc.DepthWriteMask = bpmem.zmode.updateenable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		D3D::gfxstate->depthdesc.DepthFunc = d3dCmpFuncs[bpmem.zmode.func];
	}
	else
	{
		// if the test is disabled write is disabled too
		D3D::gfxstate->depthdesc.DepthEnable = FALSE;
		D3D::gfxstate->depthdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	}
}

void Renderer::SetLogicOpMode()
{
	if (bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != 3)
	{
		s_blendMode = 0;
		D3D::gfxstate->SetAlphaBlendEnable(true);
		D3D::gfxstate->SetBlendOp(d3dLogicOps[bpmem.blendmode.logicmode]);
		D3D::gfxstate->SetSrcBlend(d3dLogicOpSrcFactors[bpmem.blendmode.logicmode]);
		D3D::gfxstate->SetDestBlend(d3dLogicOpDestFactors[bpmem.blendmode.logicmode]);
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

void Renderer::SetLineWidth()
{
	// TODO
}

void Renderer::SetSamplerState(int stage, int texindex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];
	
	unsigned int mip;
	mip = (tm0.min_filter == 8) ? TEXF_NONE:d3dMipFilters[tm0.min_filter & 3];
	if ((tm0.min_filter & 3) && (tm0.min_filter != 8) && ((tm1.max_lod >> 4) == 0)) mip = TEXF_NONE;

	if (texindex) stage += 4;

	// TODO: Clarify whether these values are correct
	// NOTE: since there's no "no filter" in DX11 we're using point filters in these cases
	if (g_ActiveConfig.bForceFiltering)
	{
		D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
	}
	else if (tm0.min_filter & 4) // linear min filter
	{
		if (tm0.mag_filter) // linear mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
		}
		else // point mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,		D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
		}
	}
	else // point min filter
	{
		if (tm0.mag_filter) // linear mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,		D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR);
		}
		else // point mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,		D3D11_FILTER_MIN_MAG_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_MAG_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR);
		}
	}

	D3D::gfxstate->samplerdesc[stage].AddressU = d3dClamps[tm0.wrap_s];
	D3D::gfxstate->samplerdesc[stage].AddressV = d3dClamps[tm0.wrap_t];

	D3D::gfxstate->samplerdesc[stage].MipLODBias = (float)tm0.lod_bias/32.0f;
	D3D::gfxstate->samplerdesc[stage].MaxLOD = (float)tm1.max_lod/16.f;
	D3D::gfxstate->samplerdesc[stage].MinLOD = (float)tm1.min_lod/16.f;
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

}