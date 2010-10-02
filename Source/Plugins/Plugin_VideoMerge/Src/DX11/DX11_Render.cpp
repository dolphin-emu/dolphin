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
#include <strsafe.h>

// Common
#include "Common.h"
#include "StringUtil.h"
#include "Atomic.h"
#include "FileUtil.h"
#include "Thread.h"
#include "Timer.h"
#include "Statistics.h"
#include "OnScreenDisplay.h"

// VideoCommon
#include "VideoConfig.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexLoaderManager.h"
#include "AVIDump.h"
#include "Fifo.h"
#include "DLCache.h"

// DX11
#include "DX11_D3DUtil.h"
#include "DX11_VertexManager.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_PixelShaderCache.h"
#include "DX11_TextureCache.h"
#include "DX11_FramebufferManager.h"
#include "DX11_Render.h"

#include "../EmuWindow.h"
#include "../Main.h"

namespace DX11
{

ID3D11Buffer* access_efb_cbuf = NULL;
ID3D11DepthStencilState* cleardepthstates[2] = {NULL};
ID3D11RasterizerState* clearraststate = NULL;
ID3D11BlendState* resetblendstate = NULL;
ID3D11DepthStencilState* resetdepthstate = NULL;
ID3D11RasterizerState* resetraststate = NULL;

bool reset_called = false;

// state translation lookup tables
static const D3D11_BLEND d3dSrcFactors[8] =
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA, 
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
	D3D11_BLEND_INV_SRC_ALPHA,
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


// what are these 2?
//bool Renderer::Allow2x()
//{
//	return false;
//}
//
//bool Renderer::AllowCustom()
//{
//	return false;
//}

void Renderer::SetupDeviceObjects()
{
	g_framebuffer_manager = new FramebufferManager;

	HRESULT hr;
	float colmat[20]= {0.0f};
	colmat[0] = colmat[5] = colmat[10] = 1.0f;
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(20*sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = colmat;
	hr = D3D::device->CreateBuffer(&cbdesc, &data, &access_efb_cbuf);
	CHECK(hr==S_OK, "Create constant buffer for AccessEFB");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)access_efb_cbuf, "constant buffer for AccessEFB");

	D3D11_DEPTH_STENCIL_DESC ddesc;
	ddesc.DepthEnable      = FALSE;
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc        = D3D11_COMPARISON_ALWAYS;
	ddesc.StencilEnable    = FALSE;
	ddesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[0]);
	CHECK(hr==S_OK, "Create depth state for ClearScreen");
	ddesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
	ddesc.DepthEnable      = TRUE;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &cleardepthstates[1]);
	CHECK(hr==S_OK, "Create depth state for ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[0], "depth state for ClearScreen (depth buffer disabled)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[1], "depth state for ClearScreen (depth buffer enabled)");

	// TODO: once multisampling gets implemented, this might need to be changed
	D3D11_RASTERIZER_DESC rdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, true, false, false);
	hr = D3D::device->CreateRasterizerState(&rdesc, &clearraststate);
	CHECK(hr==S_OK, "Create rasterizer state for ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)clearraststate, "rasterizer state for ClearScreen");

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
	CHECK(hr==S_OK, "Create blend state for ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetblendstate, "blend state for ResetAPIState");

	ddesc.DepthEnable       = FALSE;
	ddesc.DepthWriteMask    = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc         = D3D11_COMPARISON_LESS;
	ddesc.StencilEnable     = FALSE;
	ddesc.StencilReadMask   = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask  = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &resetdepthstate);
	CHECK(hr==S_OK, "Create depth state for ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetdepthstate, "depth stencil state for ResetAPIState");

	// this might need to be changed once multisampling support gets added
	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, &resetraststate);
	CHECK(hr==S_OK, "Create rasterizer state for ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetraststate, "rasterizer state for ResetAPIState");
}

void Renderer::TeardownDeviceObjects()
{
	delete g_framebuffer_manager;

	SAFE_RELEASE(access_efb_cbuf);
	SAFE_RELEASE(cleardepthstates[0]);
	SAFE_RELEASE(cleardepthstates[1]);
	SAFE_RELEASE(clearraststate);
	SAFE_RELEASE(resetblendstate);
	SAFE_RELEASE(resetdepthstate);
	SAFE_RELEASE(resetraststate);
}

Renderer::Renderer()
{
	UpdateActiveConfig();

	//int x, y, w_temp, h_temp;
	//g_VideoInitialize.pRequestWindowSize(x, y, w_temp, h_temp);

	D3D::Create(EmuWindow::GetWnd());

	FramebufferSize(D3D::GetBackBufferWidth(), D3D::GetBackBufferHeight());

	SetupDeviceObjects();

	for (unsigned int stage = 0; stage < 8; stage++)
		D3D::gfxstate->samplerdesc[stage].MaxAnisotropy = g_ActiveConfig.iMaxAnisotropy;

	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), ClearColor);
	D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)(s_Fulltarget_width - s_target_width) / 2.f,
						(float)(s_Fulltarget_height - s_target_height) / 2.f,
						(float)s_target_width, (float)s_target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	D3D::BeginFrame();
	D3D::gfxstate->rastdesc.ScissorEnable = TRUE;

	// temporary, maybe
	D3D::InitUtils();

	reset_called = false;
	//return true;
}

Renderer::~Renderer()
{
	// temporary, maybe
	D3D::ShutdownUtils();

	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

bool Renderer::CheckForResize()
{
	while (EmuWindow::IsSizing())
		Sleep(10);

	if (EmuWindow::GetParentWnd())
	{
		// re-stretch window to parent window size again, if it has a parent window.
		RECT rcParentWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcParentWindow);
		int width = rcParentWindow.right - rcParentWindow.left;
		int height = rcParentWindow.bottom - rcParentWindow.top;
		if (width != s_backbuffer_width || height != s_backbuffer_height)
			::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
	}
	RECT rcWindow;
	GetClientRect(EmuWindow::GetWnd(), &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;

	// sanity check
	return ((client_width != s_backbuffer_width || client_height != s_backbuffer_height) && 
		client_width >= 4 && client_height >= 4);
}

bool Renderer::SetScissorRect()
{
	EFBRectangle rc;
	if (g_renderer->SetScissorRect(rc))
	{
		D3D::context->RSSetScissorRects(1, (D3D11_RECT*)&rc);
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		const int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
		const int Ystride =  (s_Fulltarget_height - s_target_height) / 2;
		rc.left = Xstride;
		rc.top = Ystride;
		rc.right = Xstride + s_target_width;
		rc.bottom = Ystride + s_target_height;

		D3D::context->RSSetScissorRects(1, (D3D11_RECT*)&rc);
		return false;
	}
}

void Renderer::SetColorMask()
{
	UINT8 color_mask = 0;
	if (bpmem.blendmode.alphaupdate)
		color_mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

	if (bpmem.blendmode.colorupdate)
		color_mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;

	D3D::gfxstate->SetRenderTargetWriteMask(color_mask);
}

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	ID3D11Texture2D* read_tex;
	u32 ret;

	if (!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	if (type == POKE_Z || type == POKE_COLOR)
	{
		static bool alert_only_once = true;
		if (!alert_only_once)
			return 0;
		PanicAlert("Poke EFB not implemented");
		alert_only_once = false;
		return 0;
	}

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;
	TargetRectangle targetPixelRc = ConvertEFBRectangle(efbPixelRc);

	// Take the mean of the resulting dimensions; TODO: check whether this causes any bugs compared to taking the average color of the target area
	D3D11_RECT RectToLock;
	RectToLock.left = (targetPixelRc.left + targetPixelRc.right) / 2;
	RectToLock.top = (targetPixelRc.top + targetPixelRc.bottom) / 2;
	RectToLock.right = RectToLock.left + 1;
	RectToLock.bottom = RectToLock.top + 1;
	if (type == PEEK_Z)
	{
		ResetAPIState(); // reset any game specific settings

		// depth buffers can only be read as a whole using CopySubresourceRegion'ed, so we're using drawShadedTexQuad instead
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, 4.f, 4.f);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->PSSetConstantBuffers(0, 1, &access_efb_cbuf);
		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBDepthReadTexture()->GetRTV(), NULL);
		D3D::SetPointCopySampler();
		D3D::drawShadedTexQuad(FramebufferManager::GetEFBDepthTexture()->GetSRV(),
			&RectToLock,
			GetFullTargetWidth(),
			GetFullTargetHeight(),
			PixelShaderCache::GetDepthMatrixProgram(),
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

		// copy to system memory
		D3D11_BOX box = CD3D11_BOX(0, 0, 0, 1, 1, 1);
		read_tex = FramebufferManager::GetEFBDepthStagingBuffer();
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FramebufferManager::GetEFBDepthReadTexture()->GetTex(), 0, &box);

		RestoreAPIState(); // restore game state
	}
	else
	{
		// we can directly copy to system memory here
		read_tex = FramebufferManager::GetEFBColorStagingBuffer();
		D3D11_BOX box = CD3D11_BOX(RectToLock.left, RectToLock.top, 0, RectToLock.right, RectToLock.bottom, 1);
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FramebufferManager::GetEFBColorTexture()->GetTex(), 0, &box);
	}

	// read the data from system memory
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(read_tex, 0, D3D11_MAP_READ, 0, &map);

	switch(type)
	{
	case PEEK_Z:
		{
			// TODO: Implement this better? Maybe we're losing precision or sth..
			float val = *(float*)map.pData;
			ret = (u32)(val * 0xffffff);
			break;
		}

	case PEEK_COLOR:
		ret = *(u32*)map.pData;
		break;

	// TODO: Implement POKE_Z and POKE_COLOR
	default:
		break;
	}
	D3D::context->Unmap(read_tex, 0);
	return ret;
}

// Called from VertexShaderManager
void Renderer::UpdateViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 0xFFFFFF * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 0xFFFFFF * farz
	const int old_fulltarget_w = s_Fulltarget_width;
	const int old_fulltarget_h = s_Fulltarget_height;

	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float MValueX = GetTargetScaleX();
	float MValueY = GetTargetScaleY();
	
	int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
	int Ystride =  (s_Fulltarget_height - s_target_height) / 2;

	// Stretch picture with increased internal resolution
	int X = (int)(ceil(xfregs.rawViewport[3] - xfregs.rawViewport[0] - (scissorXOff)) * MValueX) + Xstride;
	int Y = (int)(ceil(xfregs.rawViewport[4] + xfregs.rawViewport[1] - (scissorYOff)) * MValueY) + Ystride;
	int Width  = (int)ceil((int)(2 * xfregs.rawViewport[0]) * MValueX);
	int Height = (int)ceil((int)(-2 * xfregs.rawViewport[1]) * MValueY);
	if (Width < 0)
	{
		X += Width;
		Width*=-1;
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
		sizeChanged=true;
	}
	if (Y < 0)
	{
		s_Fulltarget_height -= 2 * Y;
		Y = 0;
		sizeChanged=true;
	}

	float newx = (float)X;
	float newy = (float)Y;
	float newwidth = (float)Width;
	float newheight = (float)Height;
	if (sizeChanged)
	{
		// Make sure that the requested size is actually supported by the GFX driver
		if (s_Fulltarget_width > (int)D3D::GetMaxTextureSize() || s_Fulltarget_height > (int)D3D::GetMaxTextureSize())
		{
			// Skip EFB recreation and viewport setting. Most likely causes glitches in this case, but prevents crashes at least
			WARNING_LOG(VIDEO, "Tried to set a viewport which is too wide to emulate with Direct3D11. Requested EFB size is %dx%d\n", s_Fulltarget_width, s_Fulltarget_height);

			// Fix the viewport to fit to the old EFB size, TODO: Check this for off-by-one errors
			newx *= (float)old_fulltarget_w / (float)s_Fulltarget_width;
			newy *= (float)old_fulltarget_h / (float)s_Fulltarget_height;
			newwidth *= (float)old_fulltarget_w / (float)s_Fulltarget_width;
			newheight *= (float)old_fulltarget_h / (float)s_Fulltarget_height;

			s_Fulltarget_width = old_fulltarget_w;
			s_Fulltarget_height = old_fulltarget_h;
		}
		else
		{
			D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);

			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager;

			D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
		}
	}

	// some games set invalids values MinDepth and MaxDepth so fix them to the max an min allowed and let the shaders do this work
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(newx, newy, newwidth, newheight,
										0.f,    // (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
										1.f);   //  xfregs.rawViewport[5] / 16777216.0f;
	D3D::context->RSSetViewports(1, &vp);
}

// color is passed in bgra order so we need to convert it to rgba
void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	const TargetRectangle targetRc = ConvertEFBRectangle(rc);
	// update the view port for clearing the picture
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight(),
										0.f, 
										1.f); 
	D3D::context->RSSetViewports(1, &vp);

	// always set the scissor in case it was set by the game and has not been reset
	D3D11_RECT sirc = CD3D11_RECT(targetRc.left, targetRc.top, targetRc.right, targetRc.bottom);
	D3D::context->RSSetScissorRects(1, &sirc);
	u32 rgbaColor = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	D3D::stateman->PushDepthState(cleardepthstates[zEnable]);
	D3D::stateman->PushRasterizerState(clearraststate);
	//D3D::stateman->PushBlendState(resetblendstate); temporarily commented until I find the cause of the blending issue in mkwii (see next line)
	D3D::gfxstate->ApplyState();  // TODO (neobrain): find out whether this breaks/fixes anything or can just be dropped. Might obsolete the comment above this line
	D3D::drawClearQuad(rgbaColor, (z & 0xFFFFFF) / float(0xFFFFFF), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader(), VertexShaderCache::GetClearInputLayout());
	D3D::gfxstate->Reset();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
//	D3D::stateman->PopBlendState();
	UpdateViewport();
	SetScissorRect();
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	if (bpmem.blendmode.logicopenable)
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

void Renderer::PrepareXFBCopy(const TargetRectangle &dst_rect)
{
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)s_backbuffer_width, (float)s_backbuffer_height);
	D3D::context->RSSetViewports(1, &vp);
	static const float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };

	D3D::context->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), clear_color);

	int x = dst_rect.left;
	int y = dst_rect.top;
	int width  = dst_rect.right - dst_rect.left;
	int height = dst_rect.bottom - dst_rect.top;
	
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x > s_backbuffer_width) x = s_backbuffer_width;
	if (y > s_backbuffer_height) y = s_backbuffer_height;
	if (width < 0) width = 0;
	if (height < 0) height = 0;
	if (width > (s_backbuffer_width - x)) width = s_backbuffer_width - x;
	if (height > (s_backbuffer_height - y)) height = s_backbuffer_height - y;

	vp = CD3D11_VIEWPORT((float)x, (float)y, (float)width, (float)height);
	D3D::context->RSSetViewports(1, &vp);

	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);

	// activate linear filtering for the buffer copies
	D3D::SetLinearCopySampler();
}

void Renderer::Draw(const XFBSourceBase* xfbSource, const TargetRectangle& sourceRc,
		const MathUtil::Rectangle<float>& drawRc, const EFBRectangle& rc)
{
	if (xfbSource)
	{
		D3DTexture2D* const tex = ((XFBSource*)xfbSource)->tex;

		// TODO:
		PanicAlert("DX11 XFB");
	}
	else
	{
		D3DTexture2D* const tex = FramebufferManager::GetEFBColorTexture();

		D3D::drawShadedTexQuad(tex->GetSRV(), sourceRc.AsRECT(), GetFullTargetWidth(),
			GetFullTargetHeight(), PixelShaderCache::GetColorCopyProgram(),
			VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());
	}
}

void Renderer::EndFrame()
{
	D3D::EndFrame();
}

void Renderer::Present()
{
	D3D::Present();
		
	// TODO: Aren't we still holding a reference to the back buffer right now?
	// TODO: this was right before getting the backbuffer size in DX11::Renderer::Swap
	D3D::Reset();
}

void Renderer::GetBackBufferSize(int* w, int* h)
{
	*w = D3D::GetBackBufferWidth();
	*h = D3D::GetBackBufferHeight();
}

void Renderer::RecreateFramebufferManger()
{
	// this good?
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
	delete g_framebuffer_manager;
	g_framebuffer_manager = new FramebufferManager;
}

void Renderer::BeginFrame()
{
	D3D::BeginFrame();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::gfxstate->Reset();
	D3D::stateman->PushBlendState(resetblendstate);
	D3D::stateman->PushDepthState(resetdepthstate);
	D3D::stateman->PushRasterizerState(resetraststate);
	D3D::stateman->Apply();
	reset_called = true;
}

void Renderer::RestoreAPIState()
{
	// gets us back into a more game-like state.
	if (reset_called)
	{
		D3D::stateman->PopBlendState();
		D3D::stateman->PopDepthState();
		D3D::stateman->PopRasterizerState();
	}
	UpdateViewport();
	SetScissorRect();
	D3D::gfxstate->ApplyState();
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

void Renderer::SetSamplerState(int stage, int texindex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];
	
	unsigned int mip;
	mip = (tm0.min_filter == 8) ? TEXF_NONE : d3dMipFilters[tm0.min_filter & 3];
	if ((tm0.min_filter & 3) && (tm0.min_filter != 8) && ((tm1.max_lod >> 4) == 0)) mip = TEXF_NONE;

	if (texindex)
		stage += 4;

	// TODO: Clarify whether these values are correct
	// NOTE: since there's no "no filter" in DX11 we're using point filters in these cases
	if (tm0.min_filter & 4) // linear min filter
	{
		if (tm0.mag_filter) // linear mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,        D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
		}
		else // point mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,        D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
		}
	}
	else // point min filter
	{
		if (tm0.mag_filter) // linear mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,        D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR);
		}
		else // point mag filter
		{
			if (mip == TEXF_NONE) D3D::gfxstate->SetSamplerFilter(stage,        D3D11_FILTER_MIN_MAG_MIP_POINT);
			else if (mip == TEXF_POINT) D3D::gfxstate->SetSamplerFilter(stage,  D3D11_FILTER_MIN_MAG_MIP_POINT);
			else if (mip == TEXF_LINEAR) D3D::gfxstate->SetSamplerFilter(stage, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR);
		}
	}

	D3D::gfxstate->samplerdesc[stage].AddressU = d3dClamps[tm0.wrap_s];
	D3D::gfxstate->samplerdesc[stage].AddressV = d3dClamps[tm0.wrap_t];

	D3D::gfxstate->samplerdesc[stage].MipLODBias = (float)tm0.lod_bias / 32.0f;
	D3D::gfxstate->samplerdesc[stage].MaxLOD = (float)tm1.max_lod / 16.f;
	D3D::gfxstate->samplerdesc[stage].MinLOD = (float)tm1.min_lod / 16.f;
}

}
