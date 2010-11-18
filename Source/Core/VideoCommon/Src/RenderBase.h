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

#ifndef _COMMON_RENDERBASE_H_
#define _COMMON_RENDERBASE_H_

#include "VideoCommon.h"
#include "Thread.h"
#include "MathUtil.h"
#include "pluginspecs_video.h"
#include "NativeVertexFormat.h"
#include "FramebufferManagerBase.h"
#include "BPMemory.h"

#include <string>

// TODO: Move these out of here.
extern int frameCount;
extern int OSDChoice, OSDTime, OSDInternalW, OSDInternalH;

extern bool s_bLastFrameDumped;
extern SVideoInitialize g_VideoInitialize;
extern PLUGIN_GLOBALS* globals;

// Renderer really isn't a very good name for this class - it's more like "Misc".
// The long term goal is to get rid of this class and replace it with others that make
// more sense.
class Renderer
{
public:
	Renderer();
	virtual ~Renderer();

	virtual void SetColorMask() = 0;
	virtual void SetBlendMode(bool forceUpdate) = 0;
	virtual bool SetScissorRect() = 0;
	virtual void SetGenerationMode() = 0;
	virtual void SetDepthMode() = 0;
	virtual void SetLogicOpMode() = 0;
	virtual void SetDitherMode() = 0;
	virtual void SetLineWidth() = 0;
	virtual void SetSamplerState(int stage,int texindex) = 0;
	virtual void SetInterlacingMode() = 0;

	// Return the rendering target width and height
	static int GetTargetWidth() { return s_target_width; }
	static int GetTargetHeight() { return s_target_height; }

	static int GetFullTargetWidth() { return s_Fulltarget_width; }
	static int GetFullTargetHeight() { return s_Fulltarget_height; }

	// Multiply any 2D EFB coordinates by these when rendering.
	static float GetTargetScaleX() { return EFBxScale; }
	static float GetTargetScaleY() { return EFByScale; }

	static float GetXFBScaleX() { return xScale; }
	static float GetXFBScaleY() { return yScale; }

	static int GetBackbufferWidth() { return s_backbuffer_width; }
	static int GetBackbufferHeight() { return s_backbuffer_height; }

	virtual TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) = 0;

	// Random utilities
	static void SetScreenshot(const char *filename);
	static void DrawDebugText();

	virtual void RenderText(const char* pstr, int left, int top, u32 color) = 0;

	virtual void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) = 0;
	static void RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) = 0;

	// What's the real difference between these? Too similar names.
	virtual void ResetAPIState() = 0;
	virtual void RestoreAPIState() = 0;

	// Finish up the current frame, print some stats
	virtual void Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight, const EFBRectangle& rc) = 0;

	virtual void UpdateViewport() = 0;

	virtual bool SaveScreenshot(const std::string &filename, const TargetRectangle &rc) = 0;

protected:

	static Common::CriticalSection s_criticalScreenshot;
	static std::string s_sScreenshotName;

	static bool CalculateTargetSize(float multiplier = 1);

	static volatile bool s_bScreenshot;

	// The framebuffer size
	static int s_target_width;
	static int s_target_height;

	// The custom resolution
	static int s_Fulltarget_width;
	static int s_Fulltarget_height;

	// TODO: Add functionality to reinit all the render targets when the window is resized.
	static int s_backbuffer_width;
	static int s_backbuffer_height;

	// Internal resolution scale (related to xScale/yScale for "Auto" scaling)
	static float EFBxScale;
	static float EFByScale;

	// ratio of backbuffer size and render area size
	static float xScale;
	static float yScale;

	static unsigned int s_XFB_width;
	static unsigned int s_XFB_height;

	// can probably eliminate this static var
	static int s_LastEFBScale;

	static bool s_skipSwap;
	static bool XFBWrited;
};

extern Renderer *g_renderer;

void UpdateViewport();

template <typename R>
void GetScissorRect(MathUtil::Rectangle<R> &rect)
{
	const int xoff = bpmem.scissorOffset.x * 2 - 342;
	const int yoff = bpmem.scissorOffset.y * 2 - 342;

	rect.left   = (R)((float)bpmem.scissorTL.x - xoff - 342);
	rect.top    = (R)((float)bpmem.scissorTL.y - yoff - 342);
	rect.right  = (R)((float)bpmem.scissorBR.x - xoff - 341);
	rect.bottom = (R)((float)bpmem.scissorBR.y - yoff - 341);
}

#endif // _COMMON_RENDERBASE_H_
