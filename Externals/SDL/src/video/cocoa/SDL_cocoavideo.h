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

#ifndef _SDL_cocoavideo_h
#define _SDL_cocoavideo_h

#include "SDL_opengl.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>

#include "SDL_keysym.h"
#include "../SDL_sysvideo.h"

#include "SDL_cocoaevents.h"
#include "SDL_cocoakeyboard.h"
#include "SDL_cocoamodes.h"
#include "SDL_cocoamouse.h"
#include "SDL_cocoaopengl.h"
#include "SDL_cocoawindow.h"

/* Private display data */

@class SDLTranslatorResponder;

typedef struct SDL_VideoData
{
    SInt32 osversion;
    unsigned int modifierFlags;
    int mouse;
    int keyboard;
    void *key_layout;
    SDLTranslatorResponder *fieldEdit;
    Uint32 screensaver_activity;
} SDL_VideoData;

#endif /* _SDL_cocoavideo_h */

/* vi: set ts=4 sw=4 expandtab: */
