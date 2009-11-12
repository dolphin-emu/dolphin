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

#include "SDL_photon_pixelfmt.h"

uint32_t
photon_bits_to_sdl_pixelformat(uint32_t pixelfmt)
{
    switch (pixelfmt) {
    case 8:
        {
            return SDL_PIXELFORMAT_INDEX8;
        }
        break;
    case 15:
        {
            return SDL_PIXELFORMAT_ARGB1555;
        }
        break;
    case 16:
        {
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case 24:
        {
            return SDL_PIXELFORMAT_RGB888;
        }
        break;
    case 32:
        {
            return SDL_PIXELFORMAT_ARGB8888;
        }
        break;
    }
}

uint32_t
photon_sdl_to_bits_pixelformat(uint32_t pixelfmt)
{
    switch (pixelfmt) {
    case SDL_PIXELFORMAT_INDEX8:
        {
            return 8;
        }
        break;
    case SDL_PIXELFORMAT_ARGB1555:
        {
            return 15;
        }
        break;
    case SDL_PIXELFORMAT_RGB555:
        {
            return 15;
        }
        break;
    case SDL_PIXELFORMAT_ABGR1555:
        {
            return 15;
        }
        break;
    case SDL_PIXELFORMAT_RGB565:
        {
            return 16;
        }
        break;
    case SDL_PIXELFORMAT_RGB24:
        {
            return 24;
        }
        break;
    case SDL_PIXELFORMAT_RGB888:
        {
            return 32;
        }
        break;
    case SDL_PIXELFORMAT_BGRA8888:
        {
            return 32;
        }
        break;
    case SDL_PIXELFORMAT_ARGB8888:
        {
            return 32;
        }
        break;
    case SDL_PIXELFORMAT_YV12:
        {
            return 8;
        }
        break;
    case SDL_PIXELFORMAT_YUY2:
        {
            return 16;
        }
        break;
    case SDL_PIXELFORMAT_UYVY:
        {
            return 16;
        }
        break;
    case SDL_PIXELFORMAT_YVYU:
        {
            return 16;
        }
        break;
    }

    return 0;
}

uint32_t
photon_image_to_sdl_pixelformat(uint32_t pixelfmt)
{
    switch (pixelfmt) {
    case Pg_IMAGE_PALETTE_BYTE:
        {
            return SDL_PIXELFORMAT_INDEX8;
        }
        break;
    case Pg_IMAGE_DIRECT_8888:
        {
            return SDL_PIXELFORMAT_ARGB8888;
        }
        break;
    case Pg_IMAGE_DIRECT_888:
        {
            return SDL_PIXELFORMAT_RGB24;
        }
        break;
    case Pg_IMAGE_DIRECT_565:
        {
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case Pg_IMAGE_DIRECT_555:
        {
            return SDL_PIXELFORMAT_RGB555;
        }
        break;
    case Pg_IMAGE_DIRECT_1555:
        {
            return SDL_PIXELFORMAT_ARGB1555;
        }
        break;
    }

    return 0;
}

uint32_t
photon_sdl_to_image_pixelformat(uint32_t pixelfmt)
{
    switch (pixelfmt) {
    case SDL_PIXELFORMAT_INDEX8:
        {
            return Pg_IMAGE_PALETTE_BYTE;
        }
        break;
    case SDL_PIXELFORMAT_ARGB8888:
        {
            return Pg_IMAGE_DIRECT_8888;
        }
        break;
    case SDL_PIXELFORMAT_RGB888:
        {
            return Pg_IMAGE_DIRECT_8888;
        }
        break;
    case SDL_PIXELFORMAT_RGB24:
        {
            return Pg_IMAGE_DIRECT_888;
        }
        break;
    case SDL_PIXELFORMAT_RGB565:
        {
            return Pg_IMAGE_DIRECT_565;
        }
        break;
    case SDL_PIXELFORMAT_ARGB1555:
        {
            return Pg_IMAGE_DIRECT_1555;
        }
        break;
    case SDL_PIXELFORMAT_RGB555:
        {
            return Pg_IMAGE_DIRECT_555;
        }
        break;
    }

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
