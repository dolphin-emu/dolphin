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

/* This file contains functions for backwards compatibility with SDL 1.2 */

#ifndef _SDL_compat_h
#define _SDL_compat_h

#include "SDL_video.h"
#include "SDL_version.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SDL_SWSURFACE       0x00000000  /* Not used */
#define SDL_SRCALPHA        0x00010000
#define SDL_SRCCOLORKEY     0x00020000
#define SDL_ANYFORMAT       0x00100000
#define SDL_HWPALETTE       0x00200000
#define SDL_DOUBLEBUF       0x00400000
#define SDL_FULLSCREEN      0x00800000
#define SDL_RESIZABLE       0x01000000
#define SDL_NOFRAME         0x02000000
#define SDL_OPENGL          0x04000000
#define SDL_HWSURFACE       0x08000001  /* Not used */
#define SDL_ASYNCBLIT       0x08000000  /* Not used */
#define SDL_RLEACCELOK      0x08000000  /* Not used */
#define SDL_HWACCEL         0x08000000  /* Not used */

#define SDL_APPMOUSEFOCUS	0x01
#define SDL_APPINPUTFOCUS	0x02
#define SDL_APPACTIVE		0x04

#define SDL_LOGPAL 0x01
#define SDL_PHYSPAL 0x02

#define SDL_ACTIVEEVENT	SDL_EVENT_RESERVED1
#define SDL_VIDEORESIZE	SDL_EVENT_RESERVED2
#define SDL_VIDEOEXPOSE	SDL_EVENT_RESERVED3
#define SDL_ACTIVEEVENTMASK	SDL_EVENTMASK(SDL_ACTIVEEVENT)
#define SDL_VIDEORESIZEMASK SDL_EVENTMASK(SDL_VIDEORESIZE)
#define SDL_VIDEOEXPOSEMASK SDL_EVENTMASK(SDL_VIDEOEXPOSE)

#define SDL_BUTTON_WHEELUP	4
#define SDL_BUTTON_WHEELDOWN	5

#define SDL_DEFAULT_REPEAT_DELAY	500
#define SDL_DEFAULT_REPEAT_INTERVAL	30

typedef struct SDL_VideoInfo
{
    Uint32 hw_available:1;
    Uint32 wm_available:1;
    Uint32 UnusedBits1:6;
    Uint32 UnusedBits2:1;
    Uint32 blit_hw:1;
    Uint32 blit_hw_CC:1;
    Uint32 blit_hw_A:1;
    Uint32 blit_sw:1;
    Uint32 blit_sw_CC:1;
    Uint32 blit_sw_A:1;
    Uint32 blit_fill:1;
    Uint32 UnusedBits3:16;
    Uint32 video_mem;

    SDL_PixelFormat *vfmt;

    int current_w;
    int current_h;
} SDL_VideoInfo;

/* The most common video overlay formats.
   For an explanation of these pixel formats, see:
   http://www.webartz.com/fourcc/indexyuv.htm

   For information on the relationship between color spaces, see:
   http://www.neuro.sfc.keio.ac.jp/~aly/polygon/info/color-space-faq.html
 */
#define SDL_YV12_OVERLAY  0x32315659    /* Planar mode: Y + V + U  (3 planes) */
#define SDL_IYUV_OVERLAY  0x56555949    /* Planar mode: Y + U + V  (3 planes) */
#define SDL_YUY2_OVERLAY  0x32595559    /* Packed mode: Y0+U0+Y1+V0 (1 plane) */
#define SDL_UYVY_OVERLAY  0x59565955    /* Packed mode: U0+Y0+V0+Y1 (1 plane) */
#define SDL_YVYU_OVERLAY  0x55595659    /* Packed mode: Y0+V0+Y1+U0 (1 plane) */

/* The YUV hardware video overlay */
typedef struct SDL_Overlay
{
    Uint32 format;              /* Read-only */
    int w, h;                   /* Read-only */
    int planes;                 /* Read-only */
    Uint16 *pitches;            /* Read-only */
    Uint8 **pixels;             /* Read-write */

    /* Hardware-specific surface info */
    struct private_yuvhwfuncs *hwfuncs;
    struct private_yuvhwdata *hwdata;

    /* Special flags */
    Uint32 hw_overlay:1;        /* Flag: This overlay hardware accelerated? */
    Uint32 UnusedBits:31;
} SDL_Overlay;

typedef enum
{
    SDL_GRAB_QUERY = -1,
    SDL_GRAB_OFF = 0,
    SDL_GRAB_ON = 1
} SDL_GrabMode;

struct SDL_SysWMinfo;

/* Obsolete or renamed key codes */

/* These key constants were renamed for clarity or consistency. */
#define SDLK_0	'0'
#define SDLK_1	'1'
#define SDLK_2	'2'
#define SDLK_3	'3'
#define SDLK_4	'4'
#define SDLK_5	'5'
#define SDLK_6	'6'
#define SDLK_7	'7'
#define SDLK_8	'8'
#define SDLK_9	'9'
#define SDLK_a  'a'
#define SDLK_b  'b'
#define SDLK_c  'c'
#define SDLK_d  'd'
#define SDLK_e  'e'
#define SDLK_f  'f'
#define SDLK_g  'g'
#define SDLK_h  'h'
#define SDLK_i  'i'
#define SDLK_j  'j'
#define SDLK_k  'k'
#define SDLK_l  'l'
#define SDLK_m  'm'
#define SDLK_n  'n'
#define SDLK_o  'o'
#define SDLK_p  'p'
#define SDLK_q  'q'
#define SDLK_r  'r'
#define SDLK_s  's'
#define SDLK_t  't'
#define SDLK_u  'u'
#define SDLK_v  'v'
#define SDLK_w  'w'
#define SDLK_x  'x'
#define SDLK_y  'y'
#define SDLK_z  'z'
#define SDLK_QUOTE      '\''
#define SDLK_MINUS      '-'
#define SDLK_BACKQUOTE  '`'
#define SDLK_EXCLAIM    '!'
#define SDLK_QUOTEDBL   '"'
#define SDLK_HASH       '#'
#define SDLK_DOLLAR     '$'
#define SDLK_AMPERSAND  '&'
#define SDLK_LEFTPAREN  '('
#define SDLK_RIGHTPAREN ')'
#define SDLK_ASTERISK   '*'
#define SDLK_PLUS       '+'
#define SDLK_COLON      ':'
#define SDLK_LESS       '<'
#define SDLK_GREATER    '>'
#define SDLK_QUESTION   '?'
#define SDLK_AT         '@'
#define SDLK_CARET      '^'
#define SDLK_UNDERSCORE '_'
#define SDLK_KP0 SDLK_KP_0
#define SDLK_KP1 SDLK_KP_1
#define SDLK_KP2 SDLK_KP_2
#define SDLK_KP3 SDLK_KP_3
#define SDLK_KP4 SDLK_KP_4
#define SDLK_KP5 SDLK_KP_5
#define SDLK_KP6 SDLK_KP_6
#define SDLK_KP7 SDLK_KP_7
#define SDLK_KP8 SDLK_KP_8
#define SDLK_KP9 SDLK_KP_9
#define SDLK_NUMLOCK SDLK_NUMLOCKCLEAR
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#define SDLK_PRINT SDLK_PRINTSCREEN

/* The META modifier is equivalent to the GUI modifier from the USB standard */
#define KMOD_LMETA KMOD_LGUI
#define KMOD_RMETA KMOD_RGUI
#define KMOD_META KMOD_GUI

/* These keys don't appear in the USB specification (or at least not under those names). I'm unsure if the following assignments make sense or if these codes should be defined as actual additional SDLK_ constants. */
#define SDLK_LSUPER SDLK_LMETA
#define SDLK_RSUPER SDLK_RMETA
#define SDLK_COMPOSE SDLK_APPLICATION
#define SDLK_BREAK SDLK_STOP
#define SDLK_EURO SDLK_2


#define SDL_SetModuleHandle(x)
#define SDL_AllocSurface    SDL_CreateRGBSurface

extern DECLSPEC const SDL_version *SDLCALL SDL_Linked_Version(void);
extern DECLSPEC char *SDLCALL SDL_AudioDriverName(char *namebuf, int maxlen);
extern DECLSPEC char *SDLCALL SDL_VideoDriverName(char *namebuf, int maxlen);
extern DECLSPEC const SDL_VideoInfo *SDLCALL SDL_GetVideoInfo(void);
extern DECLSPEC int SDLCALL SDL_VideoModeOK(int width,
                                            int height,
                                            int bpp, Uint32 flags);
extern DECLSPEC SDL_Rect **SDLCALL SDL_ListModes(const SDL_PixelFormat *
                                                 format, Uint32 flags);
extern DECLSPEC SDL_Surface *SDLCALL SDL_SetVideoMode(int width, int height,
                                                      int bpp, Uint32 flags);
extern DECLSPEC SDL_Surface *SDLCALL SDL_GetVideoSurface(void);
extern DECLSPEC void SDLCALL SDL_UpdateRects(SDL_Surface * screen,
                                             int numrects, SDL_Rect * rects);
extern DECLSPEC void SDLCALL SDL_UpdateRect(SDL_Surface * screen,
                                            Sint32 x,
                                            Sint32 y, Uint32 w, Uint32 h);
extern DECLSPEC int SDLCALL SDL_Flip(SDL_Surface * screen);
extern DECLSPEC int SDLCALL SDL_SetAlpha(SDL_Surface * surface,
                                         Uint32 flag, Uint8 alpha);
extern DECLSPEC SDL_Surface *SDLCALL SDL_DisplayFormat(SDL_Surface * surface);
extern DECLSPEC SDL_Surface *SDLCALL SDL_DisplayFormatAlpha(SDL_Surface *
                                                            surface);
extern DECLSPEC void SDLCALL SDL_WM_SetCaption(const char *title,
                                               const char *icon);
extern DECLSPEC void SDLCALL SDL_WM_GetCaption(const char **title,
                                               const char **icon);
extern DECLSPEC void SDLCALL SDL_WM_SetIcon(SDL_Surface * icon, Uint8 * mask);
extern DECLSPEC int SDLCALL SDL_WM_IconifyWindow(void);
extern DECLSPEC int SDLCALL SDL_WM_ToggleFullScreen(SDL_Surface * surface);
extern DECLSPEC SDL_GrabMode SDLCALL SDL_WM_GrabInput(SDL_GrabMode mode);
extern DECLSPEC int SDLCALL SDL_SetPalette(SDL_Surface * surface,
                                           int flags,
                                           const SDL_Color * colors,
                                           int firstcolor, int ncolors);
extern DECLSPEC int SDLCALL SDL_SetColors(SDL_Surface * surface,
                                          const SDL_Color * colors,
                                          int firstcolor, int ncolors);
extern DECLSPEC int SDLCALL SDL_GetWMInfo(struct SDL_SysWMinfo *info);
extern DECLSPEC Uint8 SDLCALL SDL_GetAppState(void);
extern DECLSPEC void SDLCALL SDL_WarpMouse(Uint16 x, Uint16 y);
extern DECLSPEC SDL_Overlay *SDLCALL SDL_CreateYUVOverlay(int width,
                                                          int height,
                                                          Uint32 format,
                                                          SDL_Surface *
                                                          display);
extern DECLSPEC int SDLCALL SDL_LockYUVOverlay(SDL_Overlay * overlay);
extern DECLSPEC void SDLCALL SDL_UnlockYUVOverlay(SDL_Overlay * overlay);
extern DECLSPEC int SDLCALL SDL_DisplayYUVOverlay(SDL_Overlay * overlay,
                                                  SDL_Rect * dstrect);
extern DECLSPEC void SDLCALL SDL_FreeYUVOverlay(SDL_Overlay * overlay);
extern DECLSPEC void SDLCALL SDL_GL_SwapBuffers(void);
extern DECLSPEC int SDLCALL SDL_EnableKeyRepeat(int delay, int interval);
extern DECLSPEC void SDLCALL SDL_GetKeyRepeat(int *delay, int *interval);
extern DECLSPEC int SDLCALL SDL_EnableUNICODE(int enable);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_compat_h */

/* vi: set ts=4 sw=4 expandtab: */
