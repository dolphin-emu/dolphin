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

#ifndef _SDL_directfb_window_h
#define _SDL_directfb_window_h

#include "SDL_DirectFB_WM.h"

#define SDL_DFB_WINDOWDATA(win)  DFB_WindowData *windata = ((win) ? (DFB_WindowData *) ((win)->driverdata) : NULL)

typedef struct _DFB_WindowData DFB_WindowData;
struct _DFB_WindowData
{
    IDirectFBSurface *surface;
    IDirectFBSurface *window_surface;   /* only used with has_own_wm */
    IDirectFBWindow *window;
    DirectFB_GLContext *gl_context;
    IDirectFBEventBuffer *eventbuffer;
    SDL_WindowID sdl_id;
    DFB_WindowData *next;
    Uint8 opacity;
    DFBRectangle client;
    DFBDimension size;
    /* WM extras */
    DFBRectangle restore;
    int is_managed;
    int wm_needs_redraw;
    IDirectFBSurface *icon;
    DFB_Theme theme;
};

extern int DirectFB_CreateWindow(_THIS, SDL_Window * window);
extern int DirectFB_CreateWindowFrom(_THIS, SDL_Window * window,
                                     const void *data);
extern void DirectFB_SetWindowTitle(_THIS, SDL_Window * window);
extern void DirectFB_SetWindowIcon(_THIS, SDL_Window * window,
                                   SDL_Surface * icon);

extern void DirectFB_SetWindowPosition(_THIS, SDL_Window * window);
extern void DirectFB_SetWindowSize(_THIS, SDL_Window * window);
extern void DirectFB_ShowWindow(_THIS, SDL_Window * window);
extern void DirectFB_HideWindow(_THIS, SDL_Window * window);
extern void DirectFB_RaiseWindow(_THIS, SDL_Window * window);
extern void DirectFB_MaximizeWindow(_THIS, SDL_Window * window);
extern void DirectFB_MinimizeWindow(_THIS, SDL_Window * window);
extern void DirectFB_RestoreWindow(_THIS, SDL_Window * window);
extern void DirectFB_SetWindowGrab(_THIS, SDL_Window * window);
extern void DirectFB_DestroyWindow(_THIS, SDL_Window * window);
extern SDL_bool DirectFB_GetWindowWMInfo(_THIS, SDL_Window * window,
                                         struct SDL_SysWMinfo *info);

extern void DirectFB_AdjustWindowSurface(SDL_Window * window);

#endif /* _SDL_directfb_window_h */

/* vi: set ts=4 sw=4 expandtab: */
