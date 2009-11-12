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

#include "SDL_config.h"
#include "SDL_gf_pixelfmt.h"

gf_format_t
qnxgf_sdl_to_gf_pixelformat(uint32_t pixelfmt)
{
    switch (pixelfmt) {
    case SDL_PIXELFORMAT_INDEX8:
        {
            return GF_FORMAT_PAL8;
        }
        break;
    case SDL_PIXELFORMAT_ARGB1555:
        {
            return GF_FORMAT_PACK_ARGB1555;
        }
        break;
    case SDL_PIXELFORMAT_RGB555:
        {
            /* RGB555 is the same as ARGB1555, but alpha is ignored */
            return GF_FORMAT_PACK_ARGB1555;
        }
        break;
    case SDL_PIXELFORMAT_RGB565:
        {
            return GF_FORMAT_PACK_RGB565;
        }
        break;
    case SDL_PIXELFORMAT_BGR565:
        {
            return GF_FORMAT_PKBE_RGB565;
        }
        break;
    case SDL_PIXELFORMAT_RGB24:
        {
            /* GF has wrong components order */
            return GF_FORMAT_BGR888;
        }
        break;
    case SDL_PIXELFORMAT_RGB888:
        {
            /* The same format as ARGB8888, but with alpha ignored */
            /* and GF has wrong components order                   */
            return GF_FORMAT_BGRA8888;
        }
        break;
    case SDL_PIXELFORMAT_ARGB8888:
        {
            /* GF has wrong components order */
            return GF_FORMAT_BGRA8888;
        }
        break;
    case SDL_PIXELFORMAT_BGRA8888:
        {
            /* GF has wrong components order */
            return GF_FORMAT_ARGB8888;
        }
        break;
    case SDL_PIXELFORMAT_YV12:
        {
            return GF_FORMAT_PLANAR_YUV_YV12;
        }
        break;
    case SDL_PIXELFORMAT_YUY2:
        {
            return GF_FORMAT_PACK_YUV_YUY2;
        }
        break;
    case SDL_PIXELFORMAT_UYVY:
        {
            return GF_FORMAT_PACK_YUV_UYVY;
        }
        break;
    case SDL_PIXELFORMAT_YVYU:
        {
            return GF_FORMAT_PACK_YUV_YVYU;
        }
        break;
    }

    return GF_FORMAT_INVALID;
}

uint32_t
qnxgf_gf_to_sdl_pixelformat(gf_format_t pixelfmt)
{
    switch (pixelfmt) {
    case GF_FORMAT_PAL8:
        {
            return SDL_PIXELFORMAT_INDEX8;
        }
        break;
    case GF_FORMAT_PKLE_ARGB1555:
        {
            return SDL_PIXELFORMAT_ARGB1555;
        }
        break;
    case GF_FORMAT_PACK_ARGB1555:
        {
            return SDL_PIXELFORMAT_ARGB1555;
        }
        break;
    case GF_FORMAT_PKBE_RGB565:
        {
            return SDL_PIXELFORMAT_BGR565;
        }
        break;
    case GF_FORMAT_PKLE_RGB565:
        {
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case GF_FORMAT_PACK_RGB565:
        {
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case GF_FORMAT_BGR888:
        {
            /* GF has wrong components order */
            return SDL_PIXELFORMAT_RGB24;
        }
        break;
    case GF_FORMAT_BGRA8888:
        {
            /* GF has wrong components order */
            return SDL_PIXELFORMAT_ARGB8888;
        }
        break;
    case GF_FORMAT_ARGB8888:
        {
            /* GF has wrong components order */
            return SDL_PIXELFORMAT_BGRA8888;
        }
        break;
    case GF_FORMAT_PLANAR_YUV_YV12:
        {
            return SDL_PIXELFORMAT_YV12;
        }
        break;
    case GF_FORMAT_PACK_YUV_YUY2:
        {
            return SDL_PIXELFORMAT_YUY2;
        }
        break;
    case GF_FORMAT_PACK_YUV_UYVY:
        {
            return SDL_PIXELFORMAT_UYVY;
        }
        break;
    case GF_FORMAT_PACK_YUV_YVYU:
        {
            return SDL_PIXELFORMAT_YVYU;
        }
        break;
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

/* vi: set ts=4 sw=4 expandtab: */
