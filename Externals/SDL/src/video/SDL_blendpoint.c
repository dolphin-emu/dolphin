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

#include "SDL_draw.h"

static int
SDL_BlendPoint_RGB555(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
                      Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB555(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB555(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB555(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB555(x, y);
        break;
    }
    return 0;
}

static int
SDL_BlendPoint_RGB565(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
                      Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB565(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB565(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB565(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB565(x, y);
        break;
    }
    return 0;
}

static int
SDL_BlendPoint_RGB888(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
                      Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB888(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB888(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB888(x, y);
        break;
    }
    return 0;
}

static int
SDL_BlendPoint_ARGB8888(SDL_Surface * dst, int x, int y, int blendMode,
                        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_ARGB8888(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_ARGB8888(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_ARGB8888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_ARGB8888(x, y);
        break;
    }
    return 0;
}

static int
SDL_BlendPoint_RGB(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
                   Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 2:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAW_SETPIXELXY2_BLEND_RGB(x, y);
            break;
        case SDL_BLENDMODE_ADD:
            DRAW_SETPIXELXY2_ADD_RGB(x, y);
            break;
        case SDL_BLENDMODE_MOD:
            DRAW_SETPIXELXY2_MOD_RGB(x, y);
            break;
        default:
            DRAW_SETPIXELXY2_RGB(x, y);
            break;
        }
        return 0;
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAW_SETPIXELXY4_BLEND_RGB(x, y);
            break;
        case SDL_BLENDMODE_ADD:
            DRAW_SETPIXELXY4_ADD_RGB(x, y);
            break;
        case SDL_BLENDMODE_MOD:
            DRAW_SETPIXELXY4_MOD_RGB(x, y);
            break;
        default:
            DRAW_SETPIXELXY4_RGB(x, y);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

static int
SDL_BlendPoint_RGBA(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
                    Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAW_SETPIXELXY4_BLEND_RGBA(x, y);
            break;
        case SDL_BLENDMODE_ADD:
            DRAW_SETPIXELXY4_ADD_RGBA(x, y);
            break;
        case SDL_BLENDMODE_MOD:
            DRAW_SETPIXELXY4_MOD_RGBA(x, y);
            break;
        default:
            DRAW_SETPIXELXY4_RGBA(x, y);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

int
SDL_BlendPoint(SDL_Surface * dst, int x, int y, int blendMode, Uint8 r,
               Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;

    /* This function doesn't work on surfaces < 8 bpp */
    if (dst->format->BitsPerPixel < 8) {
        SDL_SetError("SDL_BlendPoint(): Unsupported surface format");
        return (-1);
    }

    /* Perform clipping */
    if (x < dst->clip_rect.x || y < dst->clip_rect.y ||
        x >= (dst->clip_rect.x + dst->clip_rect.w) ||
        y >= (dst->clip_rect.y + dst->clip_rect.h)) {
        return 0;
    }

    if ((blendMode == SDL_BLENDMODE_BLEND)
        || (blendMode == SDL_BLENDMODE_ADD)) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    switch (fmt->BitsPerPixel) {
    case 15:
        switch (fmt->Rmask) {
        case 0x7C00:
            return SDL_BlendPoint_RGB555(dst, x, y, blendMode, r, g, b, a);
        }
        break;
    case 16:
        switch (fmt->Rmask) {
        case 0xF800:
            return SDL_BlendPoint_RGB565(dst, x, y, blendMode, r, g, b, a);
        }
        break;
    case 32:
        switch (fmt->Rmask) {
        case 0x00FF0000:
            if (!fmt->Amask) {
                return SDL_BlendPoint_RGB888(dst, x, y, blendMode, r, g, b,
                                             a);
            } else {
                return SDL_BlendPoint_ARGB8888(dst, x, y, blendMode, r, g, b,
                                               a);
            }
            break;
        }
    default:
        break;
    }

    if (!fmt->Amask) {
        return SDL_BlendPoint_RGB(dst, x, y, blendMode, r, g, b, a);
    } else {
        return SDL_BlendPoint_RGBA(dst, x, y, blendMode, r, g, b, a);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
