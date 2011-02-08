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

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The gpu draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------


#include "RenderBase.h"
#include "Atomic.h"
#include "MainBase.h"
#include "VideoConfig.h"
#include "FramebufferManagerBase.h"
#include "TextureCacheBase.h"
#include "Fifo.h"
#include "Timer.h"
#include "StringUtil.h"

#include <cmath>
#include <string>

// TODO: Move these out of here.
int frameCount;
int OSDChoice, OSDTime;

Renderer *g_renderer;

bool s_bLastFrameDumped = false;
Common::CriticalSection Renderer::s_criticalScreenshot;
std::string Renderer::s_sScreenshotName;

volatile bool Renderer::s_bScreenshot;

// The framebuffer size
int Renderer::s_target_width;
int Renderer::s_target_height;

// The custom resolution
int Renderer::s_Fulltarget_width;
int Renderer::s_Fulltarget_height;

// TODO: Add functionality to reinit all the render targets when the window is resized.
int Renderer::s_backbuffer_width;
int Renderer::s_backbuffer_height;

// ratio of backbuffer size and render area size
float Renderer::xScale;
float Renderer::yScale;

unsigned int Renderer::s_XFB_width;
unsigned int Renderer::s_XFB_height;

int Renderer::s_LastEFBScale;

bool Renderer::s_skipSwap;
bool Renderer::XFBWrited;

unsigned int Renderer::prev_efb_format = (unsigned int)-1;

Renderer::Renderer()
{
	UpdateActiveConfig();
}

Renderer::~Renderer()
{
	// invalidate previous efb format
	prev_efb_format = (unsigned int)-1;
}

void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
	if (!fbWidth || !fbHeight)
		return;

	s_skipSwap = g_bSkipCurrentFrame;

	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	XFBWrited = true;

	// XXX: Without the VI, how would we know what kind of field this is? So
	// just use progressive.
	if (g_ActiveConfig.bUseXFB)
	{
		FramebufferManagerBase::CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc,Gamma);
	}
	else
	{
		g_renderer->Swap(xfbAddr, FIELD_PROGRESSIVE, fbWidth, fbHeight,sourceRc,Gamma);
		Common::AtomicStoreRelease(s_swapRequested, false);
	}
	
	if (TextureCache::DeferredInvalidate)
	{
		TextureCache::Invalidate(false);
	}
}

void Renderer::CalculateTargetScale(int x, int y, int &scaledX, int &scaledY)
{
	switch (g_ActiveConfig.iEFBScale)
	{
		case 3: // 2x
			scaledX = x * 2;
			scaledY = y * 2;
			break;
		case 4: // 3x
			scaledX = x * 3;
			scaledY = y * 3;
			break;
		case 5: // 0.75x
			scaledX = (x * 3) / 4;
			scaledY = (y * 3) / 4;
			break;
		case 6: // 0.5x
			scaledX = x / 2;
			scaledY = y / 2;
			break;
		case 7: // 0.375x
			scaledX = (x * 3) / 8;
			scaledY = (y * 3) / 8;
			break;
		default:
			scaledX = x;
			scaledY = y;
	};
}

// return true if target size changed
bool Renderer::CalculateTargetSize(int multiplier)
{
	int newEFBWidth, newEFBHeight;
	switch (s_LastEFBScale)
	{
		case 0: // fractional
			newEFBWidth = (int)(EFB_WIDTH * xScale);
			newEFBHeight = (int)(EFB_HEIGHT * yScale);
			break;
		case 1: // integral
			newEFBWidth = EFB_WIDTH * (int)ceilf(xScale);
			newEFBHeight = EFB_HEIGHT * (int)ceilf(yScale);
			break;
		default:
			CalculateTargetScale(EFB_WIDTH, EFB_HEIGHT, newEFBWidth, newEFBHeight);
	}

	newEFBWidth *= multiplier;
	newEFBHeight *= multiplier;

	s_Fulltarget_width = newEFBWidth;
	s_Fulltarget_height = newEFBHeight;

	if (newEFBWidth != s_target_width || newEFBHeight != s_target_height)
	{
		s_target_width  = newEFBWidth;
		s_target_height = newEFBHeight;
		return true;
	}
	return false;
}

void Renderer::SetScreenshot(const char *filename)
{
	s_criticalScreenshot.Enter();
	s_sScreenshotName = filename;
	s_bScreenshot = true;
	s_criticalScreenshot.Leave();
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
			const char* res_text = "";
			switch (g_ActiveConfig.iEFBScale)
			{
			case 0:
				res_text = "Auto (fractional)";
				break;
			case 1:
				res_text = "Auto (integral)";
				break;
			case 2:
				res_text = "Native";
				break;
			case 3:
				res_text = "2x";
				break;
			case 4:
				res_text = "3x";
				break;
			case 5:
				res_text = "0.75x";
				break;
			case 6:
				res_text = "0.5x";
				break;
			case 7:
				res_text = "0.375x";
				break;
			}

			const char* ar_text = "";
			switch(g_ActiveConfig.iAspectRatio)
			{
			case ASPECT_AUTO:
				ar_text = "Auto";
				break;
			case ASPECT_FORCE_16_9:
				ar_text = "16:9";
				break;
			case ASPECT_FORCE_4_3:
				ar_text = "4:3";
				break;
			case ASPECT_STRETCH:
				ar_text = "Stretch";
				break;
			}

			const char* const efbcopy_text = g_ActiveConfig.bEFBCopyEnable ?
				(g_ActiveConfig.bCopyEFBToTexture ? "to Texture" : "to RAM") : "Disabled";

			// The rows
			const std::string lines[] =
			{
				std::string("3: Internal Resolution: ") + res_text,
				std::string("4: Aspect Ratio: ") + ar_text + (g_ActiveConfig.bCrop ? " (crop)" : ""),
				std::string("5: Copy EFB: ") + efbcopy_text,
				std::string("6: Fog: ") + (g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"),
				std::string("7: Material Lighting: ") + (g_ActiveConfig.bDisableLighting ? "Disabled" : "Enabled"),
			};

			enum { lines_count = sizeof(lines)/sizeof(*lines) };

			std::string final_yellow, final_cyan;

			// If there is more text than this we will have a collision
			if (g_ActiveConfig.bShowFPS)
			{
				final_yellow = final_cyan = "\n\n";
			}

			// The latest changed setting in yellow
			for (int i = 0; i != lines_count; ++i)
			{
				if (OSDChoice == -i - 1)
					final_yellow += lines[i];
				final_yellow += '\n';
			}

			// The other settings in cyan
			for (int i = 0; i != lines_count; ++i)
			{
				if (OSDChoice != -i - 1)
					final_cyan += lines[i];
				final_cyan += '\n';
			}

			// Render a shadow
			g_renderer->RenderText(final_cyan.c_str(), 21, 21, 0xDD000000);
			g_renderer->RenderText(final_yellow.c_str(), 21, 21, 0xDD000000);
			//and then the text
			g_renderer->RenderText(final_cyan.c_str(), 20, 20, 0xFF00FFFF);
			g_renderer->RenderText(final_yellow.c_str(), 20, 20, 0xFFFFFF00);
		}
	}
}

void Renderer::CalculateXYScale(const TargetRectangle& dst_rect)
{
	if (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB)
	{
		xScale = 1.0f;
		yScale = 1.0f;
	}
	else
	{
		if (g_ActiveConfig.b3DVision)
		{
			// This works, yet the version in the else doesn't. No idea why.
			xScale = (float)(s_backbuffer_width-1) / (float)(s_XFB_width-1);
			yScale = (float)(s_backbuffer_height-1) / (float)(s_XFB_height-1);
		}
		else
		{
			xScale = (float)(dst_rect.right - dst_rect.left - 1) / (float)(s_XFB_width-1);
			yScale = (float)(dst_rect.bottom - dst_rect.top - 1) / (float)(s_XFB_height-1);
		}
	}
}

void UpdateViewport()
{
	g_renderer->UpdateViewport();
}
