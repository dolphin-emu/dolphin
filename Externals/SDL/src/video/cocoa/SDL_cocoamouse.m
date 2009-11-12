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

#include "SDL_cocoavideo.h"

#include "../../events/SDL_mouse_c.h"

void
Cocoa_InitMouse(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_Mouse mouse;

    SDL_zero(mouse);
    data->mouse = SDL_AddMouse(&mouse, "Mouse", 0, 0, 1);
}

void
Cocoa_QuitMouse(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    SDL_DelMouse(data->mouse);
}

/* vi: set ts=4 sw=4 expandtab: */
