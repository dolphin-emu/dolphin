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
#include <d3dx9.h>
#include <strsafe.h>

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
#include "Utils.h"
#include "EmuWindow.h"
#include "AVIDump.h"
#include "OnScreenDisplay.h"
#include "FramebufferManager.h"
#include "Fifo.h"
#include "TextureConverter.h"

#include "debugger/debugger.h"

int s_fps=0;

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

static float EFBxScale;
static float EFByScale;

static int s_recordWidth;
static int s_recordHeight;

static bool s_LastFrameDumped;
static bool s_AVIDumping;

static u32 s_blendMode;
static u32 s_LastAA;
static u32 s_LastEFBScale;
static bool IS_AMD;
static bool XFBWrited;

char st[32768];

static bool s_bScreenshot = false;
static Common::CriticalSection s_criticalScreenshot;
static char s_sScreenshotName[1024];
static LPDIRECT3DSURFACE9 ScreenShootMEMSurface = NULL;


// State translation lookup tables
static const D3DBLEND d3dSrcFactors[8] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA, 
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

static const D3DBLEND d3dDestFactors[8] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA, 
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

static const D3DBLENDOP d3dLogicOpop[16] =
{
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD,
	D3DBLENDOP_SUBTRACT,
	D3DBLENDOP_ADD,
	D3DBLENDOP_REVSUBTRACT,
	D3DBLENDOP_ADD,
	D3DBLENDOP_MAX,
	D3DBLENDOP_ADD,
	
	D3DBLENDOP_MAX,
	D3DBLENDOP_MAX,
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD,
	D3DBLENDOP_ADD
};

static const D3DBLEND d3dLogicOpSrcFactors[16] =
{
	D3DBLEND_ZERO,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_ONE,
	D3DBLEND_ONE,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_ZERO,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_INVDESTCOLOR,

	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_ONE,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_ONE
};

static const D3DBLEND d3dLogicOpDestFactors[16] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ZERO,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_ONE,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_ONE,

	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_ONE,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_ONE
};

static const D3DCULL d3dCullModes[4] = 
{
	D3DCULL_NONE,
	D3DCULL_CCW,
	D3DCULL_CW,
	D3DCULL_CCW
};

static const D3DCMPFUNC d3dCmpFuncs[8] = 
{
	D3DCMP_NEVER,
	D3DCMP_LESS,
	D3DCMP_EQUAL,
	D3DCMP_LESSEQUAL,
	D3DCMP_GREATER,
	D3DCMP_NOTEQUAL,
	D3DCMP_GREATEREQUAL,
	D3DCMP_ALWAYS
};

static const D3DTEXTUREFILTERTYPE d3dMipFilters[4] = 
{
	D3DTEXF_NONE,
	D3DTEXF_POINT,
	D3DTEXF_LINEAR,
	D3DTEXF_NONE, //reserved
};

static const D3DTEXTUREADDRESS d3dClamps[4] =
{
	D3DTADDRESS_CLAMP,
	D3DTADDRESS_WRAP,
	D3DTADDRESS_MIRROR,
	D3DTADDRESS_WRAP //reserved
};

void SetupDeviceObjects()
{
	D3D::font.Init();
	VertexLoaderManager::Init();
	FBManager.Create();

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();
	TextureConverter::Init();
	// Tex and shader caches will recreate themselves over time.

}

// Kill off all POOL_DEFAULT device objects.
void TeardownDeviceObjects()
{
	if(ScreenShootMEMSurface)
		ScreenShootMEMSurface->Release();
	ScreenShootMEMSurface = NULL;
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	FBManager.Destroy();
	D3D::font.Shutdown();
	TextureCache::Invalidate(false);
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Clear();
	PixelShaderCache::Clear();
	TextureConverter::Shutdown();
}

bool Renderer::Init() 
{
	UpdateActiveConfig();
	int fullScreenRes, x, y, w_temp, h_temp;
	s_blendMode = 0;
	// Multisample Anti-aliasing hasn't been implemented yet use supersamling instead
	int backbuffer_ms_mode = 0;

	g_VideoInitialize.pRequestWindowSize(x, y, w_temp, h_temp);

	for (fullScreenRes = 0; fullScreenRes < (int)D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size(); fullScreenRes++)
	{
		if ((D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].xres == w_temp) && 
			(D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].yres == h_temp))
			break;
	}
	if (fullScreenRes == D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size())
		fullScreenRes = 0;

	D3D::Create(g_ActiveConfig.iAdapter, EmuWindow::GetWnd(),
				fullScreenRes, backbuffer_ms_mode, false);

	IS_AMD = D3D::IsATIDevice();
	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	s_XFB_width = MAX_XFB_WIDTH;
	s_XFB_height = MAX_XFB_HEIGHT;

	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	if(g_ActiveConfig.bUseRealXFB)
	{
		xScale = yScale = 1.0f;
	}
	else
	{
		xScale = (float)(dst_rect.right - dst_rect.left) / (float)s_XFB_width;
		yScale = (float)(dst_rect.bottom - dst_rect.top) / (float)s_XFB_height;
	}
	
	s_LastAA = g_ActiveConfig.iMultisampleMode;
	s_LastEFBScale = g_ActiveConfig.iEFBScale;
	float SupersampleCoeficient = s_LastAA + 1;	
	switch(s_LastEFBScale)
	{
		case 0: 
			EFBxScale = xScale;
			EFByScale = yScale;
			break;
		case 1: 
			EFBxScale = ceilf(xScale);
			EFByScale = ceilf(yScale);
			break;
		default:
			EFBxScale = g_ActiveConfig.iEFBScale - 1;
			EFByScale = EFBxScale;
			break;
	};
	
	EFBxScale *= SupersampleCoeficient;
	EFByScale *= SupersampleCoeficient;

	s_target_width  = EFB_WIDTH * EFBxScale;
	s_target_height = EFB_HEIGHT * EFByScale;

	s_Fulltarget_width  = s_target_width;
	s_Fulltarget_height = s_target_height;
	
	s_LastFrameDumped = false;
	s_AVIDumping = false;

	// We're not using fixed function.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);

	SetupDeviceObjects();

	for (int stage = 0; stage < 8; stage++)
		D3D::SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, g_ActiveConfig.iMaxAnisotropy);

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 0, 0);
	
	D3D::dev->SetRenderTarget(0, FBManager.GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager.GetEFBDepthRTSurface());
	vp.X = (s_Fulltarget_width - s_target_width) / 2;
	vp.Y = (s_Fulltarget_height - s_target_height) / 2;
	vp.Width  = s_target_width;
	vp.Height = s_target_height;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x0, 1.0f, 0);
	D3D::BeginFrame();
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	D3D::dev->CreateOffscreenPlainSurface(s_backbuffer_width,s_backbuffer_height, FBManager.GetEFBColorRTSurfaceFormat(), D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );
	return true;
}

void Renderer::Shutdown()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
	
	if (s_AVIDumping)
	{
		AVIDump::Stop();
	}
}

int Renderer::GetTargetWidth() { return s_target_width; }
int Renderer::GetTargetHeight() { return s_target_height; }
int Renderer::GetFullTargetWidth() { return s_Fulltarget_width; }
int Renderer::GetFullTargetHeight() { return s_Fulltarget_height; }
float Renderer::GetTargetScaleX() { return EFBxScale; }
float Renderer::GetTargetScaleY() { return EFByScale; }

float Renderer::GetXFBScaleX() { return xScale; }
float Renderer::GetXFBScaleY() { return yScale; }


int Renderer::GetFrameBufferWidth()
{
	return s_backbuffer_width;
}
int Renderer::GetFrameBufferHeight()
{
	return s_backbuffer_height;
}

// Create On-Screen-Messages
void Renderer::DrawDebugText()
{
	// OSD Menu messages
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
			std::string OSDM3 = g_ActiveConfig.bEFBCopyDisable ? "Disabled" :
				g_ActiveConfig.bCopyEFBToTexture ? "To Texture" : "To RAM";

			// If there is more text than this we will have a collission
			if (g_ActiveConfig.bShowFPS)
				{ T1 += "\n\n"; T2 += "\n\n"; }

			// The rows
			T0.push_back(StringFromFormat("3: Internal Resolution: %s\n", OSDM1.c_str()));
			T0.push_back(StringFromFormat("4: Aspect Ratio: %s%s\n", OSDM21.c_str(), OSDM22.c_str()));
			T0.push_back(StringFromFormat("5: Copy EFB: %s\n", OSDM3.c_str()));
			T0.push_back(StringFromFormat("6: Fog: %s\n", g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"));
			T0.push_back(StringFromFormat("7: Material Lighting: %s\n", g_ActiveConfig.bDisableLighting ? "Disabled" : "Enabled"));	

			// The latest changed setting in yellow
			T1 += (OSDChoice == -1) ? T0.at(0) : "\n";
			T1 += (OSDChoice == -2) ? T0.at(1) : "\n";
			T1 += (OSDChoice == -3) ? T0.at(2) : "\n";
			T1 += (OSDChoice == -4) ? T0.at(3) : "\n";
			T1 += (OSDChoice == -5) ? T0.at(4) : "\n";

			// The other settings in cyan
			T2 += (OSDChoice != -1) ? T0.at(0) : "\n";
			T2 += (OSDChoice != -2) ? T0.at(1) : "\n";
			T2 += (OSDChoice != -3) ? T0.at(2) : "\n";
			T2 += (OSDChoice != -4) ? T0.at(3) : "\n";
			T2 += (OSDChoice != -5) ? T0.at(4) : "\n";

			// Render a shadow, and then the text
			Renderer::RenderText(T1.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T1.c_str(), 20, 20, 0xFFffff00);
			Renderer::RenderText(T2.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T2.c_str(), 20, 20, 0xFF00FFFF);
		}
	}
}

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text, false);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	int Xstride = (s_Fulltarget_width - s_target_width) / 2;
	int Ystride = (s_Fulltarget_height - s_target_height) / 2;
	TargetRectangle result;
	result.left   = (int)(rc.left   * EFBxScale) + Xstride;
	result.top    = (int)(rc.top    * EFByScale) + Ystride;
	result.right  = (int)(rc.right  * EFBxScale) + Xstride;
	result.bottom = (int)(rc.bottom * EFByScale) + Ystride;
	return result;
}

void formatBufferDump(const char *in, char *out, int w, int h, int p)
{
	for (int y = 0; y < h; y++)
	{
		const char *line = in + (h - y - 1) * p;
		for (int x = 0; x < w; x++)
		{
			memcpy(out, line, 3);
			out += 3;
			line += 4;
		}
	}
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
void CheckForResize()
{
	while (EmuWindow::IsSizing())
	{
		Sleep(10);
	}

	
	if (EmuWindow::GetParentWnd())
	{
		// Re-stretch window to parent window size again, if it has a parent window.
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
	// Sanity check.
	if ((client_width != s_backbuffer_width ||
		client_height != s_backbuffer_height) && 
		client_width >= 4 && client_height >= 4)
	{
		TeardownDeviceObjects();

		D3D::Reset();		
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();
		WindowResized = true;
	}
}



void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if(!fbWidth || !fbHeight)
		return;
	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	FBManager.CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
	XFBWrited = true;
	// XXX: Without the VI, how would we know what kind of field this is? So
	// just use progressive.
	if (!g_ActiveConfig.bUseXFB)
	{
		Renderer::Swap(xfbAddr, FIELD_PROGRESSIVE, fbWidth, fbHeight);
		Common::AtomicStoreRelease(s_swapRequested, FALSE);
	}
}

bool Renderer::SetScissorRect()
{
	int xoff = bpmem.scissorOffset.x * 2 - 342;
	int yoff = bpmem.scissorOffset.y * 2 - 342;
	RECT rc;
	rc.left   = (int)((float)bpmem.scissorTL.x - xoff - 342);
	rc.top    = (int)((float)bpmem.scissorTL.y - yoff - 342);
	rc.right  = (int)((float)bpmem.scissorBR.x - xoff - 341);
	rc.bottom = (int)((float)bpmem.scissorBR.y - yoff - 341);

	if (rc.left < 0) rc.left = 0;
	if (rc.right < 0) rc.right = 0;
	if (rc.left > EFB_WIDTH) rc.left = EFB_WIDTH;
	if (rc.right > EFB_WIDTH) rc.right = EFB_WIDTH;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom < 0) rc.bottom = 0;
	if (rc.top > EFB_HEIGHT) rc.top = EFB_HEIGHT;
	if (rc.bottom > EFB_HEIGHT) rc.bottom = EFB_HEIGHT;

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

	int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
	int Ystride =  (s_Fulltarget_height - s_target_height) / 2;

	rc.left   = (int)(rc.left   * EFBxScale) + Xstride;
	rc.top    = (int)(rc.top    * EFByScale) + Ystride;
	rc.right  = (int)(rc.right  * EFBxScale) + Xstride;
	rc.bottom = (int)(rc.bottom * EFByScale) + Ystride;	

	if (rc.right != rc.left && rc.bottom != rc.top)
	{
		D3D::dev->SetScissorRect(&rc);
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		rc.left   = Xstride;
		rc.top    = Ystride;
		rc.right  = Xstride + s_target_width;
		rc.bottom = Ystride + s_target_height;
		D3D::dev->SetScissorRect(&rc);
		return false;
	}
	return false;
}

void Renderer::SetColorMask() 
{
	DWORD color_mask = 0;
	if (bpmem.blendmode.alphaupdate) 
		color_mask = D3DCOLORWRITEENABLE_ALPHA;
	if (bpmem.blendmode.colorupdate) 
		color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
}

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	if (!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	//Get the working buffer
	LPDIRECT3DSURFACE9 pBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager.GetEFBDepthRTSurface() : FBManager.GetEFBColorRTSurface();
	//get the temporal buffer to move 1pixel data
	LPDIRECT3DSURFACE9 RBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager.GetEFBDepthReadSurface() : FBManager.GetEFBColorReadSurface();
	//get the memory buffer that can be locked
	LPDIRECT3DSURFACE9 pOffScreenBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager.GetEFBDepthOffScreenRTSurface() : FBManager.GetEFBColorOffScreenRTSurface();
	//get the buffer format
	D3DFORMAT BufferFormat = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager.GetEFBDepthRTSurfaceFormat() : FBManager.GetEFBColorRTSurfaceFormat();
	D3DFORMAT ReadBufferFormat = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager.GetEFBDepthReadSurfaceFormat() : BufferFormat;
	
	if (BufferFormat == D3DFMT_D24X8)
		return 0;

	D3DLOCKED_RECT drect;
	
	//Buffer not found alert
	if (!pBuffer) {
		PanicAlert("No %s!", (type == PEEK_Z || type == POKE_Z) ? "Z-Buffer" : "Color EFB");
		return 0;
	}
	// Get the rectangular target region covered by the EFB pixel.
	
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	u32 z = 0;
	float val = 0.0f;
	HRESULT hr;
	RECT RectToLock;
	RectToLock.bottom = targetPixelRc.bottom;
	RectToLock.left = targetPixelRc.left;
	RectToLock.right = targetPixelRc.right;
	RectToLock.top = targetPixelRc.top;	
	if (type == PEEK_Z)
	{
		RECT PixelRect;
		PixelRect.bottom = 4;
		PixelRect.left = 0;
		PixelRect.right = 4;
		PixelRect.top = 0;
		RectToLock.bottom+=2;
		RectToLock.right+=1;
		RectToLock.top-=1;
		RectToLock.left-=2;
		if ((RectToLock.bottom - RectToLock.top) > 4)
			RectToLock.bottom--;
		if ((RectToLock.right - RectToLock.left) > 4)
			RectToLock.left++;
		ResetAPIState(); // reset any game specific settings
		hr =D3D::dev->SetDepthStencilSurface(NULL);
		hr = D3D::dev->SetRenderTarget(0, RBuffer);
		if (FAILED(hr))
		{
			PanicAlert("unable to set pixel render buffer");
			return 0;
		}
		D3DVIEWPORT9 vp;
		// Stretch picture with increased internal resolution
		vp.X = 0;
		vp.Y = 0;
		vp.Width  = 4;
		vp.Height = 4;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		hr = D3D::dev->SetViewport(&vp);
		if (FAILED(hr))
		{
			PanicAlert("unable to set pixel viewport");
			return 0;
		}
		float colmat[16]= {0.0f};
		float fConstAdd[4] = {0.0f};
		colmat[0] = colmat[5] = colmat[10] = 1.0f;
		PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
		EFBRectangle source_rect;
		LPDIRECT3DTEXTURE9 read_texture = FBManager.GetEFBDepthTexture(source_rect);
		
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);		

		D3D::drawShadedTexQuad(
			read_texture,
			&RectToLock, 
			Renderer::GetFullTargetWidth() , 
			Renderer::GetFullTargetHeight(),
			4,4,
			(BufferFormat == FOURCC_RAWZ)?PixelShaderCache::GetColorMatrixProgram(0):PixelShaderCache::GetDepthMatrixProgram(0),
			VertexShaderCache::GetSimpleVertexShader(0));

		D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);

		hr = D3D::dev->SetRenderTarget(0, FBManager.GetEFBColorRTSurface());
		hr = D3D::dev->SetDepthStencilSurface(FBManager.GetEFBDepthRTSurface());
		RestoreAPIState();
		RectToLock.bottom = 4;
		RectToLock.left = 0;
		RectToLock.right = 4;
		RectToLock.top = 0;

	}
	else
	{
		hr = D3D::dev->StretchRect(pBuffer,&RectToLock,RBuffer,NULL, D3DTEXF_NONE);
		//change the rect to lock the entire one pixel buffer
		RectToLock.bottom = 1;
		RectToLock.left = 0;
		RectToLock.right = 1;
		RectToLock.top = 0;
	}
	if (FAILED(hr))
	{
		PanicAlert("Unable to stretch data to buffer");
		return 0;
	}
	//retriebe the pixel data to the local memory buffer
	D3D::dev->GetRenderTargetData(RBuffer,pOffScreenBuffer);
	if (FAILED(hr))
	{
		PanicAlert("Unable to copy data to mem buffer");
		return 0;
	}
	
		
	
		
	//the surface is good.. lock it
	if ((hr = pOffScreenBuffer->LockRect(&drect, &RectToLock, D3DLOCK_READONLY)) != D3D_OK)
	{
		PanicAlert("ERROR: %s", hr == D3DERR_WASSTILLDRAWING ? "Still drawing" :
											  hr == D3DERR_INVALIDCALL ? "Invalid call" : "w00t");
		return 0;
	}
		
	switch(type) {
		case PEEK_Z:
			{
				switch (ReadBufferFormat)
				{
				case D3DFMT_R32F:
					val = ((float *)drect.pBits)[6];
					break;
				default:
					float ffrac = 1.0f/255.0f;
					z = ((u32 *)drect.pBits)[6];
					val =	((float)((z>>16) & 0xFF)) * ffrac;
					ffrac*= 1 / 255.0f;
					val +=	((float)((z>>8) & 0xFF)) * ffrac;
					ffrac*= 1 / 255.0f;
					val +=	((float)(z & 0xFF)) * ffrac;
					break;
				};
				z = ((u32)(val * 0xffffff));
			}
			break;
		case POKE_Z:
			// TODO: Get that Z value to poke from somewhere
			//((float *)drect.pBits)[0] = val;
			PanicAlert("Poke Z-buffer not implemented");
			break;

		case PEEK_COLOR:
			z = ((u32 *)drect.pBits)[0];
			break;
		case POKE_COLOR:
			// TODO: Get that ARGB value to poke from somewhere
			//((float *)drect.pBits)[0] = val;
			PanicAlert("Poke color EFB not implemented");
			break;
	}


	pOffScreenBuffer->UnlockRect();	
	// TODO: in RE0 this value is often off by one, which causes lighting to disappear
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

	D3DVIEWPORT9 vp;

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
	if(X < 0)
	{
		s_Fulltarget_width -= 2 * X;
		X = 0;
		sizeChanged=true;
	}
	if(Y < 0)
	{
		s_Fulltarget_height -= 2 * Y;
		Y = 0;
		sizeChanged=true;
	}
	if(!IS_AMD)
	{
		if(X + Width > s_Fulltarget_width)
		{
			s_Fulltarget_width += (X + Width - s_Fulltarget_width) * 2;
			sizeChanged=true;
		}
		if(Y + Height > s_Fulltarget_height)
		{
			s_Fulltarget_height += (Y + Height - s_Fulltarget_height) * 2;
			sizeChanged=true;
		}
	}
	if(sizeChanged)
	{
		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
		FBManager.Destroy();
		FBManager.Create();
		D3D::dev->SetRenderTarget(0, FBManager.GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FBManager.GetEFBDepthRTSurface());
	}	
	vp.X = X;
	vp.Y = Y;
	vp.Width = Width;
	vp.Height = Height;
	
	//some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
	vp.MinZ = 0.0f;//(xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
	vp.MaxZ =1.0f;// xfregs.rawViewport[5] / 16777216.0f;
	D3D::dev->SetViewport(&vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{	
	// Update the view port for clearing the picture

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;	
	vp.Width  = Renderer::GetFullTargetWidth();
	vp.Height = Renderer::GetFullTargetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);	

	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);

	// Always set the scissor in case it was set by the game and has not been reset
	RECT sirc;
	sirc.left   = targetRc.left;
	sirc.top    = targetRc.top;
	sirc.right  = targetRc.right;
	sirc.bottom = targetRc.bottom;
	D3D::dev->SetScissorRect(&sirc);
	if (zEnable)
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	D3D::drawClearQuad(color ,(z & 0xFFFFFF) / float(0xFFFFFF),PixelShaderCache::GetClearProgram(),VertexShaderCache::GetClearVertexShader());
	if (zEnable)
		D3D::RefreshRenderState(D3DRS_ZFUNC);	
	UpdateViewport();
	SetScissorRect();
}

void Renderer::SetBlendMode(bool forceUpdate)
{	
	// blend mode bit mask
	// 0 - blend enable
	// 2 - reverse subtract enable (else add)
	// 3-5 - srcRGB function
	// 6-8 - dstRGB function
	if (bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != 3)
		return;
	u32 newval = bpmem.blendmode.subtract << 2;

	if (bpmem.blendmode.subtract) {
		newval |= 0x0049;   // enable blending src 1 dst 1
	} else if (bpmem.blendmode.blendenable) {
		newval |= 1;    // enable blending
		newval |= bpmem.blendmode.srcfactor << 3;
		newval |= bpmem.blendmode.dstfactor << 6;
	}
	
	u32 changes = forceUpdate ? 0xFFFFFFFF : newval ^ s_blendMode;

	if (changes & 1) {
		// blend enable change
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, (newval & 1));
		
	}

	if (changes & 4) {
		// subtract enable change
		D3D::SetRenderState(D3DRS_BLENDOP, newval & 4 ? D3DBLENDOP_REVSUBTRACT : D3DBLENDOP_ADD);
	}

	if (changes & 0x1F8) {
		// blend RGB change
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dSrcFactors[(newval >> 3) & 7]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dDestFactors[(newval >> 6) & 7]);
	}

	s_blendMode = newval;
}



void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
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
	if (!xfbSourceList || xfbCount == 0)
	{
		g_VideoInitialize.pCopiedToXFB(false);	
		return;
	}	

	Renderer::ResetAPIState();
	// Set the backbuffer as the rendering target
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	
	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0,NULL, D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0f,0);

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
	vp.X = X;
	vp.Y = Y;
	vp.Width = Width;
	vp.Height = Height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;

	D3D::dev->SetViewport(&vp);

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

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

		D3D::drawShadedTexSubQuad(xfbSource->texture,&sourceRc,xfbSource->texWidth,xfbSource->texHeight,&drawRc,Width,Height,PixelShaderCache::GetColorCopyProgram(0),VertexShaderCache::GetSimpleVertexShader(0));
	}

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);	
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	if(s_bScreenshot)
	{
		s_criticalScreenshot.Enter();
		// create a R8G8B8 surface for the screenshot (no alpha channel)
		// otherwise funky screenshots get saved
		LPDIRECT3DSURFACE9 screenshot_surface;
		if (D3D_OK == D3D::dev->CreateOffscreenPlainSurface(s_backbuffer_width, s_backbuffer_height, D3DFMT_R8G8B8, D3DPOOL_SCRATCH, &screenshot_surface, NULL))
		{
			D3DXLoadSurfaceFromSurface(screenshot_surface, NULL, NULL, D3D::GetBackBufferSurface(), NULL, NULL, D3DX_DEFAULT, 0);
			D3DXSaveSurfaceToFileA(s_sScreenshotName, D3DXIFF_PNG, screenshot_surface, NULL, NULL);
			screenshot_surface->Release();
		}
		else
			PanicAlert("Failed to create surface for screenshot!");
		
		s_bScreenshot = false;
		s_criticalScreenshot.Leave();
	}
	if (g_ActiveConfig.bDumpFrames) 
	{
		D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
		if (!s_LastFrameDumped) 
		{
			s_recordWidth = s_backbuffer_width;
			s_recordHeight = s_backbuffer_height;
			s_AVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), s_recordWidth, s_recordHeight);
			if (!s_AVIDumping) 
			{
				PanicAlert("Error dumping frames to AVI.");
			} 
			else 
			{
				char msg [255];
				sprintf_s(msg,255, "Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)", File::GetUserPath(D_DUMPFRAMES_IDX), s_recordWidth, s_recordHeight);
				OSD::AddMessage(msg, 2000);
			}
		}
		if (s_AVIDumping) 
		{
			D3DLOCKED_RECT rect;
			if (SUCCEEDED(ScreenShootMEMSurface->LockRect(&rect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY))) 
			{
				char *data = (char *)malloc(3 * s_recordWidth * s_recordHeight);
				formatBufferDump((const char *)rect.pBits, data, s_recordWidth, s_recordHeight, rect.Pitch);
				AVIDump::AddFrame(data);
				free(data);
				ScreenShootMEMSurface->UnlockRect();
			}
		}
		s_LastFrameDumped = true;
	}		 
	else 
	{
		if (s_LastFrameDumped && s_AVIDumping) 
		{
			AVIDump::Stop();
			s_AVIDumping = false;
		}
		s_LastFrameDumped = false;
	}
	

	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bShowFPS)
	{
		char fps[20];
		StringCchPrintfA(fps, 20, "FPS: %d\n", s_fps);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,fps,false);
	}
	Renderer::DrawDebugText();

	if (g_ActiveConfig.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		Statistics::ToStringProj(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}

	OSD::DrawMessages();
	
	D3D::EndFrame();

	frameCount++;
	TextureCache::Cleanup();

	// Make any new configuration settings active.
	UpdateActiveConfig();	
	WindowResized = false;
	CheckForResize();
	
	bool xfbchanged = false;
	
	if(s_XFB_width != fbWidth || s_XFB_height != fbHeight)
	{
		xfbchanged = true;
		s_XFB_width = fbWidth;
		s_XFB_height = fbHeight;
		if(s_XFB_width < 1) s_XFB_width = MAX_XFB_WIDTH;
		if(s_XFB_width > MAX_XFB_WIDTH) s_XFB_width = MAX_XFB_WIDTH;
		if(s_XFB_height < 1) s_XFB_height = MAX_XFB_HEIGHT;
		if(s_XFB_height > MAX_XFB_HEIGHT) s_XFB_height = MAX_XFB_HEIGHT;

	}

	u32 newAA = g_ActiveConfig.iMultisampleMode;
	u32 newEFBScale = g_ActiveConfig.iEFBScale;
	if(newAA != s_LastAA || newEFBScale != s_LastEFBScale || xfbchanged || WindowResized)
	{	
		s_LastAA = newAA;
		s_LastEFBScale = newEFBScale;

		ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

		if(g_ActiveConfig.bUseRealXFB)
		{
			xScale = yScale = 1.0f;
		}
		else
		{
			xScale = (float)(dst_rect.right - dst_rect.left) / (float)s_XFB_width;
			yScale = (float)(dst_rect.bottom - dst_rect.top) / (float)s_XFB_height;
		}
		
		float SupersampleCoeficient = s_LastAA + 1;	
		switch(s_LastEFBScale)
		{
			case 0: 
				EFBxScale = xScale;
				EFByScale = yScale;
				break;
			case 1: 
				EFBxScale = ceilf(xScale);
				EFByScale = ceilf(yScale);
				break;
			default:
				EFBxScale = g_ActiveConfig.iEFBScale - 1;
				EFByScale = EFBxScale;
				break;
		};
		
		EFBxScale *= SupersampleCoeficient;
		EFByScale *= SupersampleCoeficient;
		s_target_width  = EFB_WIDTH * EFBxScale;
		s_target_height = EFB_HEIGHT * EFByScale;
		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
		if(WindowResized)
		{
			SetupDeviceObjects();
		}
		else
		{
			FBManager.Destroy();
			FBManager.Create();
		}
		D3D::dev->SetRenderTarget(0, FBManager.GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FBManager.GetEFBDepthRTSurface());
		
	}

	// ---------------------------------------------------------------------
	// Count FPS.
	// -------------
	static int fpscount = 1;
	static unsigned long lasttime;
	if(XFBWrited)
		++fpscount;
	if (Common::Timer::GetTimeMs() - lasttime > 1000) 
	{
		lasttime = Common::Timer::GetTimeMs();
		s_fps = fpscount - 1;
		fpscount = 1;
	}

	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();
	D3D::BeginFrame();
	Renderer::RestoreAPIState();
	D3D::dev->SetRenderTarget(0, FBManager.GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager.GetEFBDepthRTSurface());
	UpdateViewport();
	VertexShaderManager::SetViewportChanged();
	g_VideoInitialize.pCopiedToXFB(XFBWrited);
	XFBWrited = false;
}

void Renderer::ResetAPIState()
{
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	D3D::SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	DWORD color_mask = D3DCOLORWRITEENABLE_ALPHA| D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	UpdateViewport();
	SetScissorRect();
	if (bpmem.zmode.testenable) D3D::SetRenderState(D3DRS_ZENABLE, TRUE);
	if (bpmem.zmode.updateenable)   D3D::SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	SetColorMask();
	SetLogicOpMode();
}

void Renderer::SetGenerationMode()
{
	D3D::SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);
}

void Renderer::SetDepthMode()
{
	if (bpmem.zmode.testenable)
	{
		D3D::SetRenderState(D3DRS_ZENABLE, TRUE);
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
		D3D::SetRenderState(D3DRS_ZFUNC, d3dCmpFuncs[bpmem.zmode.func]);
	}
	else
	{
		D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);  // ??
	}
}

void Renderer::SetLogicOpMode()
{
	if (bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != 3) 
	{
		s_blendMode = 0;
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
		D3D::SetRenderState(D3DRS_BLENDOP, d3dLogicOpop[bpmem.blendmode.logicmode]);
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dLogicOpSrcFactors[bpmem.blendmode.logicmode]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dLogicOpDestFactors[bpmem.blendmode.logicmode]);
	}
	else
	{
		SetBlendMode(true);
	}
}

void Renderer::SetDitherMode()
{
	D3D::SetRenderState(D3DRS_DITHERENABLE,bpmem.blendmode.dither);
}

void Renderer::SetLineWidth()
{
	// We can't change line width in D3D unless we use ID3DXLine
	float fratio = xfregs.rawViewport[0] != 0 ? Renderer::GetTargetScaleX() : 1.0f;
	float psize = bpmem.lineptwidth.linesize * fratio / 6.0f;
	D3D::SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&psize));
}

void Renderer::SetSamplerState(int stage, int texindex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];	
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];
	
	D3DTEXTUREFILTERTYPE min, mag, mip;
	if (g_ActiveConfig.bForceFiltering)
	{
		min = mag = mip = D3DTEXF_LINEAR;
	}
	else
	{
		min = (tm0.min_filter & 4) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mag = tm0.mag_filter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mip = (tm0.min_filter == 8)?D3DTEXF_NONE:d3dMipFilters[tm0.min_filter & 3];
		if((tm0.min_filter & 3) && (tm0.min_filter != 8) && ((tm1.max_lod >> 4) == 0)) mip = D3DTEXF_NONE;
	}
	if (texindex)
		stage += 4;	
	
	if (mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR &&
		g_ActiveConfig.iMaxAnisotropy > 1)
	{
		min = D3DTEXF_ANISOTROPIC;
	}
	D3D::SetSamplerState(stage, D3DSAMP_MINFILTER, min);
	D3D::SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
	D3D::SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
	
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dClamps[tm0.wrap_s]);
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dClamps[tm0.wrap_t]);
	float lodbias = tm0.lod_bias / 32.0f;
	D3D::SetSamplerState(stage,D3DSAMP_MIPMAPLODBIAS,*(DWORD*)&lodbias);
	D3D::SetSamplerState(stage,D3DSAMP_MAXMIPLEVEL,tm1.min_lod>>4);	
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

// Save screenshot
void Renderer::SetScreenshot(const char *filename)
{
	s_criticalScreenshot.Enter();
	strcpy_s(s_sScreenshotName,filename);
	s_bScreenshot = true;
	s_criticalScreenshot.Leave();
}
