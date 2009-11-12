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

    QNX Photon GUI SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#ifndef __SDL_PHOTON_H__
#define __SDL_PHOTON_H__

/* GF headers must be included first for the Photon GF functions */
#if defined(SDL_VIDEO_OPENGL_ES)
#include <gf/gf.h>
#include <GLES/egl.h>
#endif /* SDL_VIDEO_OPENGL_ES */

#include "SDL_config.h"
#include "../SDL_sysvideo.h"

#include <Ph.h>
#include <Pt.h>
#include <photon/PkKeyDef.h>

/* Maximum display devices, which can handle SDL Photon driver */
#define SDL_VIDEO_PHOTON_MAX_RIDS 16

typedef struct SDL_VideoData
{
    PhRid_t rid[SDL_VIDEO_PHOTON_MAX_RIDS];
    uint32_t avail_rids;
    uint32_t current_device_id;
#if defined(SDL_VIDEO_OPENGL_ES)
    gf_dev_t gfdev;             /* GF device handle                     */
    gf_dev_info_t gfdev_info;   /* GF device information                */
    SDL_bool gfinitialized;     /* GF device initialization status      */
    EGLDisplay egldisplay;      /* OpenGL ES display connection         */
    uint32_t egl_refcount;      /* OpenGL ES reference count            */
    uint32_t swapinterval;      /* OpenGL ES default swap interval      */
    EGLContext lgles_context;   /* Last used OpenGL ES context          */
    EGLSurface lgles_surface;   /* Last used OpenGL ES target surface   */
#endif /* SDL_VIDEO_OPENGL_ES */
} SDL_VideoData;

/* This is hardcoded value in photon/Pg.h */
#define SDL_VIDEO_PHOTON_DEVICENAME_MAX  41
#define SDL_VIDEO_PHOTON_MAX_CURSOR_SIZE 128

/* Maximum event message size with data payload */
#define SDL_VIDEO_PHOTON_EVENT_SIZE 8192

/* Current video mode graphics capabilities */
#define SDL_VIDEO_PHOTON_CAP_ALPHA_BLEND 0x00000001
#define SDL_VIDEO_PHOTON_CAP_SCALED_BLIT 0x00000002

typedef struct SDL_DisplayData
{
    uint32_t device_id;
    uint32_t custom_refresh;            /* Custom refresh rate for all modes  */
    SDL_DisplayMode current_mode;       /* Current video mode                 */
    uint8_t description[SDL_VIDEO_PHOTON_DEVICENAME_MAX];
    /* Device description */
    uint32_t caps;                      /* Device capabilities                */
    PhCursorDef_t *cursor;              /* Global cursor settings             */
    SDL_bool cursor_visible;            /* SDL_TRUE if cursor visible         */
    uint32_t cursor_size;               /* Cursor size in memory w/ structure */
    uint32_t mode_2dcaps;               /* Current video mode 2D capabilities */
    SDL_bool direct_mode;               /* Direct mode state                  */
#if defined(SDL_VIDEO_OPENGL_ES)
    gf_display_t display;               /* GF display handle                  */
    gf_display_info_t display_info;     /* GF display information             */
#endif /* SDL_VIDEO_OPENGL_ES */
} SDL_DisplayData;

/* Maximum amount of OpenGL ES framebuffer configurations */
#define SDL_VIDEO_GF_OPENGLES_CONFS 32

typedef struct SDL_WindowData
{
    SDL_bool uses_gles;         /* if true window must support OpenGL ES */
    PtWidget_t *window;         /* window handle                         */
#if defined(SDL_VIDEO_OPENGL_ES)
    EGLConfig gles_configs[SDL_VIDEO_GF_OPENGLES_CONFS];
    /* OpenGL ES framebuffer confs */
    EGLint gles_config;                 /* OpenGL ES configuration index      */
    EGLContext gles_context;            /* OpenGL ES context                  */
    EGLint gles_attributes[256];        /* OpenGL ES attributes for context   */
    EGLSurface gles_surface;            /* OpenGL ES target rendering surface */
    gf_surface_t gfsurface;             /* OpenGL ES GF's surface             */
    PdOffscreenContext_t *phsurface;    /* OpenGL ES Photon's surface         */
#endif /* SDL_VIDEO_OPENGL_ES */
} SDL_WindowData;

/****************************************************************************/
/* Low level Photon graphics driver capabilities                            */
/****************************************************************************/
typedef struct Photon_DeviceCaps
{
    uint8_t *name;
    uint32_t caps;
} Photon_DeviceCaps;

#define SDL_PHOTON_UNACCELERATED         0x00000000
#define SDL_PHOTON_ACCELERATED           0x00000001
#define SDL_PHOTON_UNACCELERATED_3D      0x00000000
#define SDL_PHOTON_ACCELERATED_3D        0x00000004

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int photon_videoinit(_THIS);
void photon_videoquit(_THIS);
void photon_getdisplaymodes(_THIS);
int photon_setdisplaymode(_THIS, SDL_DisplayMode * mode);
int photon_setdisplaypalette(_THIS, SDL_Palette * palette);
int photon_getdisplaypalette(_THIS, SDL_Palette * palette);
int photon_setdisplaygammaramp(_THIS, Uint16 * ramp);
int photon_getdisplaygammaramp(_THIS, Uint16 * ramp);
int photon_createwindow(_THIS, SDL_Window * window);
int photon_createwindowfrom(_THIS, SDL_Window * window, const void *data);
void photon_setwindowtitle(_THIS, SDL_Window * window);
void photon_setwindowicon(_THIS, SDL_Window * window, SDL_Surface * icon);
void photon_setwindowposition(_THIS, SDL_Window * window);
void photon_setwindowsize(_THIS, SDL_Window * window);
void photon_showwindow(_THIS, SDL_Window * window);
void photon_hidewindow(_THIS, SDL_Window * window);
void photon_raisewindow(_THIS, SDL_Window * window);
void photon_maximizewindow(_THIS, SDL_Window * window);
void photon_minimizewindow(_THIS, SDL_Window * window);
void photon_restorewindow(_THIS, SDL_Window * window);
void photon_setwindowgrab(_THIS, SDL_Window * window);
void photon_destroywindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool photon_getwindowwminfo(_THIS, SDL_Window * window,
                                struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int photon_gl_loadlibrary(_THIS, const char *path);
void *photon_gl_getprocaddres(_THIS, const char *proc);
void photon_gl_unloadlibrary(_THIS);
SDL_GLContext photon_gl_createcontext(_THIS, SDL_Window * window);
int photon_gl_makecurrent(_THIS, SDL_Window * window, SDL_GLContext context);
int photon_gl_setswapinterval(_THIS, int interval);
int photon_gl_getswapinterval(_THIS);
void photon_gl_swapwindow(_THIS, SDL_Window * window);
void photon_gl_deletecontext(_THIS, SDL_GLContext context);

/* Helper function, which re-creates surface, not an API */
int photon_gl_recreatesurface(_THIS, SDL_Window * window, uint32_t width, uint32_t height);

/* Event handling function */
void photon_pumpevents(_THIS);

/* Screen saver related function */
void photon_suspendscreensaver(_THIS);

#endif /* __SDL_PHOTON_H__ */

/* vi: set ts=4 sw=4 expandtab: */
