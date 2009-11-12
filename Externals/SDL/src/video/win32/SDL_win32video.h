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

#ifndef _SDL_win32video_h
#define _SDL_win32video_h

#include "../SDL_sysvideo.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define WINVER  0x500           /* Need 0x410 for AlphaBlend() and 0x500 for EnumDisplayDevices() */
#include <windows.h>

#if SDL_VIDEO_RENDER_D3D
//#include <d3d9.h>
#define D3D_DEBUG_INFO
#include "d3d9.h"
#endif

#if SDL_VIDEO_RENDER_DDRAW
/* WIN32_LEAN_AND_MEAN was defined, so we have to include this by hand */
#include <objbase.h>
#include "ddraw.h"
#endif

#include "wactab/wintab.h"
#define PACKETDATA ( PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CURSOR)
#define PACKETMODE 0
#include "wactab/pktdef.h"

#include "SDL_win32events.h"
#include "SDL_win32gamma.h"
#include "SDL_win32keyboard.h"
#include "SDL_win32modes.h"
#include "SDL_win32mouse.h"
#include "SDL_win32opengl.h"
#include "SDL_win32window.h"

#ifdef UNICODE
#define WIN_StringToUTF8(S) SDL_iconv_string("UTF-8", "UCS-2", (char *)S, (SDL_wcslen(S)+1)*sizeof(WCHAR))
#define WIN_UTF8ToString(S) (WCHAR *)SDL_iconv_string("UCS-2", "UTF-8", (char *)S, SDL_strlen(S)+1)
#else
#define WIN_StringToUTF8(S) SDL_iconv_string("UTF-8", "ASCII", (char *)S, (SDL_strlen(S)+1))
#define WIN_UTF8ToString(S) SDL_iconv_string("ASCII", "UTF-8", (char *)S, SDL_strlen(S)+1)
#endif

/* Private display data */

typedef struct SDL_VideoData
{
#if SDL_VIDEO_RENDER_D3D
    HANDLE d3dDLL;
    IDirect3D9 *d3d;
#endif
#if SDL_VIDEO_RENDER_DDRAW
    HANDLE ddrawDLL;
    IDirectDraw *ddraw;
#endif

/* *INDENT-OFF* */
    /* Function pointers for the Wacom API */
    HANDLE wintabDLL;
    UINT (*WTInfoA) (UINT, UINT, LPVOID);
    HCTX (*WTOpenA) (HWND, LPLOGCONTEXTA, BOOL);
    int (*WTPacket) (HCTX, UINT, LPVOID);
    BOOL (*WTClose) (HCTX);
/* *INDENT-ON* */

    int keyboard;
    const SDL_scancode *key_layout;
} SDL_VideoData;

#endif /* _SDL_win32video_h */

/* vi: set ts=4 sw=4 expandtab: */
