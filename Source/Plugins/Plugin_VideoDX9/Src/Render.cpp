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
#include "TextureCache.h"
#include "Utils.h"
#include "EmuWindow.h"
#include "AVIDump.h"
#include "OnScreenDisplay.h"
#include "Fifo.h"

#include "debugger/debugger.h"

static int s_targetWidth;
static int s_targetHeight;

static int s_backbuffer_width;
static int s_backbuffer_height;

static float xScale;
static float yScale;

static int s_recordWidth;
static int s_recordHeight;

static bool s_LastFrameDumped;
static bool s_AVIDumping;

#define NUMWNDRES 6
extern int g_Res[NUMWNDRES][2];

bool Renderer::Init() 
{
	UpdateActiveConfig();
    EmuWindow::SetSize(g_Res[g_ActiveConfig.iWindowedRes][0], g_Res[g_ActiveConfig.iWindowedRes][1]);

	int backbuffer_ms_mode = g_ActiveConfig.iMultisampleMode;
    D3D::Create(g_ActiveConfig.iAdapter, EmuWindow::GetWnd(), g_ActiveConfig.bFullscreen,
		        g_ActiveConfig.iFSResolution, backbuffer_ms_mode);

	s_targetWidth  = D3D::GetDisplayWidth();
	s_targetHeight = D3D::GetDisplayHeight();

	s_backbuffer_width = s_targetWidth;
	s_backbuffer_height = s_targetHeight;

	xScale = (float)s_targetWidth / (float)EFB_WIDTH;
	yScale = (float)s_targetHeight / (float)EFB_HEIGHT;

	s_LastFrameDumped = false;
	s_AVIDumping = false;

	// We're not using fixed function, except for some 2D.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);
	D3D::font.Init();

	for (int i = 0; i < 8; i++)
		D3D::dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 16);

	D3D::BeginFrame(true, 0, 1.0f);
	return true;
}

void Renderer::Shutdown()
{
	D3D::font.Shutdown();
	D3D::EndFrame();
	D3D::Close();

	if (s_AVIDumping)
	{
		AVIDump::Stop();
	}
}

float Renderer::GetTargetScaleX()
{
	return xScale;
}
float Renderer::GetTargetScaleY()
{
	return yScale;
}

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
	result.left   = (rc.left   * s_targetWidth)  / EFB_WIDTH;
	result.top    = (rc.top    * s_targetHeight) / EFB_HEIGHT;
	result.right  = (rc.right  * s_targetWidth)  / EFB_WIDTH;
	result.bottom = (rc.bottom * s_targetHeight) / EFB_HEIGHT;
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

void Renderer::SwapBuffers()
{
	if (g_bSkipCurrentFrame)
		return;

	// Center window again.
	if (EmuWindow::GetParentWnd())
	{
		RECT rcWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);

		int width = rcWindow.right - rcWindow.left;
		int height = rcWindow.bottom - rcWindow.top;

		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
	}

	// Frame dumping routine
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
		if(s_LastFrameDumped && s_AVIDumping) {
			AVIDump::Stop();
			s_AVIDumping = false;
		}

		s_LastFrameDumped = false;
	}

	char st[8192];
	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}

	if (g_ActiveConfig.bOverlayProjStats)
	{
		Statistics::ToStringProj(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}

	OSD::DrawMessages();

#if defined(DVPROFILE)
    if (g_bWriteProfile) {
        //g_bWriteProfile = 0;
        static int framenum = 0;
        const int UPDATE_FRAMES = 8;
        if (++framenum >= UPDATE_FRAMES) {
            DVProfWrite("prof.txt", UPDATE_FRAMES);
            DVProfClear();
            framenum = 0;
        }
    }
#endif

	D3D::EndFrame();

	DEBUGGER_PAUSE_COUNT_N_WITHOUT_UPDATE(NEXT_FRAME);

	// D3D frame is now over
	// Clean out old stuff from caches.
	frameCount++;
	PixelShaderCache::Cleanup();
	VertexShaderCache::Cleanup();
	TextureCache::Cleanup();

	// Make any new configuration settings active.
	UpdateActiveConfig();

	/*
	TODO: Resize backbuffer if window size has changed. This code crashes, debug it.

	RECT rcWindow;
	GetClientRect(EmuWindow::GetWnd(), &rcWindow);
	if (rcWindow.right - rcWindow.left != s_backbuffer_width ||
		rcWindow.bottom - rcWindow.top != s_backbuffer_height)
	{
		D3D::Reset();
		s_backbuffer_width = D3D::GetDisplayWidth();
		s_backbuffer_height = D3D::GetDisplayHeight();
	}
	*/

	//Begin new frame
	//Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = (DWORD)s_targetWidth;
	vp.Height = (DWORD)s_targetHeight;
	vp.MinZ = 0;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT rc;
	rc.left   = 0; 
	rc.top    = 0;
	rc.right  = (LONG)s_targetWidth;
	rc.bottom = (LONG)s_targetHeight;
	D3D::dev->SetScissorRect(&rc);
	D3D::dev->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

	// We probably shouldn't clear here.
	// D3D::dev->Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 0, 0);

	u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
	D3D::BeginFrame(false, clearColor, 1.0f);
	
	// This probably causes problems, and the visual difference is tiny anyway.
	// So let's keep it commented out.
	// D3D::EnableAlphaToCoverage();

	UpdateViewport();

	if (g_ActiveConfig.bOldCard)
		D3D::font.SetRenderStates(); //compatibility with low end cards
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
	// Get the rectangular target region covered by the EFB pixel.
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{
	case PEEK_Z:
	{
		// if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			// g_framebufferManager.GetEFBDepthTexture(efbPixelRc);
			// glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		u32 z = 0;
		// glReadPixels(srcX, srcY, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &z);

		// Scale the 32-bit value returned by glReadPixels to a 24-bit
		// value (GC uses a 24-bit Z-buffer).
		// TODO: in RE0 this value is often off by one, which causes lighting to disappear
		return z >> 8;
	}

	case POKE_Z:
		// TODO: Implement
		break;

	case PEEK_COLOR: // GXPeekARGB
	{
		// Although it may sound strange, this really is A8R8G8B8 and not RGBA or 24-bit...

		// Tested in Killer 7, the first 8bits represent the alpha value which is used to
		// determine if we're aiming at an enemy (0x80 / 0x88) or not (0x70) 
		// Wind Waker is also using it for the pictograph to determine the color of each pixel

		// if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			// g_framebufferManager.GetEFBColorTexture(efbPixelRc);
			// glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		// Read back pixel in BGRA format, then byteswap to get GameCube's ARGB Format.
		u32 color = 0;
		// glReadPixels(srcX, srcY, 1, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &color);

		return color;
	}

	case POKE_COLOR:
		// TODO: Implement. One way is to draw a tiny pixel-sized rectangle at
		// the exact location. Note: EFB pokes are susceptible to Z-buffering
		// and perhaps blending.
		// WARN_LOG(VIDEOINTERFACE, "This is probably some kind of software rendering");
		break;

	}

	return 0;
}

//		mtx.m[0][3] = pMatrix[1]; // -0.5f/s_targetWidth;  <-- fix d3d pixel center?
//		mtx.m[1][3] = pMatrix[3]; // +0.5f/s_targetHeight; <-- fix d3d pixel center?

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
	
	vp.MinZ = (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777215.0f;
	vp.MaxZ = xfregs.rawViewport[5] / 16777215.0f;
	
	D3D::dev->SetViewport(&vp);
}
