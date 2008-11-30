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

#ifndef GCOGL_RENDER
#define GCOGL_RENDER

#include "TextureMngr.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

extern CGcontext g_cgcontext;
extern CGprofile g_cgvProf, g_cgfProf;

extern float MValueX, MValueY;
extern int frameCount;

class Renderer
{
    static void FlushZBufferAlphaToTarget();

public:
    enum RenderMode
    {
        RM_Normal=0, // normal target as color0, ztarget as color1
        RM_ZBufferOnly, // zbuffer as color 0
        RM_ZBufferAlpha // zbuffer as color0, also will dump alpha info to regular target once mode is switched
                            // use stencil buffer to indicate what pixels were written
    };

	static bool Create2();
    static void Shutdown();

    // initialize opengl standard values (like viewport)
    static bool Initialize();

    static void AddMessage(const char* str, u32 ms);
    static void ProcessMessages(); // draw the current messages on the screen
    static void RenderText(const char* pstr, int left, int top, u32 color);
    static void SetAA(int aa); // sets the anti-aliasing level

    static void ReinitView(int nNewWidth, int nNewHeight);

    static int GetTargetWidth();
    static int GetTargetHeight();
    static bool CanBlendLogicOp();
    static void SetCgErrorOutput(bool bOutput);

    static void ResetGLState();
    static void RestoreGLState();
    static bool IsUsingATIDrawBuffers();
    static bool HaveStencilBuffer();

    static void SetZBufferRender(); // sets rendering of the zbuffer using MRTs
    static u32 GetZBufferTarget();

    static void SetColorMask();
	static bool SetScissorRect();

    static void SetRenderMode(RenderMode mode);
    static RenderMode GetRenderMode();

    static void SetRenderTarget(u32 targ); // if targ is 0, sets to original render target
    static void SetDepthTarget(u32 targ);
    static void SetFramebuffer(u32 fb);
    static u32 GetRenderTarget();

    // Finish up the current frame, print some stats
    static void Swap(const TRectangle& rc);

	static void SwapBuffers();

    static bool SaveRenderTarget(const char* filename, int jpeg);
};

#endif
