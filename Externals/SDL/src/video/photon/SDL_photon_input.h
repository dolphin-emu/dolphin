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

#ifndef __SDL_PHOTON_INPUT_H__
#define __SDL_PHOTON_INPUT_H__

#include "SDL_config.h"
#include "SDL_video.h"
#include "../SDL_sysvideo.h"

#include "SDL_photon.h"

typedef struct SDL_MouseData
{
    SDL_DisplayData *didata;
} SDL_MouseData;

int32_t photon_addinputdevices(_THIS);
int32_t photon_delinputdevices(_THIS);

#define SDL_PHOTON_MOUSE_COLOR_BLACK 0xFF000000
#define SDL_PHOTON_MOUSE_COLOR_WHITE 0xFFFFFFFF
#define SDL_PHOTON_MOUSE_COLOR_TRANS 0x00000000

SDL_scancode photon_to_sdl_keymap(uint32_t key);

#endif /* __SDL_GF_INPUT_H__ */
