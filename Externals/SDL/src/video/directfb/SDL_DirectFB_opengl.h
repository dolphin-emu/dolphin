/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#ifndef _SDL_directfb_opengl_h
#define _SDL_directfb_opengl_h

#if SDL_DIRECTFB_OPENGL
#include "SDL_opengl.h"

typedef struct _DirectFB_GLContext DirectFB_GLContext;
struct _DirectFB_GLContext
{
    IDirectFBGL *context;
    DirectFB_GLContext *next;
};

/* OpenGL functions */
extern int DirectFB_GL_Initialize(_THIS);
extern void DirectFB_GL_Shutdown(_THIS);

extern int DirectFB_GL_LoadLibrary(_THIS, const char *path);
extern void *DirectFB_GL_GetProcAddress(_THIS, const char *proc);
extern SDL_GLContext DirectFB_GL_CreateContext(_THIS, SDL_Window * window);
extern int DirectFB_GL_MakeCurrent(_THIS, SDL_Window * window,
                                   SDL_GLContext context);
extern int DirectFB_GL_SetSwapInterval(_THIS, int interval);
extern int DirectFB_GL_GetSwapInterval(_THIS);
extern void DirectFB_GL_SwapWindow(_THIS, SDL_Window * window);
extern void DirectFB_GL_DeleteContext(_THIS, SDL_GLContext context);

#endif /* SDL_DIRECTFB_OPENGL */

#endif /* _SDL_directfb_opengl_h */

/* vi: set ts=4 sw=4 expandtab: */
