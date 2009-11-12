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

#ifndef __SDL_GF_PIXELFMT_H__
#define __SDL_GF_PIXELFMT_H__

#include "../SDL_sysvideo.h"

#include <gf/gf.h>

gf_format_t qnxgf_sdl_to_gf_pixelformat(uint32_t pixelfmt);
uint32_t qnxgf_gf_to_sdl_pixelformat(gf_format_t pixelfmt);

#endif /* __SDL_GF_PIXELFMT_H__ */

/* vi: set ts=4 sw=4 expandtab: */
