// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <list>
#include <d3dx9.h>

#include "StringUtil.h"
#include "Common.h"
#include "Atomic.h"
#include "FileUtil.h"
#include "Thread.h"
#include "Timer.h"
#include "Statistics.h"
#include "Host.h"

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
#include "Core.h"
#include "Movie.h"
#include "BPFunctions.h"
#include "FPSCounter.h"
#include "ConfigManager.h"
#include "PerfQuery.h"
#include <strsafe.h>


namespace DX9
{

static int s_fps = 0;

static u32 s_blendMode;
static u32 s_LastAA;
static bool IS_AMD;
static float m_fMaxPointSize;
static bool s_vsync;
static char *st;

static LPDIRECT3DSURFACE9 ScreenShootMEMSurface = NULL;


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
	g_vertex_manager->CreateDeviceObjects();
	((PerfQuery*)g_perf_query)->CreateDeviceObjects();
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
	((PerfQuery*)g_perf_query)->DestroyDeviceObjects();
	D3D::font.Shutdown();
	TextureCache::Invalidate();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	PixelShaderCache::Shutdown();
	TextureConverter::Shutdown();
	g_vertex_manager->DestroyDeviceObjects();
}

// Init functions
Renderer::Renderer()
{
	InitFPSCounter();

	st = new char[32768];

	int fullScreenRes, x, y, w_temp, h_temp;
	s_blendMode = 0;
	// Multisample Anti-aliasing hasn't been implemented yet use supersamling instead
	int backbuffer_ms_mode = 0;

	Host_GetRenderWindowSize(x, y, w_temp, h_temp);

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

	// Decide framebuffer size
	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_LastAA = g_ActiveConfig.iMultisampleMode;
	int SupersampleCoeficient = (s_LastAA % 3) + 1;

	s_LastEFBScale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height, SupersampleCoeficient);

	// Make sure to use valid texture sizes
	D3D::FixTextureSize(s_target_width, s_target_height);

	// We're not using fixed function.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);

	SetupDeviceObjects();

	for (int stage = 0; stage < 8; stage++)
		D3D::SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, 1 << g_ActiveConfig.iMaxAnisotropy);

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
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_target_width;
	vp.Height = s_target_height;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
	D3D::BeginFrame();
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	D3D::dev->CreateOffscreenPlainSurface(s_backbuffer_width,s_backbuffer_height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );
	D3D::SetRenderState(D3DRS_POINTSCALEENABLE,false);
	m_fMaxPointSize = D3D::GetCaps().MaxPointSize;
	// Handle VSync on/off 
	s_vsync = g_ActiveConfig.IsVSync();
}

Renderer::~Renderer()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
	
	delete[] st;
}

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text);
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

}

void formatBufferDump(const u8* in, u8* out, int w, int h, int p)
{
	for (int y = 0; y < h; y++)
	{
		auto line = in + (h - y - 1) * p;
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

void Renderer::SetScissorRect(const TargetRectangle& rc)
{
	D3D::dev->SetScissorRect(rc.AsRECT());
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	DWORD color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24))
			color_mask = D3DCOLORWRITEENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	}
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

	// if depth textures aren't supported by the hardware, just return
	if (type == PEEK_Z)
		if (FramebufferManager::GetEFBDepthTexture() == NULL)
			return 0;

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
		// TODO: why is D3DFMT_D24X8 singled out here? why not D3DFMT_D24X4S4/D24S8/D24FS8/D32/D16/D15S1 too, or none of them?
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

		float colmat[28] = {0.0f};
		colmat[0] = colmat[5] = colmat[10] = 1.0f;
		PixelShaderManager::SetColorMatrix(colmat); // set transformation
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBDepthTexture();
		
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

		D3DFORMAT bformat = FramebufferManager::GetEFBDepthRTSurfaceFormat();

		D3D::drawShadedTexQuad(
			read_texture,
			&RectToLock,
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			4, 4,
			PixelShaderCache::GetDepthMatrixProgram(0, bformat != FOURCC_RAWZ),
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
		u32 z = ((u32*)drect.pBits)[6];	// 24 bit depth value
		pSystemBuf->UnlockRect();

		// if Z is in 16 bit format you must return a 16 bit integer
		if(bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16) {
			z >>= 8;
		}

		return z;
	}
	else if(type == PEEK_COLOR)
	{
		// We can't directly StretchRect to System buf because is not supported by all implementations
		// this is the only safe path that works in most cases
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
		PixelEngine::Read16((u16&)alpha_read_mode, PE_ALPHAREAD);

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
		else return (ret & 0x00FFFFFF); // GX_READ_00
	}
	else //if(type == POKE_COLOR)
	{
		// TODO: Speed this up by batching pokes?
		ResetAPIState();
		D3D::drawColorQuad(poke_data,
							  (float)RectToLock.left   * 2.f / (float)Renderer::GetTargetWidth()  - 1.f,
							- (float)RectToLock.top    * 2.f / (float)Renderer::GetTargetHeight() + 1.f,
							  (float)RectToLock.right  * 2.f / (float)Renderer::GetTargetWidth()  - 1.f,
							- (float)RectToLock.bottom * 2.f / (float)Renderer::GetTargetHeight() + 1.f);
		RestoreAPIState();
		return 0;
	}
}

// Viewport correction:
// Say you want a viewport at (ix, iy) with size (iw, ih),
// but your viewport must be clamped at (ax, ay) with size (aw, ah).
// Just multiply the projection matrix with the following to get the same
// effect:
// [   (iw/aw)         0     0    ((iw - 2*(ax-ix)) / aw - 1)   ]
// [         0   (ih/ah)     0   ((-ih + 2*(ay-iy)) / ah + 1)   ]
// [         0         0     1                              0   ]
// [         0         0     0                              1   ]
static void ViewportCorrectionMatrix(Matrix44& result,
	float ix, float iy, float iw, float ih, // Intended viewport (x, y, width, height)
	float ax, float ay, float aw, float ah) // Actual viewport (x, y, width, height)
{
	Matrix44::LoadIdentity(result);
	if (aw == 0.f || ah == 0.f)
		return;
	result.data[4*0+0] = iw / aw;
	result.data[4*0+3] = (iw - 2.f * (ax - ix)) / aw - 1.f;
	result.data[4*1+1] = ih / ah;
	result.data[4*1+3] = (-ih + 2.f * (ay - iy)) / ah + 1.f;
}

// Called from VertexShaderManager
void Renderer::UpdateViewport(Matrix44& vpCorrection)
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

	// TODO: ceil, floor or just cast to int?
	int intendedX = EFBToScaledX((int)ceil(xfregs.viewport.xOrig - xfregs.viewport.wd - scissorXOff));
	int intendedY = EFBToScaledY((int)ceil(xfregs.viewport.yOrig + xfregs.viewport.ht - scissorYOff));
	int intendedWd = EFBToScaledX((int)ceil(2.0f * xfregs.viewport.wd));
	int intendedHt = EFBToScaledY((int)ceil(-2.0f * xfregs.viewport.ht));
	if (intendedWd < 0)
	{
		intendedX += intendedWd;
		intendedWd = -intendedWd;
	}
	if (intendedHt < 0)
	{
		intendedY += intendedHt;
		intendedHt = -intendedHt;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	int X = intendedX;
	if (X < 0)
		X = 0;
	int Y = intendedY;
	if (Y < 0)
		Y = 0;
	int Wd = intendedWd;
	if (X + Wd > GetTargetWidth())
		Wd = GetTargetWidth() - X;
	int Ht = intendedHt;
	if (Y + Ht > GetTargetHeight())
		Ht = GetTargetHeight() - Y;

	// If GX viewport is off the render target, we must clamp our viewport
	// within the bounds. Use the correction matrix to compensate.
	ViewportCorrectionMatrix(vpCorrection,
		(float)intendedX, (float)intendedY, (float)intendedWd, (float)intendedHt,
		(float)X, (float)Y, (float)Wd, (float)Ht);

	D3DVIEWPORT9 vp;
	vp.X = X;
	vp.Y = Y;
	vp.Width = Wd;
	vp.Height = Ht;

	// Some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
	vp.MinZ = 0.0f; // (xfregs.viewport.farZ - xfregs.viewport.zRange) / 16777216.0f;
	vp.MaxZ = 1.0f; // xfregs.viewport.farZ / 16777216.0f;
	D3D::dev->SetViewport(&vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Reset rendering pipeline while keeping color masks and depth buffer settings
	ResetAPIState();

	DWORD color_mask = 0;
	if (alphaEnable)
		color_mask = D3DCOLORWRITEENABLE_ALPHA;
	if (colorEnable)
		color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, color_mask);

	if (zEnable)
	{
		D3D::ChangeRenderState(D3DRS_ZENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	}
	else
	{
		D3D::ChangeRenderState(D3DRS_ZENABLE, FALSE);
	}

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
	D3D::drawClearQuad(color, (z & 0xFFFFFF) / float(0xFFFFFF), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());
	RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	RECT source;
	SetRect(&source, 0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	LPDIRECT3DPIXELSHADER9 pixel_shader;
	if (convtype == 0) pixel_shader = PixelShaderCache::ReinterpRGB8ToRGBA6();
	else if (convtype == 2) pixel_shader = PixelShaderCache::ReinterpRGBA6ToRGB8();
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	// convert data and set the target texture as our new EFB
	g_renderer->ResetAPIState();
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorReinterpretSurface());
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = g_renderer->GetTargetWidth();
	vp.Height = g_renderer->GetTargetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);
	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture(), &source,
		g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		pixel_shader, VertexShaderCache::GetSimpleVertexShader(0));
	FramebufferManager::SwapReinterpretTexture();
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);	
	g_renderer->RestoreAPIState();
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	//bDstAlphaPass is taken into account because the ability to disable alpha composition is
	//really useful for debugging shader and blending errors
	bool use_DstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha;
	bool use_DualSource = use_DstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	const D3DBLEND d3dSrcFactors[8] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_INVDESTCOLOR,
		(use_DualSource) ? D3DBLEND_SRCCOLOR2 : D3DBLEND_SRCALPHA,
		(use_DualSource) ? D3DBLEND_INVSRCCOLOR2 : D3DBLEND_INVSRCALPHA,
		(target_has_alpha) ? D3DBLEND_DESTALPHA : D3DBLEND_ONE,
		(target_has_alpha) ? D3DBLEND_INVDESTALPHA : D3DBLEND_ZERO
	};
	const D3DBLEND d3dDestFactors[8] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_SRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		(use_DualSource) ? D3DBLEND_SRCCOLOR2 : D3DBLEND_SRCALPHA,
		(use_DualSource) ? D3DBLEND_INVSRCCOLOR2 : D3DBLEND_INVSRCALPHA,
		(target_has_alpha) ? D3DBLEND_DESTALPHA : D3DBLEND_ONE,
		(target_has_alpha) ? D3DBLEND_INVDESTALPHA : D3DBLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !forceUpdate)
	{
		D3D::SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE , false);
		return;
	}

	bool blend_enable = bpmem.blendmode.subtract || bpmem.blendmode.blendenable;
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, blend_enable);
	D3D::SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, blend_enable && g_ActiveConfig.backend_info.bSupportsSeparateAlphaFunction);
	if (blend_enable)
	{
		D3DBLENDOP op = D3DBLENDOP_ADD;
		u32 srcidx = bpmem.blendmode.srcfactor;
		u32 dstidx = bpmem.blendmode.dstfactor;
		if (bpmem.blendmode.subtract)
		{
			op = D3DBLENDOP_REVSUBTRACT;
			srcidx = GX_BL_ONE;
			dstidx = GX_BL_ONE;
		}
		D3D::SetRenderState(D3DRS_BLENDOP, op);
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dSrcFactors[srcidx]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dDestFactors[dstidx]);
		if (g_ActiveConfig.backend_info.bSupportsSeparateAlphaFunction)
		{
			if (use_DualSource)
			{			
				op = D3DBLENDOP_ADD;
				srcidx = GX_BL_ONE;
				dstidx = GX_BL_ZERO;
			}
			else
			{
				// we can't use D3DBLEND_DESTCOLOR or D3DBLEND_INVDESTCOLOR for source in alpha channel so use their alpha equivalent instead
				if (srcidx == GX_BL_DSTCLR) srcidx = GX_BL_DSTALPHA;
				if (srcidx == GX_BL_INVDSTCLR) srcidx = GX_BL_INVDSTALPHA;
				// we can't use D3DBLEND_SRCCOLOR or D3DBLEND_INVSRCCOLOR for destination in alpha channel so use their alpha equivalent instead
				if (dstidx == GX_BL_SRCCLR) dstidx = GX_BL_SRCALPHA;
				if (dstidx == GX_BL_INVSRCCLR) dstidx = GX_BL_INVSRCALPHA;
			}
			D3D::SetRenderState(D3DRS_BLENDOPALPHA, op);
			D3D::SetRenderState(D3DRS_SRCBLENDALPHA, d3dSrcFactors[srcidx]);
			D3D::SetRenderState(D3DRS_DESTBLENDALPHA, d3dDestFactors[dstidx]);
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
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc,float Gamma)
{
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		if (g_ActiveConfig.bDumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	if (field == FIELD_LOWER) xfbAddr -= fbWidth * 2;
	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		if (g_ActiveConfig.bDumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
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
	
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
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

	int X = GetTargetRectangle().left;
	int Y = GetTargetRectangle().top;
	int Width  = GetTargetRectangle().right - GetTargetRectangle().left;
	int Height = GetTargetRectangle().bottom - GetTargetRectangle().top;

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

			if (g_ActiveConfig.bUseRealXFB)
			{
				drawRc.top = -1;
				drawRc.bottom = 1;
				drawRc.left = -1;
				drawRc.right = 1;
			}
			else
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
				//float vScale = (float)fbHeight / (float)GetTargetRectangle().GetHeight();
				//float hScale = (float)fbWidth / (float)GetTargetRectangle().GetWidth();
				//drawRc.top *= vScale;
				//drawRc.bottom *= vScale;
				//drawRc.left *= hScale;
				//drawRc.right *= hScale;
			}

			xfbSource->Draw(sourceRc, drawRc, Width, Height);
		}
	}
	else
	{
		TargetRectangle targetRc = ConvertEFBRectangle(rc);
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBColorTexture();
		D3D::drawShadedTexQuad(read_texture,targetRc.AsRECT(),
			Renderer::GetTargetWidth(),Renderer::GetTargetHeight(),
			Width,Height,
			PixelShaderCache::GetColorCopyProgram(g_ActiveConfig.iMultisampleMode),
			VertexShaderCache::GetSimpleVertexShader(g_ActiveConfig.iMultisampleMode),Gamma);		
		
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
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		SaveScreenshot(s_sScreenshotName, GetTargetRectangle());
		s_bScreenshot = false;
	}

	// Dump frames
	static int w = 0, h = 0;
	if (g_ActiveConfig.bDumpFrames)
	{
		static int s_recordWidth;
		static int s_recordHeight;

		HRESULT hr = D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
		if (!bLastFrameDumped)
		{
			s_recordWidth = GetTargetRectangle().GetWidth();
			s_recordHeight = GetTargetRectangle().GetHeight();
			bAVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), s_recordWidth, s_recordHeight);
			if (!bAVIDumping)
			{
				PanicAlert("Error dumping frames to AVI.");
			}
			else
			{
				char msg [255];
				sprintf_s(msg,255, "Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
						File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_recordWidth, s_recordHeight);
				OSD::AddMessage(msg, 2000);
			}
		}
		if (bAVIDumping)
		{
			D3DLOCKED_RECT rect;
			if (SUCCEEDED(ScreenShootMEMSurface->LockRect(&rect, GetTargetRectangle().AsRECT(), D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)))
			{
				if (frame_data.empty() || w != s_recordWidth || h != s_recordHeight)
				{
					frame_data.resize(3 * s_recordWidth * s_recordHeight);
					w = s_recordWidth;
					h = s_recordHeight;
				}
				formatBufferDump((const u8*)rect.pBits, &frame_data[0], s_recordWidth, s_recordHeight, rect.Pitch);
				AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);
				ScreenShootMEMSurface->UnlockRect();
			}
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

	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bShowFPS)
	{
		char fps[20];
		StringCchPrintfA(fps, 20, "FPS: %d\n", s_fps);
		D3D::font.DrawTextScaled(0, 0, 20, 20, 0.0f, 0xFF00FFFF, fps);
	}

	if (SConfig::GetInstance().m_ShowLag)
	{
		char lag[10];
		StringCchPrintfA(lag, 10, "Lag: %llu\n", Movie::g_currentLagCount);
		D3D::font.DrawTextScaled(0, 18, 20, 20, 0.0f, 0xFF00FFFF, lag);
	}

	if (g_ActiveConfig.bShowInputDisplay)
	{
		char inputDisplay[1000];
		StringCchPrintfA(inputDisplay, 1000, Movie::GetInputDisplay().c_str());
		D3D::font.DrawTextScaled(0, 36, 20, 20, 0.0f, 0xFF00FFFF, inputDisplay);
	}

	Renderer::DrawDebugText();

	if (g_ActiveConfig.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0, 36, 20, 20, 0.0f, 0xFF00FFFF, st);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		Statistics::ToStringProj(st);
		D3D::font.DrawTextScaled(0, 36, 20, 20, 0.0f, 0xFF00FFFF, st);
	}

	OSD::DrawMessages();
	D3D::EndFrame();
	++frameCount;

	GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);

	DLCache::ProgressiveCleanup();
	TextureCache::Cleanup();
	// Flip/present backbuffer to frontbuffer here
	D3D::Present();
	// Enable configuration changes
	UpdateActiveConfig();
	TextureCache::OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fbWidth, fbHeight);

	const bool windowResized = CheckForResize();

	bool xfbchanged = false;

	if (FramebufferManagerBase::LastXfbWidth() != fbWidth || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int w = (fbWidth < 1 || fbWidth > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbWidth;
		unsigned int h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(w);
		FramebufferManagerBase::SetLastXfbHeight(h);
	}

	u32 newAA = g_ActiveConfig.iMultisampleMode;

	if (xfbchanged || windowResized || s_LastEFBScale != g_ActiveConfig.iEFBScale || s_LastAA != newAA)
	{
		s_LastAA = newAA;

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
		
		int SupersampleCoeficient = (s_LastAA % 3) + 1;

		s_LastEFBScale = g_ActiveConfig.iEFBScale;
		CalculateTargetSize(s_backbuffer_width, s_backbuffer_height, SupersampleCoeficient);

		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
		if (windowResized)
		{
			// device objects lost, so recreate all of them
			SetupDeviceObjects();
		}
		else
		{
			// just resize the frame buffer
			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager;
		}
		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
	}

	if (XFBWrited)
		s_fps = UpdateFPSCounter();

	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	// New frame
	stats.ResetFrame();

	// Handle vsync changes during execution
	if(s_vsync != g_ActiveConfig.IsVSync())
	{
		s_vsync = g_ActiveConfig.IsVSync();
		TeardownDeviceObjects();
		D3D::Reset();
		// device objects lost, so recreate all of them
		SetupDeviceObjects();
	}
	D3D::BeginFrame();
	RestoreAPIState();

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	VertexShaderManager::SetViewportChanged();

	Core::Callback_VideoCopiedToXFB(XFBWrited || (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
	XFBWrited = false;
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
	if (bUseDstAlpha)
	{
		// If we get here we are sure that we are using dst alpha pass. (bpmem.dstalpha.enable)
		// Alpha write is enabled. (because bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		// We must disable blend because we want to write alpha value directly to the alpha channel without modifications.
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
		D3D::ChangeRenderState(D3DRS_ALPHABLENDENABLE, false);
		if(bpmem.zmode.testenable && bpmem.zmode.updateenable)
		{
			// This is needed to draw to the correct pixels in multi-pass algorithms
			// to avoid z-fighting and grants that you write to the same pixels
			// affected by the last pass
			D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, false);
			D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
		}
	}
}

void Renderer::RestoreState()
{
	D3D::RefreshRenderState(D3DRS_COLORWRITEENABLE);
	D3D::RefreshRenderState(D3DRS_ALPHABLENDENABLE);
	if(bpmem.zmode.testenable && bpmem.zmode.updateenable)
	{
		D3D::RefreshRenderState(D3DRS_ZWRITEENABLE);
		D3D::RefreshRenderState(D3DRS_ZFUNC);
	}
	// TODO: Enable this code. Caused glitches for me however (neobrain)
//	for (unsigned int i = 0; i < 8; ++i)
//		D3D::dev->SetTexture(i, NULL);
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
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
	D3D::SetRenderState(D3DRS_FILLMODE, g_ActiveConfig.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	VertexShaderManager::SetViewportChanged();
	BPFunctions::SetScissor();
	if (bpmem.zmode.testenable) {
		D3D::SetRenderState(D3DRS_ZENABLE, TRUE);
		if (bpmem.zmode.updateenable)
			D3D::SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	}
	SetColorMask();
	SetLogicOpMode();
	SetGenerationMode();
}

void Renderer::SetGenerationMode()
{
	const D3DCULL d3dCullModes[4] =
	{
		D3DCULL_NONE,
		D3DCULL_CCW,
		D3DCULL_CW,
		D3DCULL_CCW
	};

	D3D::SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);
}

void Renderer::SetDepthMode()
{
	const D3DCMPFUNC d3dCmpFuncs[8] =
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
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	}
}

void Renderer::SetLogicOpMode()
{
	// D3D9 doesn't support logic blending, so this is a huge hack

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
	const D3DBLENDOP d3dLogicOpop[16] =
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
	const D3DBLEND d3dLogicOpSrcFactors[16] =
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
	const D3DBLEND d3dLogicOpDestFactors[16] =
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

	if (bpmem.blendmode.logicopenable)
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
	float fratio = xfregs.viewport.wd != 0 ? Renderer::EFBToScaledXf(1.f) : 1.0f;
	float psize = bpmem.lineptwidth.linesize * fratio / 6.0f;
	//little hack to compensate scaling problems in dx9 must be taken out when scaling is fixed.
	psize *= 2.0f;
	if (psize > m_fMaxPointSize)
	{
		psize = m_fMaxPointSize;
	}
	D3D::SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&psize));
	D3D::SetRenderState(D3DRS_POINTSIZE_MIN, *((DWORD*)&psize));
	D3D::SetRenderState(D3DRS_POINTSIZE_MAX, *((DWORD*)&psize));
}

void Renderer::SetSamplerState(int stage, int texindex)
{
	const D3DTEXTUREFILTERTYPE d3dMipFilters[4] =
	{
		D3DTEXF_NONE,
		D3DTEXF_POINT,
		D3DTEXF_LINEAR,
		D3DTEXF_NONE, //reserved
	};
	const D3DTEXTUREADDRESS d3dClamps[4] =
	{
		D3DTADDRESS_CLAMP,
		D3DTADDRESS_WRAP,
		D3DTADDRESS_MIRROR,
		D3DTADDRESS_WRAP //reserved
	};

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
		mip = d3dMipFilters[tm0.min_filter & 3];
	}
	if (texindex)
		stage += 4;

	if (mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR && g_ActiveConfig.iMaxAnisotropy)
	{
		min = D3DTEXF_ANISOTROPIC;
	}
	D3D::SetSamplerState(stage, D3DSAMP_MINFILTER, min);
	D3D::SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
	D3D::SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
	
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dClamps[tm0.wrap_s]);
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dClamps[tm0.wrap_t]);

	float lodbias = (s32)tm0.lod_bias / 32.0f;
	D3D::SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&lodbias);
	D3D::SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, tm1.min_lod >> 4);
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

}  // namespace DX9
