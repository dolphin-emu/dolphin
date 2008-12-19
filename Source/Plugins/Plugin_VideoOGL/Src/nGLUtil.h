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

#ifndef _NGLINIT_H
#define _NGLINIT_H

#include "Config.h"
#include "pluginspecs_video.h"
#include "GLWindow.h"
// backends
#include "SDLWindow.h"
#include "X11Window.h"
#include "WXGLWindow.h"

#ifndef GL_DEPTH24_STENCIL8_EXT // allows FBOs to support stencils
#define GL_DEPTH_STENCIL_EXT 0x84F9
#define GL_UNSIGNED_INT_24_8_EXT 0x84FA
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT 0x88F1
#endif

#define GL_REPORT_ERROR() { err = glGetError(); if( err != GL_NO_ERROR ) { ERROR_LOG("%s:%d: gl error 0x%x\n", __FILE__, (int)__LINE__, err); HandleGLError(); } }

#if defined(_DEBUG) || defined(DEBUGFAST) 
#define GL_REPORT_ERRORD() { GLenum err = glGetError(); if( err != GL_NO_ERROR ) { ERROR_LOG("%s:%d: gl error 0x%x\n", __FILE__, (int)__LINE__, err); HandleGLError(); } }
#else
#define GL_REPORT_ERRORD()
#endif

// TODO old interface removal
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _width, int _height);
bool OpenGL_MakeCurrent();
void OpenGL_SwapBuffers();
void OpenGL_SetWindowText(const char *text);
void OpenGL_Shutdown();
void OpenGL_Update();
u32 OpenGL_GetWidth();
u32 OpenGL_GetHeight();
void OpenGL_SetSize(u32 width, u32 height);
int OpenGL_GetXoff();
int OpenGL_GetYoff();

#endif
