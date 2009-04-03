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


// ----------------------------------------------------------------------------------------------------------
// This file handles the External Frame Buffer (XFB). The XFB is a storage point when the picture is resized
// by the system to the correct display format for output to the TV. In most cases its function can be
// supplemented by the equivalent adjustments in glScissor and glViewport (or their DirectX equivalents). But
// for some homebrew games these functions are necessary because the homebrew game communicate directly with
// them.
// ----------------------------------------------------------------------------------------------------------

#include "Globals.h"
#include "GLUtil.h"
#include "MemoryUtil.h"
#include "Render.h"
#include "TextureMngr.h"
#include "VertexShaderManager.h"
#include "XFBConvert.h"
#include "TextureConverter.h"

#define XFB_USE_SHADERS 1

enum {
	XFB_BUF_HEIGHT = 574, //480,
	// TODO: figure out what to do with PAL
};


#if XFB_USE_SHADERS

static GLuint xfb_decoded_texture;
static int XFBInitStatus = 0;
static struct 
{ 
	u8* pXFB;
	u32 width;
	u32 height;
	s32 yOffset;
} tUpdateXFBArgs;

void XFB_SetUpdateArgs(u8* _pXFB, u32 _dwWidth, u32 _dwHeight, s32 _dwYOffset);

void XFB_Init()
{
	glGenTextures(1, &xfb_decoded_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfb_decoded_texture);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, XFB_WIDTH, XFB_BUF_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	XFBInitStatus = 1;
}

void XFB_Shutdown()
{
	glDeleteTextures(1, &xfb_decoded_texture);
	XFBInitStatus = 0;
}

int XFB_isInit()
{
	return XFBInitStatus;
}

void XFB_Write(u8 *xfb_in_ram, const TRectangle& sourceRc, u32 dstWd, u32 dstHt)
{
	TRectangle renderSrcRc;
	renderSrcRc.left = sourceRc.left;
	renderSrcRc.right = sourceRc.right;
	// OpenGL upside down as usual...
	renderSrcRc.top = Renderer::GetTargetHeight() - sourceRc.top;
	renderSrcRc.bottom = Renderer::GetTargetHeight() - sourceRc.bottom; 
	TextureConverter::EncodeToRamYUYV(Renderer::ResolveAndGetRenderTarget(sourceRc), renderSrcRc, xfb_in_ram, dstWd, dstHt);
}

// Draw the XFB straight to the OpenGL backbuffer.
void XFB_Draw(u8 *xfb_in_ram, u32 width, u32 height, s32 yOffset)
{
	TextureConverter::DecodeToTexture(xfb_in_ram, width, height, xfb_decoded_texture);

	OpenGL_Update(); // just updates the render window position and the backbuffer size
	Renderer::ResetGLState();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the backbuffer

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfb_decoded_texture);

	TRectangle back_rc;
	ComputeBackbufferRectangle(&back_rc);

	glViewport(back_rc.left, back_rc.top, back_rc.right - back_rc.left, back_rc.bottom - back_rc.top);
	GL_REPORT_ERRORD();

	float w = (float)width;
	float h = (float)height;
	float yOff = (float)yOffset;

    glBegin(GL_QUADS);
	glTexCoord2f(w, 0 - yOff);  glVertex2f(1, -1);
	glTexCoord2f(w, h - yOff);  glVertex2f(1, 1);
	glTexCoord2f(0, h - yOff);  glVertex2f(-1, 1);
	glTexCoord2f(0, 0 - yOff);  glVertex2f(-1,-1);
    glEnd();	

	TextureMngr::DisableStage(0);

	Renderer::SwapBuffers();

	Renderer::RestoreGLState();
	GL_REPORT_ERRORD();
}

void XFB_Draw()
{
	XFB_Draw(tUpdateXFBArgs.pXFB, tUpdateXFBArgs.width, tUpdateXFBArgs.height, tUpdateXFBArgs.yOffset);
}

void XFB_SetUpdateArgs(u8* _pXFB, u32 _dwWidth, u32 _dwHeight, s32 _dwYOffset)
{
	tUpdateXFBArgs.pXFB = _pXFB;
	tUpdateXFBArgs.width = _dwWidth;
	tUpdateXFBArgs.height = _dwHeight;
	tUpdateXFBArgs.yOffset = _dwYOffset;
}

#else

static GLuint xfb_texture;
static u8 *xfb_buffer = 0;
static u8 *efb_buffer = 0;
static GLuint s_xfbFrameBuffer = 0;
static GLuint s_xfbRenderBuffer = 0;

void XFB_Init()
{
	// used to render XFB
	xfb_buffer = new u8[XFB_WIDTH * XFB_BUF_HEIGHT * 4];
	memset(xfb_buffer, 0, XFB_WIDTH * XFB_BUF_HEIGHT * 4);
    glGenTextures(1, &xfb_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfb_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, XFB_WIDTH, XFB_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, xfb_buffer);

	// used to render EFB
	glGenFramebuffersEXT(1, &s_xfbFrameBuffer);
	glGenRenderbuffersEXT(1, &s_xfbRenderBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_xfbRenderBuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, nBackbufferWidth, nBackbufferHeight);

	// Ensure efb_buffer is aligned.
	efb_buffer = (u8 *)AllocateMemoryPages(nBackbufferWidth * nBackbufferHeight * 4);
}

void XFB_Shutdown()
{
	glDeleteFramebuffersEXT(1, &s_xfbFrameBuffer);

	glDeleteTextures(1, &xfb_texture);
	xfb_texture = 0;
	delete [] xfb_buffer;
	xfb_buffer = 0;
	FreeMemoryPages(efb_buffer, nBackbufferWidth * nBackbufferHeight * 4);
}


void XFB_Write(u8 *xfb_in_ram, const TRectangle& sourceRc, u32 dstWd, u32 dstHt)
{
	Renderer::SetRenderMode(Renderer::RM_Normal);
	Renderer::ResetGLState();

	// Switch to XFB frame buffer.
	Renderer::SetFramebuffer(s_xfbFrameBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_xfbRenderBuffer);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_xfbRenderBuffer);
	GL_REPORT_ERRORD();

	glViewport(nXoff, nYoff, nBackbufferWidth, nBackbufferHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, Renderer::GetRenderTarget());
    TextureMngr::EnableTexRECT(0);
	for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);	
	GL_REPORT_ERRORD();

	glBegin(GL_QUADS);
    glTexCoord2f(0, nBackbufferHeight); glVertex2f(-1,-1);
	glTexCoord2f(0, 0); glVertex2f(-1,1);
    glTexCoord2f(nBackbufferWidth, 0); glVertex2f(1,1);
    glTexCoord2f(nBackbufferWidth, nBackbufferHeight); glVertex2f(1,-1);
    glEnd();
	GL_REPORT_ERRORD();

	int width  = sourceRc.right  - sourceRc.left;
	int height = sourceRc.bottom - sourceRc.top;
	glReadPixels(sourceRc.left, sourceRc.top, width, height, GL_RGBA, GL_UNSIGNED_BYTE, efb_buffer);
	GL_REPORT_ERRORD();

	Renderer::SetFramebuffer(0);
    Renderer::RestoreGLState();
    VertexShaderManager::SetViewportChanged();
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();

	ConvertToXFB((u32 *)xfb_in_ram, efb_buffer, dstWd, dstHt);
}

void XFB_Draw(u8 *xfb_in_ram, u32 width, u32 height, s32 yOffset)
{
	OpenGL_Update(); // just updates the render window position and the backbuffer size

    Renderer::SetRenderMode(Renderer::RM_Normal);

    // render to the real buffer now 
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the backbuffer
    glViewport(nXoff, nYoff, nBackbufferWidth, nBackbufferHeight);

	Renderer::ResetGLState();

	ConvertFromXFB((u32 *)xfb_buffer, xfb_in_ram, width, height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfb_texture);	
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, xfb_buffer);
    TextureMngr::EnableTexRECT(0);
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	GL_REPORT_ERRORD();

    glBegin(GL_QUADS);
	glTexCoord2f(width, height + yOffset); glVertex2f( 1,-1);
	glTexCoord2f(width, 0 + yOffset);      glVertex2f( 1, 1);
	glTexCoord2f(0, 0 + yOffset);          glVertex2f(-1, 1);
	glTexCoord2f(0, height + yOffset);     glVertex2f(-1,-1);
    glEnd();

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	Renderer::SwapBuffers();	

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();
}

#endif
