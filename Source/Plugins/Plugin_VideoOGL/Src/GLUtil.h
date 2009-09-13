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

#ifndef _GLINIT_H_
#define _GLINIT_H_

#include "VideoConfig.h"
#include "MathUtil.h"
#include "pluginspecs_video.h"

#ifdef _WIN32

#define GLEW_STATIC

#include <GLew/glew.h>
#include <GLew/wglew.h>
#include <GLew/gl.h>
#include <GLew/glext.h>

#else // linux basic definitions

#if defined(USE_WX) && USE_WX
#include <GL/glew.h>
#include "wx/wx.h"
#include "wx/glcanvas.h"
#elif defined(HAVE_X11) && HAVE_X11
#define I_NEED_OS2_H // HAXXOR
#include <GL/glxew.h>
#include <X11/XKBlib.h>
#elif defined(USE_SDL) && USE_SDL
#include <GL/glew.h>
#include <SDL.h>
#elif defined(HAVE_COCOA) && HAVE_COCOA
#include <GL/glew.h>
#include "cocoaGL.h"
#endif // end USE_WX

#if defined(__APPLE__) 
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#endif // linux basic definitions

#ifndef GL_DEPTH24_STENCIL8_EXT // allows FBOs to support stencils
#define GL_DEPTH_STENCIL_EXT 0x84F9
#define GL_UNSIGNED_INT_24_8_EXT 0x84FA
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT 0x88F1
#endif

#ifndef _WIN32
#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#if defined(HAVE_XXF86VM) && HAVE_XXF86VM
#include <X11/extensions/xf86vmode.h>
#endif // XXF86VM
#endif // X11

#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    int screen;
#if defined(HAVE_COCOA) && HAVE_COCOA
    NSWindow *cocoaWin;
    NSOpenGLContext *cocoaCtx;
#elif defined(HAVE_X11) && HAVE_X11
    Window win;
    Display *dpy;
    GLXContext ctx;
    XSetWindowAttributes attr;
    Bool fs;
    Bool doubleBuffered;
#if defined(HAVE_XXF86VM) && HAVE_XXF86VM
    XF86VidModeModeInfo deskMode;
#endif // XXF86VM
#endif // X11
#if defined(USE_WX) && USE_WX
    wxGLCanvas *glCanvas;
    wxFrame *frame;
    wxGLContext *glCtxt;
#endif 
    int x, y;
    unsigned int width, height;
    unsigned int depth;    
} GLWindow;

extern GLWindow GLWin;

#endif

// Public OpenGL util

// Initialization / upkeep
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _width, int _height);
void OpenGL_Shutdown();
void OpenGL_Update();
bool OpenGL_MakeCurrent();
void OpenGL_SwapBuffers();

// Get status
u32 OpenGL_GetBackbufferWidth();
u32 OpenGL_GetBackbufferHeight();

// Set things
void OpenGL_SetWindowText(const char *text);

// Error reporting - use the convenient macros.
void OpenGL_ReportARBProgramError();
GLuint OpenGL_ReportGLError(const char *function, const char *file, int line);
bool OpenGL_ReportFBOError(const char *function, const char *file, int line);

#if 1
#define GL_REPORT_ERROR()         OpenGL_ReportGLError        (__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_PROGRAM_ERROR() OpenGL_ReportARBProgramError()
#define GL_REPORT_FBO_ERROR()     OpenGL_ReportFBOError       (__FUNCTION__, __FILE__, __LINE__)
#else
#define GL_REPORT_ERROR() GL_NO_ERROR
#define GL_REPORT_PROGRAM_ERROR()
#define GL_REPORT_FBO_ERROR()
#endif

#if defined(_DEBUG) || defined(DEBUGFAST) 
#define GL_REPORT_ERRORD() OpenGL_ReportGLError(__FUNCTION__, __FILE__, __LINE__)
#else
#define GL_REPORT_ERRORD()
#endif

#include <Cg/cg.h>
#include <Cg/cgGL.h>

extern CGcontext g_cgcontext;
extern CGprofile g_cgvProf, g_cgfProf;

#endif  // _GLINIT_H_
