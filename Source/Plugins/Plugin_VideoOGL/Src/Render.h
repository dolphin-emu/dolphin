// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _GCOGL_RENDER_H_
#define _GCOGL_RENDER_H_

#include "TextureMngr.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

extern CGcontext g_cgcontext;
extern CGprofile g_cgvProf, g_cgfProf;

extern int frameCount;
extern int OSDChoice, OSDTime, OSDInternalW, OSDInternalH;

class Renderer
{

public:
    static bool Init();
    static void Shutdown();

    // initialize opengl standard values (like viewport)
    static bool InitializeGL();

    static void ResetGLState();
    static void RestoreGLState();

	static void SwapBuffers();

	static bool IsBlack();
    static void SetColorMask();
	static void SetBlendMode(bool forceUpdate);
	static bool SetScissorRect();

	// Render target management
    static int GetTargetWidth();
    static int GetTargetHeight();

	// Multiply any 2D EFB coordinates by these when rendering.
	static float GetTargetScaleX();
    static float GetTargetScaleY();

    static void SetFramebuffer(GLuint fb);
    static void SetRenderTarget(GLuint targ); // if targ is 0, sets to original render target

	// If in MSAA mode, this will perform a resolve of the specified rectangle, and return the resolve target as a texture ID.
	// Thus, this call may be expensive. Don't repeat it unnecessarily.
	// If not in MSAA mode, will just return the render target texture ID.
	// After calling this, before you render anything else, you MUST bind the framebuffer you want to draw to.
	static GLuint ResolveAndGetRenderTarget(const TRectangle &rect);

	// Same as above but for the depth Target.
	// After calling this, before you render anything else, you MUST bind the framebuffer you want to draw to.
    static GLuint ResolveAndGetDepthTarget(const TRectangle &rect);

	// Random utilities
    static void RenderText(const char* pstr, int left, int top, u32 color);
	static void DrawDebugText();
	static void SetScreenshot(const char *filename);
	static void FlipImageData(u8 *data, int w, int h);
	static bool SaveRenderTarget(const char *filename, int w, int h, int YOffset = 0);

    // Finish up the current frame, print some stats
    static void Swap(const TRectangle& rc);
};

void ComputeBackbufferRectangle(TRectangle *rc);

#endif // _GCOGL_RENDER_H_
