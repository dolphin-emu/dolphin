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

#ifndef __SDL_PHOTON_RENDER_H__
#define __SDL_PHOTON_RENDER_H__

#include "../SDL_sysvideo.h"

#include <Ph.h>
#include <photon/PhRender.h>

#define SDL_PHOTON_MAX_SURFACES 3

#define SDL_PHOTON_SURFTYPE_UNKNOWN    0x00000000
#define SDL_PHOTON_SURFTYPE_OFFSCREEN  0x00000001
#define SDL_PHOTON_SURFTYPE_PHIMAGE    0x00000002

#define SDL_PHOTON_UNKNOWN_BLEND       0x00000000
#define SDL_PHOTON_DRAW_BLEND          0x00000001
#define SDL_PHOTON_TEXTURE_BLEND       0x00000002

typedef struct SDL_RenderData
{
    SDL_bool enable_vsync;              /* VSYNC flip synchronization enable  */
    uint32_t surface_visible_idx;       /* Index of visible surface           */
    uint32_t surface_render_idx;        /* Index of render surface            */
    uint32_t surfaces_count;            /* Amount of allocated surfaces       */
    uint32_t surfaces_type;             /* Type of allocated surfaces         */
    uint32_t window_width;              /* Last active window width           */
    uint32_t window_height;             /* Last active window height          */
    PhGC_t* gc;                         /* Graphics context                   */
    SDL_bool direct_mode;               /* Direct Mode state                  */
    PdOffscreenContext_t* osurfaces[SDL_PHOTON_MAX_SURFACES];
    PhImage_t* psurfaces[SDL_PHOTON_MAX_SURFACES];
    PmMemoryContext_t* pcontexts[SDL_PHOTON_MAX_SURFACES];
} SDL_RenderData;

typedef struct SDL_TextureData
{
   uint32_t surface_type;
   PdOffscreenContext_t* osurface;
   PhImage_t* psurface;
   PmMemoryContext_t* pcontext;
} SDL_TextureData;

extern void photon_addrenderdriver(_THIS);

/* Helper function, which redraws the backbuffer */
int _photon_update_rectangles(SDL_Renderer* renderer, PhRect_t* rect);

#endif /* __SDL_PHOTON_RENDER_H__ */

/* vi: set ts=4 sw=4 expandtab: */
