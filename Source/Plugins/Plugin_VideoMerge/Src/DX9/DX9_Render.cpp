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
#include "DX9_VertexManager.h"
#include "DX9_Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "DX9_D3DUtil.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "DX9_VertexShaderCache.h"
#include "DX9_PixelShaderCache.h"
#include "VertexLoaderManager.h"
#include "DX9_TextureCache.h"
#include "EmuWindow.h"
#include "AVIDump.h"
#include "OnScreenDisplay.h"
#include "DX9_FramebufferManager.h"
#include "Fifo.h"
#include "DX9_TextureConverter.h"
#include "DLCache.h"

//#include "debugger/debugger.h"

#include "DX9_Render.h"

namespace DX9
{

bool Renderer::IS_AMD;

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

void Renderer::SetupDeviceObjects()
{
	D3D::font.Init();
	g_framebuffer_manager = new FramebufferManager;

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();
	// TODO:
	//TextureConverter::Init();
	
	// To avoid shader compilation stutters, read back all shaders from cache.
	// Texture cache will recreate themselves over time.
}

// Kill off all POOL_DEFAULT device objects.
void Renderer::TeardownDeviceObjects()
{
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	
	delete g_framebuffer_manager;

	//D3D::font.Shutdown();
	// TODO:
	//TextureConverter::Shutdown();
}

Renderer::Renderer() 
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

	D3D::Init();

	D3D::Create(g_ActiveConfig.iAdapter, EmuWindow::GetWnd(), 
				fullScreenRes, backbuffer_ms_mode, false);

	IS_AMD = D3D::IsATIDevice();
	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	s_XFB_width = MAX_XFB_WIDTH;
	s_XFB_height = MAX_XFB_HEIGHT;
	
	FramebufferSize(s_backbuffer_width, s_backbuffer_height);

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
	//return true;
}

Renderer::~Renderer()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
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
bool Renderer::CheckForResize()
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
			MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
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

		return true;
	}

	return false;
}

bool Renderer::SetScissorRect()
{
	EFBRectangle rc;
	if (g_renderer->SetScissorRect(rc))
	{
		D3D::dev->SetScissorRect((RECT*)&rc);
		return true;
	}
	else
	{
		//WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		const int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
		const int Ystride =  (s_Fulltarget_height - s_target_height) / 2;
		rc.left   = Xstride;
		rc.top    = Ystride;
		rc.right  = Xstride + s_target_width;
		rc.bottom = Ystride + s_target_height;

		D3D::dev->SetScissorRect((RECT*)&rc);
		return false;
	}
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

	if (type == POKE_Z || type == POKE_COLOR)
	{
		static bool alert_only_once = true;
		if (!alert_only_once) return 0;
		PanicAlert("Poke EFB not implemented");
		alert_only_once = false;
		return 0;
	}

	// Get the working buffer
	LPDIRECT3DSURFACE9 pBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FramebufferManager::GetEFBDepthRTSurface() : FramebufferManager::GetEFBColorRTSurface();
	// Get the temporal buffer to move 1pixel data
	LPDIRECT3DSURFACE9 RBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FramebufferManager::GetEFBDepthReadSurface() : FramebufferManager::GetEFBColorReadSurface();
	// Get the memory buffer that can be locked
	LPDIRECT3DSURFACE9 pOffScreenBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FramebufferManager::GetEFBDepthOffScreenRTSurface() : FramebufferManager::GetEFBColorOffScreenRTSurface();
	// Get the buffer format
	D3DFORMAT BufferFormat = (type == PEEK_Z || type == POKE_Z) ? 
		FramebufferManager::GetEFBDepthRTSurfaceFormat() : FramebufferManager::GetEFBColorRTSurfaceFormat();
	D3DFORMAT ReadBufferFormat = (type == PEEK_Z || type == POKE_Z) ? 
		FramebufferManager::GetEFBDepthReadSurfaceFormat() : BufferFormat;
	
	if (BufferFormat == D3DFMT_D24X8)
		return 0;

	D3DLOCKED_RECT drect;
	
	// Buffer not found alert
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
		ResetAPIState(); // Reset any game specific settings
		hr = D3D::dev->SetDepthStencilSurface(NULL);
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
		float colmat[16] = {0.0f};
		float fConstAdd[4] = {0.0f};
		colmat[0] = colmat[5] = colmat[10] = 1.0f;
		PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
		EFBRectangle source_rect;
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBDepthTexture(source_rect);
		
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

		D3D::drawShadedTexQuad(
			read_texture, 
			&RectToLock, 
			Renderer::GetFullTargetWidth(), 
			Renderer::GetFullTargetHeight(), 
			4, 4, 
			(BufferFormat == FOURCC_RAWZ) ? PixelShaderCache::GetColorMatrixProgram(0) : PixelShaderCache::GetDepthMatrixProgram(0), 
			VertexShaderCache::GetSimpleVertexShader(0));

		D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);

		hr = D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		hr = D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		RestoreAPIState();
		RectToLock.bottom = 4;
		RectToLock.left = 0;
		RectToLock.right = 4;
		RectToLock.top = 0;
	}
	else
	{
		hr = D3D::dev->StretchRect(pBuffer, &RectToLock, RBuffer, NULL, D3DTEXF_NONE);
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
	// Retrieve the pixel data to the local memory buffer
	D3D::dev->GetRenderTargetData(RBuffer, pOffScreenBuffer);
	if (FAILED(hr))
	{
		PanicAlert("Unable to copy data to mem buffer");
		return 0;
	}
	
	
	
	// The surface is good.. lock it
	if ((hr = pOffScreenBuffer->LockRect(&drect, &RectToLock, D3DLOCK_READONLY)) != D3D_OK)
	{
		PanicAlert("ERROR: %s", hr == D3DERR_WASSTILLDRAWING ? "Still drawing" : hr == D3DERR_INVALIDCALL ? "Invalid call" : "w00t");
		return 0;
	}
		
	switch (type) {
		case PEEK_Z:
			{
				switch (ReadBufferFormat)
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
			}
			break;

		case PEEK_COLOR:
			z = ((u32 *)drect.pBits)[0];
			break;
		case POKE_COLOR:

		// TODO: Implement POKE_Z and POKE_COLOR
		default:
			break;
	}

	pOffScreenBuffer->UnlockRect();
	// TODO: in RE0 this value is often off by one, which causes lighting to disappear
	return z;
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
	const int old_fulltarget_w = s_Fulltarget_width;
	const int old_fulltarget_h = s_Fulltarget_height;

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
		if(X + Width > s_Fulltarget_width)
		{
			s_Fulltarget_width += (X + Width - s_Fulltarget_width) * 2;
			sizeChanged = true;
		}
		if(Y + Height > s_Fulltarget_height)
		{
			s_Fulltarget_height += (Y + Height - s_Fulltarget_height) * 2;
			sizeChanged = true;
		}
	}
	if (sizeChanged)
	{
		D3DCAPS9 caps = D3D::GetCaps();
		// Make sure that the requested size is actually supported by the GFX driver
		if (s_Fulltarget_width > caps.MaxTextureWidth || s_Fulltarget_height > caps.MaxTextureHeight)
		{
			// Skip EFB recreation and viewport setting. Most likely causes glitches in this case, but prevents crashes at least
			ERROR_LOG(VIDEO, "Tried to set a viewport which is too wide to emulate with Direct3D9. Requested EFB size is %dx%d, keeping the %dx%d EFB now\n", s_Fulltarget_width, s_Fulltarget_height, old_fulltarget_w, old_fulltarget_h);

			// Fix the viewport to fit to the old EFB size, TODO: Check this for off-by-one errors
			X *= old_fulltarget_w / s_Fulltarget_width;
			Y *= old_fulltarget_h / s_Fulltarget_height;
			Width *= old_fulltarget_w / s_Fulltarget_width;
			Height *= old_fulltarget_h / s_Fulltarget_height;

			s_Fulltarget_width = old_fulltarget_w;
			s_Fulltarget_height = old_fulltarget_h;
		}
		else
		{
			D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
			D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());

			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager();

			D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
			D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		}
	}
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
	// Update the view port for clearing the picture
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
	D3DVIEWPORT9 vp;
	vp.X = targetRc.left;
	vp.Y = targetRc.top;
	vp.Width  = targetRc.GetWidth();
	vp.Height = targetRc.GetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);

	// Always set the scissor in case it was set by the game and has not been reset
	RECT sicr;
	sicr.left   = targetRc.left;
	sicr.top    = targetRc.top;
	sicr.right  = targetRc.right;
	sicr.bottom = targetRc.bottom;
	D3D::dev->SetScissorRect(&sicr);
	D3D::ChangeRenderState(D3DRS_ALPHABLENDENABLE, false);
	if (zEnable)
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	D3D::drawClearQuad(color, (z & 0xFFFFFF) / float(0xFFFFFF), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());
	if (zEnable)
		D3D::RefreshRenderState(D3DRS_ZFUNC);
	D3D::RefreshRenderState(D3DRS_ALPHABLENDENABLE);
	UpdateViewport();
	SetScissorRect();
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	if (bpmem.blendmode.logicopenable)
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

void Renderer::PrepareXFBCopy(const TargetRectangle &dst_rect)
{
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

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
}

void Renderer::Draw(const XFBSourceBase* xfbSource, const TargetRectangle& sourceRc,
	const MathUtil::Rectangle<float>& drawRc, const EFBRectangle& rc)
{
	// TODO: this is lame here
	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	const int Width  = dst_rect.right - dst_rect.left;
	const int Height = dst_rect.bottom - dst_rect.top;

	if (xfbSource)
	{
		const LPDIRECT3DTEXTURE9 tex = ((XFBSource*)xfbSource)->texture;

		// TODO:

		//D3D::drawShadedTexSubQuad(tex, &sourceRc, xfbSource->texWidth,
		//	xfbSource->texHeight, &drawRc, Width, Height, PixelShaderCache::GetColorCopyProgram(0),
		//	VertexShaderCache::GetSimpleVertexShader(0));
	}
	else
	{
		const LPDIRECT3DTEXTURE9 tex = FramebufferManager::GetEFBColorTexture(rc);

		D3D::drawShadedTexQuad(tex, sourceRc.AsRECT(), Renderer::GetFullTargetWidth(),
			Renderer::GetFullTargetHeight(), Width, Height,
			PixelShaderCache::GetColorCopyProgram(g_Config.iMultisampleMode),
			VertexShaderCache::GetSimpleVertexShader(g_Config.iMultisampleMode));		
	}

	// TODO: good here?
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);

	// TODO: this is for overlay text i think, move it elsewhere
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
}

void Renderer::EndFrame()
{
	D3D::EndFrame();
}

void Renderer::Present()
{
	D3D::Present();
}

void Renderer::GetBackBufferSize(int* w, int* h)
{
	*w = D3D::GetBackBufferWidth();
	*h = D3D::GetBackBufferHeight();
}

void Renderer::RecreateFramebufferManger()
{
	// TODO: these ok here?
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	SetupDeviceObjects();

	delete g_framebuffer_manager;
	g_framebuffer_manager = new FramebufferManager;

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

void Renderer::BeginFrame()
{
	D3D::BeginFrame();
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

}
