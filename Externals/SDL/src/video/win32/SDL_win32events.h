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

#ifndef _SDL_win32events_h
#define _SDL_win32events_h

extern LPTSTR SDL_Appname;
extern Uint32 SDL_Appstyle;
extern HINSTANCE SDL_Instance;

extern LRESULT CALLBACK WIN_WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam);
extern void WIN_PumpEvents(_THIS);
extern void WIN_SetError(const char *prefix);

#endif /* _SDL_win32events_h */

/* vi: set ts=4 sw=4 expandtab: */
