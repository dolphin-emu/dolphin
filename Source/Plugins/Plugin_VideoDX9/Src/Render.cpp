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

#include "Config.h"
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

#include "debugger/debugger.h"

static float m_targetWidth;
static float m_targetHeight;
static float xScale;
static float yScale;

static int m_recordWidth;
static int m_recordHeight;

static bool m_LastFrameDumped;
static bool m_AVIDumping;

#define NUMWNDRES 6
extern int g_Res[NUMWNDRES][2];

bool Renderer::Init() 
{
    EmuWindow::SetSize(g_Res[g_Config.iWindowedRes][0], g_Res[g_Config.iWindowedRes][1]);

    D3D::Create(g_Config.iAdapter, EmuWindow::GetWnd(), g_Config.bFullscreen, g_Config.iFSResolution, g_Config.iMultisampleMode);

	m_targetWidth  = (float)D3D::GetDisplayWidth();
	m_targetHeight = (float)D3D::GetDisplayHeight();

	xScale = m_targetWidth / (float)EFB_WIDTH;
	yScale = m_targetHeight / (float)EFB_HEIGHT;

	m_LastFrameDumped = false;
	m_AVIDumping = false;

	// We're not using fixed function, except for some 2D.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);
	D3D::font.Init();

	for (int i = 0; i < 8; i++)
		D3D::dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 16);

	D3D::BeginFrame(true, 0);
	VertexManager::BeginFrame();
	return true;
}

void Renderer::Shutdown()
{
	D3D::font.Shutdown();
	D3D::EndFrame();
	D3D::Close();

	if (m_AVIDumping)
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
	result.left   = (rc.left   * m_targetWidth)  / EFB_WIDTH;
	result.top    = (rc.top    * m_targetHeight) / EFB_HEIGHT;
	result.right  = (rc.right  * m_targetWidth)  / EFB_WIDTH;
	result.bottom = (rc.bottom * m_targetHeight) / EFB_HEIGHT;
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
	if (g_Config.bDumpFrames) {
		D3DDISPLAYMODE DisplayMode;
		if (SUCCEEDED(D3D::dev->GetDisplayMode(0, &DisplayMode))) {
			LPDIRECT3DSURFACE9 surf;
			if (SUCCEEDED(D3D::dev->CreateOffscreenPlainSurface(DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surf, NULL))) {
				if (!m_LastFrameDumped) {
					RECT windowRect;
					GetWindowRect(EmuWindow::GetWnd(), &windowRect);
					m_recordWidth = windowRect.right - windowRect.left;
					m_recordHeight = windowRect.bottom - windowRect.top;
					m_AVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), m_recordWidth, m_recordHeight);
					if (!m_AVIDumping) {
						PanicAlert("Error dumping frames to AVI.");
					} else {
						char msg [255];
						sprintf(msg, "Dumping Frames to \"%s/framedump0.avi\" (%dx%d RGB24)", FULL_FRAMES_DIR, m_recordWidth, m_recordHeight);
						OSD::AddMessage(msg, 2000);
					}
				}
				if (m_AVIDumping) {
					if (SUCCEEDED(D3D::dev->GetFrontBufferData(0, surf))) {
						RECT windowRect;
						GetWindowRect(EmuWindow::GetWnd(), &windowRect);
						D3DLOCKED_RECT rect;
						if (SUCCEEDED(surf->LockRect(&rect, &windowRect, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY))) {
							char *data = (char *)malloc(3 * m_recordWidth * m_recordHeight);
							formatBufferDump((const char *)rect.pBits, data, m_recordWidth, m_recordHeight, rect.Pitch);
							AVIDump::AddFrame(data);
							free(data);
							surf->UnlockRect();
						}
					}
				}
				m_LastFrameDumped = true;
				surf->Release();
			}
		}
	} 
	else 
	{
		if(m_LastFrameDumped && m_AVIDumping) {
			AVIDump::Stop();
			m_AVIDumping = false;
		}

		m_LastFrameDumped = false;
	}

	char st[8192];
	// Finish up the current frame, print some stats
	if (g_Config.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}

	if (g_Config.bOverlayProjStats)
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

	//Begin new frame
	//Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = (DWORD)m_targetWidth;
	vp.Height = (DWORD)m_targetHeight;
	vp.MinZ = 0;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT rc;
	rc.left   = 0; 
	rc.top    = 0;
	rc.right  = (LONG)m_targetWidth;
	rc.bottom = (LONG)m_targetHeight;
	D3D::dev->SetScissorRect(&rc);
	D3D::dev->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

	// We probably shouldn't clear here.
	D3D::dev->Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 0, 0);

	u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
	D3D::BeginFrame(false, clearColor, 1.0f);
	
	// This probably causes problems, and the visual difference is tiny anyway.
	// So let's keep it commented out.
	// D3D::EnableAlphaToCoverage();

	VertexManager::BeginFrame();

	if (g_Config.bOldCard)
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
	if (rc.right >= rc.left && rc.bottom >= rc.top) {
		D3D::dev->SetScissorRect(&rc);
		return true;
	}
	else
	{
		WARN_LOG(VIDEO, "SCISSOR ERROR");
		return false;
	}
}

void Renderer::SetColorMask() 
{
	DWORD write = 0;
	if (bpmem.blendmode.alphaupdate) 
		write = D3DCOLORWRITEENABLE_ALPHA;
	if (bpmem.blendmode.colorupdate) 
		write |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;

	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, write);
}

//		mtx.m[0][3] = pMatrix[1]; // -0.5f/m_targetWidth;  <-- fix d3d pixel center?
//		mtx.m[1][3] = pMatrix[3]; // +0.5f/m_targetHeight; <-- fix d3d pixel center?

// Called from VertexShaderManager
void UpdateViewport()
{
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;
	// -------------------------------------

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
