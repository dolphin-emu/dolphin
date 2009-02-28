// Copyright (C) 2003-2008 Dolphin Project.

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

// How the non-true-XFB mode COULD work:
//
// The game renders to the EFB:
//  
// ----------------------+
// |                     |
// |                     |
// |                     |
// |                     |
// |                     |
// |                     |   efb_height
// |                     |
// |                     |
// | - - - - - - - - - - |
// |                     |
// +---------------------+
//        efb_width
//
// At XFB blit time, the top 640-xxx X XXX part of the above buffer (size dotted below),
// should be stretch blitted into the inner rectangle of the window:
// +-----------------------------------------+
// |       |                        |        |
// |       |                     .  |        |
// |       |                        |        |
// |       |                     .  |        |
// |       |                        |        |
// |       |                     .  |        |  OpenGL_Height()
// |       |                        |        |
// |       |                     .  |        |
// |       | - - - - - - - - - -    |        |
// |       |                      \ |        |
// +-------+---------------------------------+
//                OpenGL_Width()
//
//
// efb_width and efb_height can even be adjusted so that the last blit will result
// in a 1:1 rather than a stretch. That would require creating a bigger efb from the
// start though.
//
// The above is not how it works today.

/*
int win_w = OpenGL_Width();
int win_h = OpenGL_Height();

int blit_w_640 = last_xfb_specified_width;
int blit_h_640 = last_xfb_specified_height;
*/

#ifndef GCOGL_RENDER
#define GCOGL_RENDER

#include "TextureMngr.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

extern CGcontext g_cgcontext;
extern CGprofile g_cgvProf, g_cgfProf;

extern int frameCount;

class Renderer
{
private:
    static void FlushZBufferAlphaToTarget();

public:
    enum RenderMode
    {
        RM_Normal=0,     // normal target as color0, ztarget as color1
        RM_ZBufferOnly,  // zbuffer as color0
        RM_ZBufferAlpha, // zbuffer as color0, also will dump alpha info to regular target once mode is switched
                         // use stencil buffer to indicate what pixels were written
    };

	static bool Init();
    static void Shutdown();

    // initialize opengl standard values (like viewport)
    static bool InitializeGL();

    static void ResetGLState();
    static void RestoreGLState();

	static void SwapBuffers();

	static bool IsUsingATIDrawBuffers();
    static bool HaveStencilBuffer();

    static void SetColorMask();
	static void SetBlendMode(bool forceUpdate);
	static bool SetScissorRect();

    static void SetRenderMode(RenderMode mode);
    static RenderMode GetRenderMode();

	// Render target management
    static int GetTargetWidth();
    static int GetTargetHeight();

	// Multiply any 0-640 / 0-480 coordinates by these when rendering.
	static float GetTargetScaleX();
    static float GetTargetScaleY();

    static void SetFramebuffer(GLuint fb);
	static void SetZBufferRender(); // sets rendering of the zbuffer using MRTs
    static void SetRenderTarget(GLuint targ); // if targ is 0, sets to original render target
    static void SetDepthTarget(GLuint targ);

	static GLuint GetRenderTarget();
    static GLuint GetZBufferTarget();

	// Random utilities
    static void RenderText(const char* pstr, int left, int top, u32 color);
	static void DrawDebugText();
	static void SetScreenshot(const char *filename);
	static bool SaveRenderTarget(const char *filename, int w, int h);

    // Finish up the current frame, print some stats
    static void Swap(const TRectangle& rc);
};

void ComputeBackbufferRectangle(TRectangle *rc);

#endif
