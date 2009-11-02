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

#include "Common.h"
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

#include "debugger/debugger.h"

static int s_target_width;
static int s_target_height;

static int s_backbuffer_width;
static int s_backbuffer_height;

static float xScale;
static float yScale;

static int s_recordWidth;
static int s_recordHeight;

static bool s_LastFrameDumped;
static bool s_AVIDumping;

static u32 s_blendMode;

char st[32768];

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

void SetupDeviceObjects()
{
	D3D::font.Init();
	VertexLoaderManager::Init();
	FBManager::Create();

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();

	// Tex and shader caches will recreate themselves over time.
}

// Kill off all POOL_DEFAULT device objects.
void TeardownDeviceObjects()
{
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	FBManager::Destroy();
	D3D::font.Shutdown();
	TextureCache::Invalidate(false);
	VertexManager::DestroyDeviceObjects();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Clear();
	PixelShaderCache::Clear();
}

bool Renderer::Init() 
{
	UpdateActiveConfig();
	int fullScreenRes, w_temp, h_temp;
	s_blendMode = 0;
	int backbuffer_ms_mode = 0;  // g_ActiveConfig.iMultisampleMode;

	sscanf(g_Config.cFSResolution, "%dx%d", &w_temp, &h_temp);

	for (fullScreenRes = 0; fullScreenRes < D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size(); fullScreenRes++)
	{
		if ((D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].xres == w_temp) && 
			(D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].yres == h_temp))
			break;
	}
	if (fullScreenRes == D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size())
		fullScreenRes = 0;

	D3D::Create(g_ActiveConfig.iAdapter, EmuWindow::GetWnd(), g_ActiveConfig.bFullscreen,
				fullScreenRes, backbuffer_ms_mode, false);

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	// TODO: Grab target width from configured resolution?
	s_target_width  = s_backbuffer_width * EFB_WIDTH / 640;
	s_target_height = s_backbuffer_height * EFB_HEIGHT / 480;

	xScale = (float)s_target_width / (float)EFB_WIDTH;
	yScale = (float)s_target_height / (float)EFB_HEIGHT;

	s_LastFrameDumped = false;
	s_AVIDumping = false;

	// We're not using fixed function, except for some 2D.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);

	SetupDeviceObjects();

	for (int stage = 0; stage < 8; stage++)
		D3D::SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, g_ActiveConfig.iMaxAnisotropy);

	D3D::dev->Clear(0, NULL, D3DCLEAR_ZBUFFER,D3DCOLOR_XRGB(255,255,255),1.0f,0);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 0, 0);

	D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());

	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x0, 1.0f, 0);

	D3D::BeginFrame();
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);
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
float Renderer::GetTargetScaleX() { return xScale; }
float Renderer::GetTargetScaleY() { return yScale; }

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text, false);
}

void dumpMatrix(D3DXMATRIX &mtx)
{
	for (int y = 0; y < 4; y++)
	{
		char temp[256];
		sprintf(temp,"%4.4f %4.4f %4.4f %4.4f",mtx.m[y][0],mtx.m[y][1],mtx.m[y][2],mtx.m[y][3]);
		g_VideoInitialize.pLog(temp, FALSE);
	}
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = (rc.left   * s_target_width)  / EFB_WIDTH;
	result.top    = (rc.top    * s_target_height) / EFB_HEIGHT;
	result.right  = (rc.right  * s_target_width)  / EFB_WIDTH;
	result.bottom = (rc.bottom * s_target_height) / EFB_HEIGHT;
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

		SetupDeviceObjects();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();
	}
}

static void EFBTextureToD3DBackBuffer(const EFBRectangle& sourceRc)
{
	// Set the backbuffer as the rendering target
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(NULL);

	// Blit our render target onto the backbuffer.
	// TODO: Change to a quad so we can do post processing.
	TargetRectangle src_rect, dst_rect;
	src_rect = Renderer::ConvertEFBRectangle(sourceRc);
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	//LPD3DXSPRITE pSprite=NULL;
	//D3DXCreateSprite(D3D::dev, &pSprite);
	//D3DXVECTOR3 pos(0,0,0);
	//EFBRectangle efbRect;
	//
	//pSprite->Begin(D3DXSPRITE_ALPHABLEND);
	//pSprite->Draw(FBManager::GetEFBColorTexture(efbRect),NULL, NULL, &pos, 0xFFFFFFFF);
	//pSprite->End();
	//pSprite->Release();

	D3D::dev->Clear(0,NULL, D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0f,0);

	// todo, to draw the EFB texture to the backbuffer instead of StretchRect
	D3D::dev->StretchRect(FBManager::GetEFBColorRTSurface(), src_rect.AsRECT(),
		D3D::GetBackBufferSurface(), dst_rect.AsRECT(),
		D3DTEXF_LINEAR);

	// Finish up the current frame, print some stats
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

	// u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;

	// Clear the render target. We probably don't need to do this every frame.
	//D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 1.0f, 0);

	// Set rendering target back to the EFB rendering texture
	D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());
}


static void D3DDumpFrame()
{
	if (EmuWindow::GetParentWnd())
	{
		// Re-stretch window to parent window size again, if it has a parent window.
		RECT rcWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);

		int width = rcWindow.right - rcWindow.left;
		int height = rcWindow.bottom - rcWindow.top;

		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
	}

	// Frame dumping routine - seems buggy and wrong, esp. regarding buffer sizes
	if (g_ActiveConfig.bDumpFrames) {
		D3DDISPLAYMODE DisplayMode;
		if (SUCCEEDED(D3D::dev->GetDisplayMode(0, &DisplayMode))) {
			LPDIRECT3DSURFACE9 surf;
			if (SUCCEEDED(D3D::dev->CreateOffscreenPlainSurface(DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surf, NULL))) {
				if (!s_LastFrameDumped) {
					RECT windowRect;
					GetClientRect(EmuWindow::GetWnd(), &windowRect);
					s_recordWidth = windowRect.right - windowRect.left;
					s_recordHeight = windowRect.bottom - windowRect.top;
					s_AVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), s_recordWidth, s_recordHeight);
					if (!s_AVIDumping) {
						PanicAlert("Error dumping frames to AVI.");
					} else {
						char msg [255];
						sprintf(msg, "Dumping Frames to \"%s/framedump0.avi\" (%dx%d RGB24)", FULL_FRAMES_DIR, s_recordWidth, s_recordHeight);
						OSD::AddMessage(msg, 2000);
					}
				}
				if (s_AVIDumping) {
					if (SUCCEEDED(D3D::dev->GetFrontBufferData(0, surf))) {
						RECT windowRect;
						GetWindowRect(EmuWindow::GetWnd(), &windowRect);
						D3DLOCKED_RECT rect;
						if (SUCCEEDED(surf->LockRect(&rect, &windowRect, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY))) {
							char *data = (char *)malloc(3 * s_recordWidth * s_recordHeight);
							formatBufferDump((const char *)rect.pBits, data, s_recordWidth, s_recordHeight, rect.Pitch);
							AVIDump::AddFrame(data);
							free(data);
							surf->UnlockRect();
						}
					}
				}
				s_LastFrameDumped = true;
				surf->Release();
			}
		}
	} 
	else 
	{
		if (s_LastFrameDumped && s_AVIDumping) {
			AVIDump::Stop();
			s_AVIDumping = false;
		}

		s_LastFrameDumped = false;
	}
}


void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if (g_bSkipCurrentFrame)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		DEBUGGER_PAUSE_LOG_AT(NEXT_XFB_CMD,false,{printf("RenderToXFB - disabled");});
		return;
	}

	D3D::EndFrame();
	D3DDumpFrame();
	EFBTextureToD3DBackBuffer(sourceRc);
	D3D::BeginFrame();

	DEBUGGER_LOG_AT((NEXT_XFB_CMD|NEXT_EFB_CMD|NEXT_FRAME),
		{printf("StretchRect, EFB->XFB\n");});
	DEBUGGER_PAUSE_LOG_AT(
		(NEXT_XFB_CMD),false,
	{printf("RenderToXFB: addr = %08X, %d x %d, sourceRc = (%d,%d,%d,%d)\n", 
		xfbAddr, fbWidth, fbHeight, 
		sourceRc.left, sourceRc.top, sourceRc.right, sourceRc.bottom);}
	);


	RECT rc;
	rc.left   = 0; 
	rc.top    = 0;
	rc.right  = (LONG)s_target_width;
	rc.bottom = (LONG)s_target_height;
	D3D::dev->SetScissorRect(&rc);
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, false);

	UpdateViewport();

	Swap(0,FIELD_PROGRESSIVE,0,0);	// we used to swap the buffer here, now we will wait
										// until the XFB pointer is updated by VI
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);
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

	rc.left   = (int)(rc.left   * xScale);
	rc.top    = (int)(rc.top    * yScale);
	rc.right  = (int)(rc.right  * xScale);
	rc.bottom = (int)(rc.bottom * yScale);

	if (rc.left < 0) rc.left = 0;
	if (rc.right > s_target_width) rc.right = s_target_width;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom > s_target_height) rc.bottom = s_target_height;

	if (rc.right >= rc.left && rc.bottom >= rc.top)
	{
		D3D::dev->SetScissorRect(&rc);
		return true;
	}
	else
	{
		WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
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
	
	//Get the working buffer
	LPDIRECT3DSURFACE9 pBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager::GetEFBDepthRTSurface() : FBManager::GetEFBColorRTSurface();
	//get the temporal buffer to move 1pixel data
	LPDIRECT3DSURFACE9 RBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager::GetEFBDepthReadSurface() : FBManager::GetEFBColorReadSurface();
	//get the memory buffer that can be locked
	LPDIRECT3DSURFACE9 pOffScreenBuffer = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager::GetEFBDepthOffScreenRTSurface() : FBManager::GetEFBColorOffScreenRTSurface();
	//get the buffer format
	D3DFORMAT BufferFormat = (type == PEEK_Z || type == POKE_Z) ? 
		FBManager::GetEFBDepthRTSurfaceFormat() : FBManager::GetEFBColorRTSurfaceFormat();
	
	D3DLOCKED_RECT drect;
	
	
	//Buffer not found alert
	if(!pBuffer) {
		PanicAlert("No %s!", (type == PEEK_Z || type == POKE_Z) ? "Z-Buffer" : "Color EFB");
		return 0;
	}
	// Z buffer lock not suported: returning
	if((type == PEEK_Z || type == POKE_Z) && BufferFormat == D3DFMT_D24X8)
	{
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
	
	//lock the buffer

	if(!(BufferFormat == D3DFMT_D32F_LOCKABLE || BufferFormat == D3DFMT_D16_LOCKABLE))
	{
		//the hard support stretchrect in both color and z so use it
		hr = D3D::dev->StretchRect(pBuffer,&RectToLock,RBuffer,NULL, D3DTEXF_NONE);
		if(FAILED(hr))
		{
			PanicAlert("Unable to stretch data to buffer");
			return 0;
		}
		//retriebe the pixel data to the local memory buffer
		D3D::dev->GetRenderTargetData(RBuffer,pOffScreenBuffer);
		if(FAILED(hr))
		{
			PanicAlert("Unable to copy data to mem buffer");
			return 0;
		}
		//change the rect to lock the entire one pixel buffer
		RectToLock.bottom = 1;
		RectToLock.left = 0;
		RectToLock.right = 1;
		RectToLock.top = 0;
	}	
	//the surface is good.. lock it
	if((hr = pOffScreenBuffer->LockRect(&drect, &RectToLock, D3DLOCK_READONLY)) != D3D_OK)
	{
		PanicAlert("ERROR: %s", hr == D3DERR_WASSTILLDRAWING ? "Still drawing" :
											  hr == D3DERR_INVALIDCALL     ? "Invalid call" : "w00t");	
		return 0;
	}
		
	switch(type) {
		case PEEK_Z:
			{			
			switch (BufferFormat)
			{
			case D3DFMT_D32F_LOCKABLE:
				val = ((float *)drect.pBits)[0];
				z = ((u32)(val * 0xffffff));// 0xFFFFFFFF;
				break;
			case D3DFMT_D16_LOCKABLE:
				val = ((float)((u16 *)drect.pBits)[0])/((float)0xFFFF);
				z = ((u32)(val * 0xffffff));
				break;
			default:
				z = ((u32 *)drect.pBits)[0] >> 8;
				break;
			};			
			// [0.0, 1.0] ==> [0, 0xFFFFFFFF]
			
			break;
			}
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

//		mtx.m[0][3] = pMatrix[1]; // -0.5f/s_target_width;  <-- fix d3d pixel center?
//		mtx.m[1][3] = pMatrix[3]; // +0.5f/s_target_height; <-- fix d3d pixel center?

// Called from VertexShaderManager
// Called from VertexShaderManager
void UpdateViewport()
{
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = (int)(ceil(xfregs.rawViewport[3] - xfregs.rawViewport[0] - (scissorXOff)) * MValueX);
	vp.Y = (int)(ceil(xfregs.rawViewport[4] + xfregs.rawViewport[1] - (scissorYOff)) * MValueY);
	vp.Width  = (int)ceil(abs((int)(2 * xfregs.rawViewport[0])) * MValueX);
	vp.Height = (int)ceil(abs((int)(2 * xfregs.rawViewport[1])) * MValueY);
	//some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
	vp.MinZ = 0.0f;//(xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
	vp.MaxZ = 1.0f;//xfregs.rawViewport[5] / 16777216.0f;
	D3D::dev->SetViewport(&vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Update the view port for clearing the picture
	D3DVIEWPORT9 vp;	
	vp.X = 0;
	vp.Y = 0;	
	vp.Width  = Renderer::GetTargetWidth();
	vp.Height = Renderer::GetTargetHeight();
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

    VertexShaderManager::SetViewportChanged();

	DWORD clearflags = 0;	
	if(colorEnable)
	{
		clearflags |= D3DCLEAR_TARGET;
	}
	if (zEnable)
	{
		clearflags |= D3DCLEAR_ZBUFFER;
	}

	D3D::dev->Clear(0, NULL, clearflags, color,(z & 0xFFFFFF) / float(0xFFFFFF), 0);	
}

void Renderer::SetBlendMode(bool forceUpdate)
{	
	// blend mode bit mask
	// 0 - blend enable
	// 2 - reverse subtract enable (else add)
	// 3-5 - srcRGB function
	// 6-8 - dstRGB function

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
	// this function is called after the XFB field is changed, not after
	// EFB is copied to XFB. In this way, flickering is reduced in games
	// and seems to also give more FPS in ZTP

	// D3D frame is now over
	// Clean out old stuff from caches.
	frameCount++;
	PixelShaderCache::Cleanup();
	VertexShaderCache::Cleanup();
	TextureCache::Cleanup();

	// Make any new configuration settings active.
	UpdateActiveConfig();

	//TODO: Resize backbuffer if window size has changed. This code crashes, debug it.
	g_VideoInitialize.pCopiedToXFB(false);

	CheckForResize();


	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();
}
