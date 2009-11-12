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

#ifndef __SDL_GF_RENDER_H__
#define __SDL_GF_RENDER_H__

#include "../SDL_sysvideo.h"

#include <gf/gf.h>

#define SDL_GF_MAX_SURFACES 3

typedef struct SDL_RenderData
{
    SDL_Window *window;         /* SDL window type                    */
    SDL_bool enable_vsync;      /* VSYNC flip synchronization enable  */
    gf_surface_t surface[SDL_GF_MAX_SURFACES];  /* Surface handles     */
    gf_surface_info_t surface_info[SDL_GF_MAX_SURFACES];        /* Surface info   */
    uint32_t surface_visible_idx;       /* Index of visible surface     */
    uint32_t surface_render_idx;        /* Index of render surface      */
    uint32_t surfaces_count;    /* Amount of allocated surfaces */
} SDL_RenderData;

typedef struct SDL_TextureData
{
    gf_surface_t surface;
    gf_surface_info_t surface_info;
} SDL_TextureData;

extern void gf_addrenderdriver(_THIS);

#endif /* __SDL_GF_RENDER_H__ */

/* vi: set ts=4 sw=4 expandtab: */
