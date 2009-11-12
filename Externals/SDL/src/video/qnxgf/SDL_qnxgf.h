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

    QNX Graphics Framework SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#ifndef __SDL_QNXGF_H__
#define __SDL_QNXGF_H__

#include "../SDL_sysvideo.h"

#include <gf/gf.h>
#include <gf/gf3d.h>

#if defined(SDL_VIDEO_OPENGL_ES)
#include <GLES/egl.h>
#endif /* SDL_VIDEO_OPENGL_ES */

typedef struct SDL_VideoData
{
    gf_dev_t gfdev;             /* GF device handle                     */
    gf_dev_info_t gfdev_info;   /* GF device information                */
    SDL_bool gfinitialized;     /* GF device initialization status      */
#if defined(SDL_VIDEO_OPENGL_ES)
    EGLDisplay egldisplay;      /* OpenGL ES display connection         */
    uint32_t egl_refcount;      /* OpenGL ES reference count            */
    uint32_t swapinterval;      /* OpenGL ES default swap interval      */
#endif                          /* SDL_VIDEO_OPENGL_ES */
} SDL_VideoData;

#define SDL_VIDEO_GF_DEVICENAME_MAX  257
#define SDL_VIDEO_GF_MAX_CURSOR_SIZE 128

typedef struct SDL_DisplayData
{
    gf_display_info_t display_info;     /* GF display information             */
    gf_display_t display;       /* GF display handle                  */
    uint32_t custom_refresh;    /* Custom refresh rate for all modes  */
    SDL_DisplayMode current_mode;       /* Current video mode                 */
    uint8_t description[SDL_VIDEO_GF_DEVICENAME_MAX];
    /* Device description                 */
    uint32_t caps;              /* Device capabilities                */
    SDL_bool layer_attached;    /* Layer attach status                */
    gf_layer_t layer;           /* Graphics layer to which attached   */
    gf_surface_t surface[3];    /* Visible surface on the display     */
    SDL_bool cursor_visible;    /* SDL_TRUE if cursor visible         */
    gf_cursor_t cursor;         /* Cursor shape which was set last    */
} SDL_DisplayData;

/* Maximum amount of OpenGL ES framebuffer configurations */
#define SDL_VIDEO_GF_OPENGLES_CONFS 32

typedef struct SDL_WindowData
{
    SDL_bool uses_gles;         /* true if window support OpenGL ES   */
#if defined(SDL_VIDEO_OPENGL_ES)
    gf_3d_target_t target;      /* OpenGL ES window target            */
    SDL_bool target_created;    /* GF 3D target is created if true    */
    EGLConfig gles_configs[SDL_VIDEO_GF_OPENGLES_CONFS];
    /* OpenGL ES framebuffer confs        */
    EGLint gles_config;         /* Config index in the array of cfgs  */
    EGLContext gles_context;    /* OpenGL ES context                  */
    EGLint gles_attributes[256];        /* OpenGL ES attributes for context   */
    EGLSurface gles_surface;    /* OpenGL ES target rendering surface */
#endif                          /* SDL_VIDEO_OPENGL_ES */
} SDL_WindowData;

typedef struct SDL_GLDriverData
{
#if defined(SDL_VIDEO_OPENGL_ES)
#endif                          /* SDL_VIDEO_OPENGL_ES */
} SDL_GLDriverData;

/****************************************************************************/
/* Low level GF graphics driver capabilities                                */
/****************************************************************************/
typedef struct GF_DeviceCaps
{
    uint8_t *name;
    uint32_t caps;
} GF_DeviceCaps;

#define SDL_GF_UNACCELERATED         0x00000000 /* driver is unaccelerated  */
#define SDL_GF_ACCELERATED           0x00000001 /* driver is accelerated    */
#define SDL_GF_NOLOWRESOLUTION       0x00000000 /* no modes below 640x480   */
#define SDL_GF_LOWRESOLUTION         0x00000002 /* support modes <640x480   */
#define SDL_GF_UNACCELERATED_3D      0x00000000 /* software OpenGL ES       */
#define SDL_GF_ACCELERATED_3D        0x00000004 /* hardware acc. OpenGL ES  */
#define SDL_GF_NOVIDEOMEMORY         0x00000000 /* no video memory alloc.   */
#define SDL_GF_VIDEOMEMORY           0x00000008 /* has video memory alloc.  */

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int qnxgf_videoinit(_THIS);
void qnxgf_videoquit(_THIS);
void qnxgf_getdisplaymodes(_THIS);
int qnxgf_setdisplaymode(_THIS, SDL_DisplayMode * mode);
int qnxgf_setdisplaypalette(_THIS, SDL_Palette * palette);
int qnxgf_getdisplaypalette(_THIS, SDL_Palette * palette);
int qnxgf_setdisplaygammaramp(_THIS, Uint16 * ramp);
int qnxgf_getdisplaygammaramp(_THIS, Uint16 * ramp);
int qnxgf_createwindow(_THIS, SDL_Window * window);
int qnxgf_createwindowfrom(_THIS, SDL_Window * window, const void *data);
void qnxgf_setwindowtitle(_THIS, SDL_Window * window);
void qnxgf_setwindowicon(_THIS, SDL_Window * window, SDL_Surface * icon);
void qnxgf_setwindowposition(_THIS, SDL_Window * window);
void qnxgf_setwindowsize(_THIS, SDL_Window * window);
void qnxgf_showwindow(_THIS, SDL_Window * window);
void qnxgf_hidewindow(_THIS, SDL_Window * window);
void qnxgf_raisewindow(_THIS, SDL_Window * window);
void qnxgf_maximizewindow(_THIS, SDL_Window * window);
void qnxgf_minimizewindow(_THIS, SDL_Window * window);
void qnxgf_restorewindow(_THIS, SDL_Window * window);
void qnxgf_setwindowgrab(_THIS, SDL_Window * window);
void qnxgf_destroywindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool qnxgf_getwindowwminfo(_THIS, SDL_Window * window,
                               struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int qnxgf_gl_loadlibrary(_THIS, const char *path);
void *qnxgf_gl_getprocaddres(_THIS, const char *proc);
void qnxgf_gl_unloadlibrary(_THIS);
SDL_GLContext qnxgf_gl_createcontext(_THIS, SDL_Window * window);
int qnxgf_gl_makecurrent(_THIS, SDL_Window * window, SDL_GLContext context);
int qnxgf_gl_setswapinterval(_THIS, int interval);
int qnxgf_gl_getswapinterval(_THIS);
void qnxgf_gl_swapwindow(_THIS, SDL_Window * window);
void qnxgf_gl_deletecontext(_THIS, SDL_GLContext context);

/* Event handling function */
void qnxgf_pumpevents(_THIS);

/* Screen saver related function */
void qnxgf_suspendscreensaver(_THIS);

#endif /* __SDL_QNXGF_H__ */

/* vi: set ts=4 sw=4 expandtab: */
