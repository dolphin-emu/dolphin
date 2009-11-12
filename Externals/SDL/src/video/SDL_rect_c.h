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

typedef struct SDL_DirtyRect
{
    SDL_Rect rect;
    struct SDL_DirtyRect *next;
} SDL_DirtyRect;

typedef struct SDL_DirtyRectList
{
    SDL_DirtyRect *list;
    SDL_DirtyRect *free;
} SDL_DirtyRectList;

extern void SDL_AddDirtyRect(SDL_DirtyRectList * list, const SDL_Rect * rect);
extern void SDL_ClearDirtyRects(SDL_DirtyRectList * list);
extern void SDL_FreeDirtyRects(SDL_DirtyRectList * list);

/* vi: set ts=4 sw=4 expandtab: */
