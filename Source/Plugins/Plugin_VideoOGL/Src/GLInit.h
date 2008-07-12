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

#ifndef _GLINIT_H
#define _GLINIT_H

#ifndef _WIN32

#undef I_NEED_OS2_H
#undef BOOL

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86vmode.h>
//#include <gtk/gtk.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    Display *dpy;
    int screen;
    Window win;
    GLXContext ctx;
    XSetWindowAttributes attr;
    Bool fs;
    Bool doubleBuffered;
    XF86VidModeModeInfo deskMode;
    int x, y;
    unsigned int width, height;
    unsigned int depth;    
} GLWindow;

extern GLWindow GLWin;

#endif

// yeah yeah, these should be hidden
extern int nBackbufferWidth, nBackbufferHeight;
extern u32 s_nTargetWidth, s_nTargetHeight;
extern u32 g_AAx, g_AAy;

bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _width, int _height);
bool OpenGL_MakeCurrent();
void OpenGL_SwapBuffers();
void OpenGL_SetWindowText(const char *text);
void OpenGL_Shutdown();
void OpenGL_Update();



#endif
