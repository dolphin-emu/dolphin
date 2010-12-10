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
#include "TextureConverter.h"
#include "DLCache.h"
#include "Debugger.h"

static int s_fps = 0;

static int s_recordWidth;
static int s_recordHeight;

static bool s_bLastFrameDumped;
static bool s_bAVIDumping;

static u32 s_blendMode;
static u32 s_LastAA;
static bool IS_AMD;

static char *st;

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
	g_framebuffer_manager = new FramebufferManager;

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();
	TextureConverter::Init();
	
	// To avoid shader compilation stutters, read back all shaders from cache.
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	// Texture cache will recreate themselves over time.
}

// Kill off all POOL_DEFAULT device objects.
void TeardownDeviceObjects()
{
	if(ScreenShootMEMSurface)
		ScreenShootMEMSurface->Release();
	ScreenShootMEMSurface = NULL;
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	delete g_framebuffer_manager;
	D3D::font.Shutdown();
	TextureCache::Invalidate(false);
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	PixelShaderCache::Shutdown();
	TextureConverter::Shutdown();
}

namespace DX9
{

// Init functions
Renderer::Renderer()
{
	st = new char[32768];

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

	// Decide frambuffer size
	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	s_XFB_width = MAX_XFB_WIDTH;
	s_XFB_height = MAX_XFB_HEIGHT;

	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	CalculateXYScale(dst_rect);
	
	s_LastAA = g_ActiveConfig.iMultisampleMode;
	int SupersampleCoeficient = s_LastAA + 1;

	s_LastEFBScale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize(SupersampleCoeficient);
	
	s_bLastFrameDumped = false;
	s_bAVIDumping = false;

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
	
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	vp.X = (s_Fulltarget_width - s_target_width) / 2;
	vp.Y = (s_Fulltarget_height - s_target_height) / 2;
	vp.Width  = s_target_width;
	vp.Height = s_target_height;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x0, 1.0f, 0);
	D3D::BeginFrame();
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	D3D::dev->CreateOffscreenPlainSurface(s_backbuffer_width,s_backbuffer_height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );
}

Renderer::~Renderer()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
	
	if (s_bAVIDumping)
	{
		AVIDump::Stop();
	}
	delete[] st;
}

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text);
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

namespace DX9
{

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
		TeardownDeviceObjects();

		D3D::Reset();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();
		if(ScreenShootMEMSurface)
			ScreenShootMEMSurface->Release();
		D3D::dev->CreateOffscreenPlainSurface(Renderer::GetBackbufferWidth(), Renderer::GetBackbufferHeight(),
			D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );

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

	rc.left   = EFBToScaledX(rc.left) + TargetStrideX();
	rc.top    = EFBToScaledY(rc.top) + TargetStrideY();
	rc.right  = EFBToScaledX(rc.right) + TargetStrideX();
	rc.bottom = EFBToScaledY(rc.bottom) + TargetStrideY();

	// Check that the coordinates are good
	if (rc.right != rc.left && rc.bottom != rc.top)
	{
		D3D::dev->SetScissorRect(rc.AsRECT());
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		rc.left   = TargetStrideX();
		rc.top    = TargetStrideY();
		rc.right  = TargetStrideX() + s_target_width;
		rc.bottom = TargetStrideY() + s_target_height;
		D3D::dev->SetScissorRect(rc.AsRECT());
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

	// We're using three surfaces here:
	// - pEFBSurf: EFB Surface. Source surface when peeking, destination surface when poking.
	// - pBufferRT: A render target surface. When peeking, we render a textured quad to this surface.
	// - pSystemBuf: An offscreen surface. Used to retrieve the pixel data from pBufferRT.
	LPDIRECT3DSURFACE9 pEFBSurf, pBufferRT, pSystemBuf;
	if(type == PEEK_Z || type == POKE_Z)
	{
		pEFBSurf = FramebufferManager::GetEFBDepthRTSurface();
		pBufferRT = FramebufferManager::GetEFBDepthReadSurface();
		pSystemBuf = FramebufferManager::GetEFBDepthOffScreenRTSurface();
	}
	else //if(type == PEEK_COLOR || type == POKE_COLOR)
	{
		pEFBSurf = FramebufferManager::GetEFBColorRTSurface();
		pBufferRT = FramebufferManager::GetEFBColorReadSurface();
		pSystemBuf = FramebufferManager::GetEFBColorOffScreenRTSurface();
	}

	// Buffer not found alert
	if (!pEFBSurf) {
		PanicAlert("No %s!", (type == PEEK_Z || type == POKE_Z) ? "Z-Buffer" : "Color EFB");
		return 0;
	}

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = ConvertEFBRectangle(efbPixelRc);

	HRESULT hr;
	RECT RectToLock;
	RectToLock.bottom = targetPixelRc.bottom;
	RectToLock.left = targetPixelRc.left;
	RectToLock.right = targetPixelRc.right;
	RectToLock.top = targetPixelRc.top;	
	if (type == PEEK_Z)
	{
		if (FramebufferManager::GetEFBDepthRTSurfaceFormat() == D3DFMT_D24X8)
			return 0;

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

		ResetAPIState(); // Reset any game specific settings
		D3D::dev->SetDepthStencilSurface(NULL);
		D3D::dev->SetRenderTarget(0, pBufferRT);

		// Stretch picture with increased internal resolution
		D3DVIEWPORT9 vp;
		vp.X = 0;
		vp.Y = 0;
		vp.Width  = 4;
		vp.Height = 4;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		D3D::dev->SetViewport(&vp);

		float colmat[16] = {0.0f};
		float fConstAdd[4] = {0.0f};
		colmat[0] = colmat[5] = colmat[10] = 1.0f;
		PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBDepthTexture();
		
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

		D3D::drawShadedTexQuad(
			read_texture,
			&RectToLock,
			Renderer::GetFullTargetWidth(),
			Renderer::GetFullTargetHeight(),
			4, 4,
			(FramebufferManager::GetEFBDepthRTSurfaceFormat() == FOURCC_RAWZ) ? PixelShaderCache::GetColorMatrixProgram(0) : PixelShaderCache::GetDepthMatrixProgram(0),
			VertexShaderCache::GetSimpleVertexShader(0));

		D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);

		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		RestoreAPIState();

		// Retrieve the pixel data to the local memory buffer
		RectToLock.bottom = 4;
		RectToLock.left = 0;
		RectToLock.right = 4;
		RectToLock.top = 0;
		D3D::dev->GetRenderTargetData(pBufferRT, pSystemBuf);

		// EFB data successfully retrieved, now get the pixel data
		D3DLOCKED_RECT drect;
		pSystemBuf->LockRect(&drect, &RectToLock, D3DLOCK_READONLY);

		float val = 0.0f;
		u32 z = 0;

		switch (FramebufferManager::GetEFBDepthReadSurfaceFormat())
		{
		case D3DFMT_R32F:
			val = ((float*)drect.pBits)[6];
			break;
		default:
			float ffrac = 1.0f/255.0f;
			z = ((u32*)drect.pBits)[6];
			val =	((float)((z>>16) & 0xFF)) * ffrac;
			ffrac*= 1 / 255.0f;
			val +=	((float)((z>>8) & 0xFF)) * ffrac;
			ffrac*= 1 / 255.0f;
			val +=	((float)(z & 0xFF)) * ffrac;
			break;
		};
		z = ((u32)(val * 0xffffff));

		pSystemBuf->UnlockRect();
		// TODO: in RE0 this value is often off by one, which causes lighting to disappear
		return z;
	}
	else if(type == PEEK_COLOR)
	{
		// TODO: Can't we directly StretchRect to System buf?
		hr = D3D::dev->StretchRect(pEFBSurf, &RectToLock, pBufferRT, NULL, D3DTEXF_NONE);
		D3D::dev->GetRenderTargetData(pBufferRT, pSystemBuf);

		// EFB data successfully retrieved, now get the pixel data
		RectToLock.bottom = 1;
		RectToLock.left = 0;
		RectToLock.right = 1;
		RectToLock.top = 0;
		D3DLOCKED_RECT drect;
		pSystemBuf->LockRect(&drect, &RectToLock, D3DLOCK_READONLY);

		u32 ret = ((u32*)drect.pBits)[0];
		pSystemBuf->UnlockRect();

		// check what to do with the alpha channel (GX_PokeAlphaRead)
		PixelEngine::UPEAlphaReadReg alpha_read_mode;
		PixelEngine::Read16((u16&)alpha_read_mode, PE_DSTALPHACONF);
		if(alpha_read_mode.ReadMode == 2) return ret; // GX_READ_NONE
		else if(alpha_read_mode.ReadMode == 1) return (ret | 0xFF000000); // GX_READ_FF
		else /*if(alpha_read_mode.ReadMode == 0)*/ return (ret & 0x00FFFFFF); // GX_READ_00
	}
	else //if(type == POKE_COLOR)
	{
		// TODO: Speed this up by batching pokes?
		ResetAPIState();
		D3D::drawColorQuad(poke_data, (float)RectToLock.left   * 2.f / (float)Renderer::GetFullTargetWidth()  - 1.f,
									- (float)RectToLock.top    * 2.f / (float)Renderer::GetFullTargetHeight() + 1.f,
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

	int Xstride =  (Renderer::GetFullTargetWidth() - Renderer::GetTargetWidth()) / 2;
	int Ystride =  (Renderer::GetFullTargetHeight() - Renderer::GetTargetHeight()) / 2;

	// TODO: ceil, floor or just cast to int?
	int X = EFBToScaledX((int)ceil(xfregs.rawViewport[3] - xfregs.rawViewport[0] - scissorXOff)) + TargetStrideX();
	int Y = EFBToScaledY((int)ceil(xfregs.rawViewport[4] + xfregs.rawViewport[1] - scissorYOff)) + TargetStrideY();
	int Width = EFBToScaledX((int)ceil(2.0f * xfregs.rawViewport[0]));
	int Height = EFBToScaledY((int)ceil(-2.0f * xfregs.rawViewport[1]));
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
		sizeChanged = true;
	}
	if (!IS_AMD)
	{
		if(X + Width > Renderer::GetFullTargetWidth())
		{
			s_Fulltarget_width += (X + Width - Renderer::GetFullTargetWidth()) * 2;
			sizeChanged = true;
		}
		if(Y + Height > Renderer::GetFullTargetHeight())
		{
			s_Fulltarget_height += (Y + Height - Renderer::GetFullTargetHeight()) * 2;
			sizeChanged = true;
		}
	}
	if (sizeChanged)
	{
		D3DCAPS9 caps = D3D::GetCaps();
		// Make sure that the requested size is actually supported by the GFX driver
		if (Renderer::GetFullTargetWidth() > caps.MaxTextureWidth || Renderer::GetFullTargetHeight() > caps.MaxTextureHeight)
		{
			// Skip EFB recreation and viewport setting. Most likely causes glitches in this case, but prevents crashes at least
			ERROR_LOG(VIDEO, "Tried to set a viewport which is too wide to emulate with Direct3D9. Requested EFB size is %dx%d, keeping the %dx%d EFB now\n", Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(), old_fulltarget_w, old_fulltarget_h);

			// Fix the viewport to fit to the old EFB size, TODO: Check this for off-by-one errors
			X *= old_fulltarget_w / Renderer::GetFullTargetWidth();
			Y *= old_fulltarget_h / Renderer::GetFullTargetHeight();
			Width *= old_fulltarget_w / Renderer::GetFullTargetWidth();
			Height *= old_fulltarget_h / Renderer::GetFullTargetHeight();

			s_Fulltarget_width = old_fulltarget_w;
			s_Fulltarget_height = old_fulltarget_h;
		}
		else
		{
			D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
			D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
			
			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager;

			D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
			D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		}
	}

	D3DVIEWPORT9 vp;
	vp.X = X;
	vp.Y = Y;
	vp.Width = Width;
	vp.Height = Height;
	
	// Some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
	vp.MinZ = 0.0f; // (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
	vp.MaxZ = 1.0f; // xfregs.rawViewport[5] / 16777216.0f;
	D3D::dev->SetViewport(&vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Reset rendering pipeline while keeping color masks and depth buffer settings
	ResetAPIState();
	SetDepthMode();
	SetColorMask();

	if (zEnable) // other depth functions don't make sense here
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

	// Update the viewport for clearing the target EFB rect
	TargetRectangle targetRc = ConvertEFBRectangle(rc);
	D3DVIEWPORT9 vp;
	vp.X = targetRc.left;
	vp.Y = targetRc.top;
	vp.Width  = targetRc.GetWidth();
	vp.Height = targetRc.GetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);
	D3D::drawClearQuad((color | (alphaEnable ? 0x0 : 0xFF000000)), (z & 0xFFFFFF) / float(0xFFFFFF), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());
	RestoreAPIState();
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	if (bpmem.blendmode.logicopenable && !forceUpdate)
		return;
	
	if (bpmem.blendmode.subtract && bpmem.blendmode.blendenable)
	{
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		D3D::SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dSrcFactors[1]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dDestFactors[1]);
	}
	else
	{
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, bpmem.blendmode.blendenable && (!( bpmem.blendmode.srcfactor == 1 && bpmem.blendmode.dstfactor == 0)));
		if (bpmem.blendmode.blendenable && (!( bpmem.blendmode.srcfactor == 1 && bpmem.blendmode.dstfactor == 0)))
		{
			D3D::SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			D3D::SetRenderState(D3DRS_SRCBLEND, d3dSrcFactors[bpmem.blendmode.srcfactor]);
			D3D::SetRenderState(D3DRS_DESTBLEND, d3dDestFactors[bpmem.blendmode.dstfactor]);
		}
	}
}

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle &dst_rect)
{
	HRESULT hr = D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
	if(FAILED(hr))
	{
		PanicAlert("Error dumping surface data.");
		return false;
	}
	hr = PD3DXSaveSurfaceToFileA(filename.c_str(), D3DXIFF_PNG, ScreenShootMEMSurface, NULL, dst_rect.AsRECT());
	if(FAILED(hr))
	{
		PanicAlert("Error saving screen.");
		return false;
	}

	return true;
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc)
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

	if(g_ActiveConfig.bAnaglyphStereo)
	{
		static bool RightFrame = false;
		if(RightFrame)
		{
			D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN);
			VertexShaderManager::ResetView();
			VertexShaderManager::TranslateView(-0.001f * g_ActiveConfig.iAnaglyphStereoSeparation,0.0f);
			VertexShaderManager::RotateView(-0.0001f *g_ActiveConfig.iAnaglyphFocalAngle,0.0f);
			RightFrame = false;
		}
		else
		{
			D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
			VertexShaderManager::ResetView();
			VertexShaderManager::TranslateView(0.001f *g_ActiveConfig.iAnaglyphStereoSeparation,0.0f);
			VertexShaderManager::RotateView(0.0001f * g_ActiveConfig.iAnaglyphFocalAngle,0.0f);
			RightFrame = true;
		}
	}

	// Prepare to copy the XFBs to our backbuffer
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	
	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);
	D3DVIEWPORT9 vp;

	// Clear full target screen (edges, borders etc)
	if(g_ActiveConfig.bAnaglyphStereo) {
		// use a clear quad to keep old red or blue/green data
		vp.X = 0;
		vp.Y = 0;
		vp.Width  = s_backbuffer_width;
		vp.Height = s_backbuffer_height;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		D3D::dev->SetViewport(&vp);
		D3D::drawClearQuad(0, 1.0, PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());
	}
	else
	{
		D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	}

	int X = dst_rect.left;
	int Y = dst_rect.top;
	int Width  = dst_rect.right - dst_rect.left;
	int Height = dst_rect.bottom - dst_rect.top;

	// Sanity check
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

	const XFBSourceBase* xfbSource = NULL;

	if(g_ActiveConfig.bUseXFB)
	{
		// draw each xfb source
		// Render to the real buffer now.
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = xfbSourceList[i];

			MathUtil::Rectangle<float> sourceRc;
			
			sourceRc.left = 0;
			sourceRc.top = 0;
			sourceRc.right = (float)xfbSource->texWidth;
			sourceRc.bottom = (float)xfbSource->texHeight;

			MathUtil::Rectangle<float> drawRc;

			if (!g_ActiveConfig.bUseRealXFB)
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
				//float vScale = (float)fbHeight / (float)dst_rect.GetHeight();
				//float hScale = (float)fbWidth / (float)dst_rect.GetWidth();
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

			xfbSource->Draw(sourceRc, drawRc, Width, Height);
		}
	}
	else
	{
		TargetRectangle targetRc = ConvertEFBRectangle(rc);
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBColorTexture();
		D3D::drawShadedTexQuad(read_texture,targetRc.AsRECT(),Renderer::GetFullTargetWidth(),Renderer::GetFullTargetHeight(),Width,Height,PixelShaderCache::GetColorCopyProgram(g_Config.iMultisampleMode),VertexShaderCache::GetSimpleVertexShader(g_Config.iMultisampleMode));
	}
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);

	if(g_ActiveConfig.bAnaglyphStereo)
	{
		DWORD color_mask = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
		D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
	}

	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	// Save screenshot
	if (s_bScreenshot)
	{
		s_criticalScreenshot.Enter();
		SaveScreenshot(s_sScreenshotName, dst_rect);
		s_bScreenshot = false;
		s_criticalScreenshot.Leave();
	}
	if (g_ActiveConfig.bDumpFrames)
	{
		HRESULT hr = D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
		if (!s_bLastFrameDumped)
		{
			s_recordWidth = dst_rect.GetWidth();
			s_recordHeight = dst_rect.GetHeight();
			s_bAVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), s_recordWidth, s_recordHeight);
			if (!s_bAVIDumping)
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
		if (s_bAVIDumping)
		{
			D3DLOCKED_RECT rect;
			if (SUCCEEDED(ScreenShootMEMSurface->LockRect(&rect, dst_rect.AsRECT(), D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)))
			{
				char* data = (char*)malloc(3 * s_recordWidth * s_recordHeight);
				formatBufferDump((const char*)rect.pBits, data, s_recordWidth, s_recordHeight, rect.Pitch);
				AVIDump::AddFrame(data);
				free(data);
				ScreenShootMEMSurface->UnlockRect();
			}
		}
		s_bLastFrameDumped = true;
	}
	else
	{
		if (s_bLastFrameDumped && s_bAVIDumping)
		{
			AVIDump::Stop();
			s_bAVIDumping = false;
			OSD::AddMessage("Stop dumping frames to AVI", 2000);
		}
		s_bLastFrameDumped = false;
	}

	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bShowFPS)
	{
		char fps[20];
		StringCchPrintfA(fps, 20, "FPS: %d\n", s_fps);
		D3D::font.DrawTextScaled(0, 30, 20, 20, 0.0f, 0xFF00FFFF, fps);
	}
	Renderer::DrawDebugText();

	if (g_ActiveConfig.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0, 30, 20, 20, 0.0f, 0xFF00FFFF, st);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		Statistics::ToStringProj(st);
		D3D::font.DrawTextScaled(0, 30, 20, 20, 0.0f, 0xFF00FFFF, st);
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

	u32 newAA = g_ActiveConfig.iMultisampleMode;

	if (xfbchanged || WindowResized || s_LastEFBScale != g_ActiveConfig.iEFBScale || s_LastAA != newAA)
	{
		s_LastAA = newAA;

		ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

		CalculateXYScale(dst_rect);
		
		int SupersampleCoeficient = s_LastAA + 1;

		s_LastEFBScale = g_ActiveConfig.iEFBScale;
		CalculateTargetSize(SupersampleCoeficient);

		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
		if (WindowResized)
		{
			SetupDeviceObjects();
		}
		else
		{
			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager;
		}
		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	}

	// Place messages on the picture, then copy it to the screen
	// ---------------------------------------------------------------------
	// Count FPS.
	// -------------
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
	D3D::BeginFrame();
	RestoreAPIState();

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	UpdateViewport();
	VertexShaderManager::SetViewportChanged();
	// For testing zbuffer targets.
	// Renderer::SetZBufferRender();
	// SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget,
	//	      GetTargetWidth(), GetTargetHeight());
	g_VideoInitialize.pCopiedToXFB(XFBWrited || (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
	XFBWrited = false;
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	D3D::SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	DWORD color_mask = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	UpdateViewport();
	SetScissorRect();
	if (bpmem.zmode.testenable)
		D3D::SetRenderState(D3DRS_ZENABLE, TRUE);
	if (bpmem.zmode.updateenable)
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
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
		// if the test is disabled write is disabled too
		D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);  // ??
	}
}

void Renderer::SetLogicOpMode()
{
	if (bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != 3)
	{
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, true);
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
	D3D::SetRenderState(D3DRS_DITHERENABLE, bpmem.blendmode.dither);
}

void Renderer::SetLineWidth()
{
	// We can't change line width in D3D unless we use ID3DXLine
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
		mip = (tm0.min_filter == 8) ? D3DTEXF_NONE : d3dMipFilters[tm0.min_filter & 3];
		if((tm0.min_filter & 3) && (tm0.min_filter != 8) && ((tm1.max_lod >> 4) == 0))
			mip = D3DTEXF_NONE;
	}
	if (texindex)
		stage += 4;
	
	if (mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR && g_ActiveConfig.iMaxAnisotropy > 1)
	{
		min = D3DTEXF_ANISOTROPIC;
	}
	D3D::SetSamplerState(stage, D3DSAMP_MINFILTER, min);
	D3D::SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
	D3D::SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
	
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dClamps[tm0.wrap_s]);
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dClamps[tm0.wrap_t]);
	//float SuperSampleCoeficient = (s_LastAA < 3)? s_LastAA + 1 : s_LastAA - 1;// uncoment this changes to conserve detail when incresing ssaa level
	float lodbias = (tm0.lod_bias / 32.0f);// + (s_LastAA)?(log(SuperSampleCoeficient) / log(2.0f)):0;
	D3D::SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&lodbias);
	D3D::SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, tm1.min_lod >> 4);
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

}
