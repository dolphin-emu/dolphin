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
#include "FBManager.h"
#include "Fifo.h"

#include <strsafe.h>

int frameCount = 0;
static int s_fps = 0;

static bool WindowResized;
static int s_target_width;
static int s_target_height;

static int s_Fulltarget_width;
static int s_Fulltarget_height;

static int s_backbuffer_width;
static int s_backbuffer_height;

static int s_XFB_width;
static int s_XFB_height;

static float xScale;
static float yScale;

static u32 s_blendMode;
static bool XFBWrited;

static bool s_bScreenshot = false;
static Common::CriticalSection s_criticalScreenshot;
static char s_sScreenshotName[1024];

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

void SetupDeviceObjects()
{
	HRESULT hr;

	D3D::font.Init();
	VertexLoaderManager::Init();
	FBManager.Create();

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();

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
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[0], "depth state for Renderer::ClearScreen (depth buffer disabled)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)cleardepthstates[1], "depth state for Renderer::ClearScreen (depth buffer enabled)");

	// TODO: once multisampling gets implemented, this might need to be changed
	D3D11_RASTERIZER_DESC rdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, true, false, false);
	hr = D3D::device->CreateRasterizerState(&rdesc, &clearraststate);
	CHECK(hr==S_OK, "Create rasterizer state for Renderer::ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)clearraststate, "rasterizer state for Renderer::ClearScreen");

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

	// TODO: For some reason this overwrites existing resource private data...
	ddesc.DepthEnable       = FALSE;
	ddesc.DepthWriteMask    = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc         = D3D11_COMPARISON_LESS;
	ddesc.StencilEnable     = FALSE;
	ddesc.StencilReadMask   = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask  = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, &resetdepthstate);
	CHECK(hr==S_OK, "Create depth state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetdepthstate, "depth stencil state for Renderer::ResetAPIState");

	// this might need to be changed once multisampling support gets added
	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, &resetraststate);
	CHECK(hr==S_OK, "Create rasterizer state for Renderer::ClearScreen");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)resetraststate, "rasterizer state for Renderer::ResetAPIState");
}

void TeardownDeviceObjects()
{
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
	FBManager.Destroy();
	D3D::font.Shutdown();
	TextureCache::Invalidate(false);
	VertexManager::DestroyDeviceObjects();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Clear();
	PixelShaderCache::Clear();

	SAFE_RELEASE(access_efb_cbuf);
	SAFE_RELEASE(cleardepthstates[0]);
	SAFE_RELEASE(cleardepthstates[1]);
	SAFE_RELEASE(clearraststate);
	SAFE_RELEASE(resetblendstate);
	SAFE_RELEASE(resetdepthstate);
	SAFE_RELEASE(resetraststate);
}

bool Renderer::Init()
{
	UpdateActiveConfig();
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

	xScale = (float)(dst_rect.right - dst_rect.left) / (float)s_XFB_width;
	yScale = (float)(dst_rect.bottom - dst_rect.top) / (float)s_XFB_height;

	s_target_width  = (int)(EFB_WIDTH * xScale);
	s_target_height = (int)(EFB_HEIGHT * yScale);

	s_Fulltarget_width  = s_target_width;
	s_Fulltarget_height = s_target_height;

	SetupDeviceObjects();

	for (unsigned int stage = 0; stage < 8; stage++)
		D3D::gfxstate->samplerdesc[stage].MaxAnisotropy = g_ActiveConfig.iMaxAnisotropy;

	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D::context->ClearRenderTargetView(FBManager.GetEFBColorTexture()->GetRTV(), ClearColor);
	D3D::context->ClearDepthStencilView(FBManager.GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)(s_Fulltarget_width - s_target_width) / 2.f,
						(float)(s_Fulltarget_height - s_target_height) / 2.f,
						(float)s_target_width, (float)s_target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBColorTexture()->GetRTV(), FBManager.GetEFBDepthTexture()->GetDSV());
	D3D::BeginFrame();
	D3D::gfxstate->rastdesc.ScissorEnable = TRUE;

	reset_called = false;
	return true;
}

void Renderer::Shutdown()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

int Renderer::GetTargetWidth() { return s_target_width; }
int Renderer::GetTargetHeight() { return s_target_height; }
int Renderer::GetFullTargetWidth() { return s_Fulltarget_width; }
int Renderer::GetFullTargetHeight() { return s_Fulltarget_height; }
float Renderer::GetTargetScaleX() { return xScale; }
float Renderer::GetTargetScaleY() { return yScale; }

int Renderer::GetFrameBufferWidth()
{
	return s_backbuffer_width;
}
int Renderer::GetFrameBufferHeight()
{
	return s_backbuffer_height;
}

// create On-Screen-Messages
void Renderer::DrawDebugText()
{
	// OSD menu messages
	if (g_ActiveConfig.bOSDHotKey)
	{
		if (OSDChoice > 0)
		{
			OSDTime = Common::Timer::GetTimeMs() + 3000;
			OSDChoice = -OSDChoice;
		}
		if ((u32)OSDTime > Common::Timer::GetTimeMs())
		{
			std::string T1 = "", T2 = "";
			std::vector<std::string> T0;

			std::string OSDM1 = StringFromFormat("%i x %i", OSDInternalW, OSDInternalH);
			std::string OSDM21;
			switch(g_ActiveConfig.iAspectRatio)
			{
			case ASPECT_AUTO:
				OSDM21 = "Auto";
				break;
			case ASPECT_FORCE_16_9:
				OSDM21 = "16:9";
				break;
			case ASPECT_FORCE_4_3:
				OSDM21 = "4:3";
				break;
			case ASPECT_STRETCH:
				OSDM21 = "Stretch";
				break;
			}
			std::string OSDM22 =
				g_ActiveConfig.bCrop ? " (crop)" : "";
			std::string OSDM3 = "Disabled";

			// if there is more text than this we will have a collission
			if (g_ActiveConfig.bShowFPS)
				{ T1 += "\n\n"; T2 += "\n\n"; }

			T0.push_back(StringFromFormat("3: Internal Resolution: %s\n", OSDM1.c_str()));
			T0.push_back(StringFromFormat("4: Aspect Ratio: %s%s\n", OSDM21.c_str(), OSDM22.c_str()));
			T0.push_back(StringFromFormat("5: Copy EFB: %s\n", OSDM3.c_str()));
			T0.push_back(StringFromFormat("6: Fog: %s\n", g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"));
			T0.push_back(StringFromFormat("7: Material Lighting: %s\n", g_ActiveConfig.bDisableLighting ? "Disabled" : "Enabled"));	

			// latest changed setting in yellow
			T1 += (OSDChoice == -1) ? T0.at(0) : "\n";
			T1 += (OSDChoice == -2) ? T0.at(1) : "\n";
			T1 += (OSDChoice == -3) ? T0.at(2) : "\n";
			T1 += (OSDChoice == -4) ? T0.at(3) : "\n";
			T1 += (OSDChoice == -5) ? T0.at(4) : "\n";

			// other settings in cyan
			T2 += (OSDChoice != -1) ? T0.at(0) : "\n";
			T2 += (OSDChoice != -2) ? T0.at(1) : "\n";
			T2 += (OSDChoice != -3) ? T0.at(2) : "\n";
			T2 += (OSDChoice != -4) ? T0.at(3) : "\n";
			T2 += (OSDChoice != -5) ? T0.at(4) : "\n";

			// render a shadow, and then the text
			Renderer::RenderText(T1.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T1.c_str(), 20, 20, 0xFFffff00);
			Renderer::RenderText(T2.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T2.c_str(), 20, 20, 0xFF00FFFF);
		}
	}
}

void Renderer::RenderText(const char* text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text, false);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	int Xstride = (s_Fulltarget_width - s_target_width) / 2;
	int Ystride = (s_Fulltarget_height - s_target_height) / 2;
	TargetRectangle result;
	result.left   = (int)(rc.left   * xScale) + Xstride;
	result.top    = (int)(rc.top    * yScale) + Ystride;
	result.right  = (int)(rc.right  * xScale) + Xstride;
	result.bottom = (int)(rc.bottom * yScale) + Ystride;
	return result;
}

void CheckForResize()
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
	if ((client_width != s_backbuffer_width || client_height != s_backbuffer_height) && 
		client_width >= 4 && client_height >= 4)
	{
		WindowResized = true;
	}
}

void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if (!fbWidth || !fbHeight)
		return;
	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	XFBWrited = true;
	// XXX: Without the VI, how would we know what kind of field this is? So
	// just use progressive.
	if (g_ActiveConfig.bUseXFB)
	{
		FBManager.CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
	}
	else
	{
		Renderer::Swap(xfbAddr, FIELD_PROGRESSIVE, fbWidth, fbHeight,sourceRc);
		Common::AtomicStoreRelease(s_swapRequested, FALSE);
	}
}

bool Renderer::SetScissorRect()
{
	int xoff = bpmem.scissorOffset.x * 2 - 342;
	int yoff = bpmem.scissorOffset.y * 2 - 342;
	D3D11_RECT rc = CD3D11_RECT(bpmem.scissorTL.x - xoff - 342,
								bpmem.scissorTL.y - yoff - 342,
								bpmem.scissorBR.x - xoff - 341,
								bpmem.scissorBR.y - yoff - 341);

	int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
	int Ystride =  (s_Fulltarget_height - s_target_height) / 2;

	rc.left   = (int)(rc.left   * xScale);
	rc.top    = (int)(rc.top    * yScale);
	rc.right  = (int)(rc.right  * xScale);
	rc.bottom = (int)(rc.bottom * yScale);

	if (rc.left < 0) rc.left = 0;
	if (rc.right < 0) rc.right = 0;
	if (rc.left > s_target_width) rc.left = s_target_width;
	if (rc.right > s_target_width) rc.right = s_target_width;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom < 0) rc.bottom = 0;
	if (rc.top > s_target_height) rc.top = s_target_height;
	if (rc.bottom > s_target_height) rc.bottom = s_target_height;

	rc.left   += Xstride;
	rc.top    += Ystride;
	rc.right  += Xstride;
	rc.bottom += Ystride;

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
		D3D::context->RSSetScissorRects(1, &rc);
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		rc = CD3D11_RECT(Xstride, Ystride, Xstride + s_target_width, Ystride + s_target_height);
		D3D::context->RSSetScissorRects(1, &rc);
		return false;
	}
	return false;
}

void Renderer::SetColorMask()
{
	UINT8 color_mask = 0;
	if (bpmem.blendmode.alphaupdate) color_mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
	if (bpmem.blendmode.colorupdate) color_mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	D3D::gfxstate->SetRenderTargetWriteMask(color_mask);
}

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	ID3D11Texture2D* read_tex;

	if (!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	// get the rectangular target region covered by the EFB pixel
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	u32 z = 0;
	float val = 0.0f;
	D3D11_RECT RectToLock = CD3D11_RECT(targetPixelRc.left, targetPixelRc.top, targetPixelRc.right, targetPixelRc.bottom);
	if (type == PEEK_Z)
	{
		// depth buffers can only be completely CopySubresourceRegion'ed, so we're using drawShadedTexQuad instead

		RectToLock.bottom+=2;
		RectToLock.right+=1;
		RectToLock.top-=1;
		RectToLock.left-=2;
		if ((RectToLock.bottom - RectToLock.top) > 4)
			RectToLock.bottom--;
		if ((RectToLock.right - RectToLock.left) > 4)
			RectToLock.left++;

		ResetAPIState(); // reset any game specific settings

		// Stretch picture with increased internal resolution
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, 4.f, 4.f);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->PSSetConstantBuffers(0, 1, &access_efb_cbuf);
		D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBDepthReadTexture()->GetRTV(), NULL);
//		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);  // TODO!
		D3D::drawShadedTexQuad(FBManager.GetEFBDepthTexture()->GetSRV(),
								&RectToLock,
								Renderer::GetFullTargetWidth(),
								Renderer::GetFullTargetHeight(),
								PixelShaderCache::GetDepthMatrixProgram(),
								VertexShaderCache::GetSimpleVertexShader(),
								VertexShaderCache::GetSimpleInputLayout());

//		D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
		D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBColorTexture()->GetRTV(), FBManager.GetEFBDepthTexture()->GetDSV());
		RestoreAPIState();
		RectToLock = CD3D11_RECT(0, 0, 4, 4);

		// copy to system memory
		D3D11_BOX box = CD3D11_BOX(0, 0, 0, 4, 4, 1);
		read_tex = FBManager.GetEFBDepthStagingBuffer();
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FBManager.GetEFBDepthReadTexture()->GetTex(), 0, &box);
	}
	else
	{
		// we can directly copy to system memory here
		read_tex = FBManager.GetEFBColorStagingBuffer();
		D3D11_BOX box = CD3D11_BOX(RectToLock.left, RectToLock.top, 0, RectToLock.right, RectToLock.bottom, 1);
		D3D::context->CopySubresourceRegion(read_tex, 0, 0, 0, 0, FBManager.GetEFBColorTexture()->GetTex(), 0, &box);
		RectToLock = CD3D11_RECT(0, 0, 1, 1);
	}

	// read the data from system memory
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(read_tex, 0, D3D11_MAP_READ, 0, &map);

	switch(type) {
		case PEEK_Z:
			val = ((float*)map.pData)[0];
			z = ((u32)(val * 0xffffff));
			break;

		case POKE_Z:
			PanicAlert("Poke Z-buffer not implemented");
			break;

		case PEEK_COLOR:
			z = ((u32*)map.pData)[0];
			break;

		case POKE_COLOR:
			PanicAlert("Poke color EFB not implemented");
			break;
	}
	D3D::context->Unmap(read_tex, 0);
	return z;
}

// Called from VertexShaderManager
void UpdateViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();
	
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
	if (sizeChanged)
	{
		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
		FBManager.Destroy();
		FBManager.Create();
		D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBColorTexture()->GetRTV(), FBManager.GetEFBDepthTexture()->GetDSV());
	}

	// some games set invalids values MinDepth and MaxDepth so fix them to the max an min allowed and let the shaders do this work
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)X, (float)Y, (float)Width, (float)Height,
										0.f,    // (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
										1.f);   //  xfregs.rawViewport[5] / 16777216.0f;
	D3D::context->RSSetViewports(1, &vp);
}
// Tino: color is passed in bgra mode so need to convert it to rgba
void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
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

void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc)
{
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.bUseRealXFB) || !fbWidth || !fbHeight)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}
	// this function is called after the XFB field is changed, not after
	// EFB is copied to XFB. In this way, flickering is reduced in games
	// and seems to also give more FPS in ZTP
	
	if (field == FIELD_LOWER) xfbAddr -= fbWidth * 2;
	u32 xfbCount = 0;
	const XFBSource** xfbSourceList = FBManager.GetXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}

	Renderer::ResetAPIState();

	// prepare copying the XFBs to our backbuffer
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

	// TODO: Enable linear filtering here
	
	if(g_ActiveConfig.bUseXFB)
	{
		const XFBSource* xfbSource;

		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = xfbSourceList[i];	
			MathUtil::Rectangle<float> sourceRc;
			
			sourceRc.left = 0;
			sourceRc.top = 0;
			sourceRc.right = xfbSource->texWidth;
			sourceRc.bottom = xfbSource->texHeight;		

			MathUtil::Rectangle<float> drawRc;

			if (g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbWidth * 2);

				drawRc.bottom = 1.0f - 2.0f * ((hOffset) / (float)fbHeight);
				drawRc.top = 1.0f - 2.0f * ((hOffset + xfbHeight) / (float)fbHeight);
				drawRc.left = -(xfbWidth / (float)fbWidth);
				drawRc.right = (xfbWidth / (float)fbWidth);
				

				if (!g_ActiveConfig.bAutoScale)
				{
					// scale draw area for a 1 to 1 pixel mapping with the draw target
					float vScale = (float)fbHeight / (float)s_backbuffer_height;
					float hScale = (float)fbWidth / (float)s_backbuffer_width;

					drawRc.top *= vScale;
					drawRc.bottom *= vScale;
					drawRc.left *= hScale;
					drawRc.right *= hScale;
				}
			}
			else
			{
				drawRc.top = -1;
				drawRc.bottom = 1;
				drawRc.left = -1;
				drawRc.right = 1;
			}
			D3D::drawShadedTexSubQuad(xfbSource->tex->GetSRV(), &sourceRc, xfbSource->texWidth, xfbSource->texHeight, &drawRc, PixelShaderCache::GetColorCopyProgram(),VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());
		}
	}
	else
	{
		TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
		D3DTexture2D* read_texture = FBManager.GetEFBColorTexture();
		D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(), PixelShaderCache::GetColorCopyProgram(),VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());		
	}
	// done with drawing the game stuff, good moment to save a screenshot
	if (s_bScreenshot)
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
		hr = PD3DX11SaveTextureToFileA(D3D::context, buftex, D3DX11_IFF_PNG, s_sScreenshotName);
		if (FAILED(hr)) PanicAlert("Failed to save screenshot");
		buftex->Release();
		s_bScreenshot = false;
	}

	// finally present some information
	if (g_ActiveConfig.bShowFPS)
	{
		char fps[20];
		StringCchPrintfA(fps, 20, "FPS: %d\n", s_fps);
		D3D::font.DrawTextScaled(0,30,20,0.0f,0xFF00FFFF,fps,false);
	}
	Renderer::DrawDebugText();

	if (g_ActiveConfig.bOverlayStats)
	{
		char buf[32768];
		Statistics::ToString(buf);
		D3D::font.DrawTextScaled(0,30,20,0.0f,0xFF00FFFF,buf,false);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		char buf[32768];
		Statistics::ToStringProj(buf);
		D3D::font.DrawTextScaled(0,30,20,0.0f,0xFF00FFFF,buf,false);
	}

	OSD::DrawMessages();
	D3D::EndFrame();
	frameCount++;
	TextureCache::Cleanup();

	// enable any configuration changes
	UpdateActiveConfig();
	WindowResized = false;
	CheckForResize();

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
	static unsigned long lasttime = Common::Timer::GetTimeMs();
	if (Common::Timer::GetTimeMs() - lasttime >= 1000) 
	{
		lasttime = Common::Timer::GetTimeMs();
		s_fps = fpscount-1;
		fpscount = 0;
	}
	if (XFBWrited) ++fpscount;

	// set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();

	// done. Show our work ;)
	D3D::Present();

	// resize the back buffers NOW to avoid flickering when resizing windows
	if (xfbchanged || WindowResized)
	{
		// TODO: Aren't we still holding a reference to the back buffer right now?
		D3D::Reset();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();

		ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

		xScale = (float)(dst_rect.right - dst_rect.left) / (float)s_XFB_width;
		yScale = (float)(dst_rect.bottom - dst_rect.top) / (float)s_XFB_height;

		s_target_width  = (int)(EFB_WIDTH * xScale);
		s_target_height = (int)(EFB_HEIGHT * yScale);

		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), NULL);
		FBManager.Destroy();
		FBManager.Create();
	}

	// begin next frame
	Renderer::RestoreAPIState();
	D3D::BeginFrame();
	D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBColorTexture()->GetRTV(), FBManager.GetEFBDepthTexture()->GetDSV());
	UpdateViewport();
	VertexShaderManager::SetViewportChanged();
	g_VideoInitialize.pCopiedToXFB(XFBWrited || g_ActiveConfig.bUseRealXFB);
	XFBWrited = false;
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

	D3D::gfxstate->samplerdesc[stage].MipLODBias = (float)tm0.lod_bias/32.0f;
	D3D::gfxstate->samplerdesc[stage].MaxLOD = (float)tm1.max_lod/16.f;
	D3D::gfxstate->samplerdesc[stage].MinLOD = (float)tm1.min_lod/16.f;
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

// Save screenshot
void Renderer::SetScreenshot(const char* filename)
{
	s_criticalScreenshot.Enter();
	strcpy_s(s_sScreenshotName, filename);
	s_bScreenshot = true;
	s_criticalScreenshot.Leave();
}

