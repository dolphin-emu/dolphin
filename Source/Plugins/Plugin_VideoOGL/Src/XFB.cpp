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


// Preliminary non-working code.

#include "Common.h"
#include "Globals.h"
#include "Render.h"
#include "TextureMngr.h"
#include "XFBConvert.h"

enum {
	XFB_WIDTH = 640,
	XFB_HEIGHT = 480,
};

static GLuint xfb_texture;
static u8 *xfb_buffer;

void XFB_Init()
{
	xfb_buffer = new u8[XFB_WIDTH * XFB_HEIGHT * 4];
	memset(xfb_buffer, 0, XFB_WIDTH * XFB_HEIGHT * 4);
    glGenTextures(1, &xfb_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, xfb_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 4, XFB_WIDTH, XFB_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, xfb_buffer);
}

void XFB_Draw(u8 *xfb_in_ram)
{
	Renderer::ResetGLState();
	ConvertXFB((u32 *)xfb_buffer, xfb_in_ram, XFB_WIDTH, XFB_HEIGHT);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, xfb_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 4, XFB_WIDTH, XFB_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, xfb_buffer);
    TextureMngr::EnableTexRECT(0);
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1,-1);
	glTexCoord2f(0, XFB_WIDTH); glVertex2f(-1,1);
    glTexCoord2f(XFB_WIDTH, XFB_HEIGHT); glVertex2f(1,1);
    glTexCoord2f(XFB_WIDTH, 0); glVertex2f(1,-1);
    glEnd();

	Renderer::RestoreGLState();
}

void XFB_Shutdown()
{
	glDeleteTextures(1, &xfb_texture);
	xfb_texture = 0;
	delete [] xfb_buffer;
	xfb_buffer = 0;
}
