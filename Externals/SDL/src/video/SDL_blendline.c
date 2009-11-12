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
SDL_BlendLine_RGB555(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                     int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_BLEND_RGB555);
        break;
    case SDL_BLENDMODE_ADD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_ADD_RGB555);
        break;
    case SDL_BLENDMODE_MOD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_MOD_RGB555);
        break;
    default:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_RGB555);
        break;
    }
    return 0;
}

static int
SDL_BlendLine_RGB565(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                     int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_BLEND_RGB565);
        break;
    case SDL_BLENDMODE_ADD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_ADD_RGB565);
        break;
    case SDL_BLENDMODE_MOD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_MOD_RGB565);
        break;
    default:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_RGB565);
        break;
    }
    return 0;
}

static int
SDL_BlendLine_RGB888(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                     int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_BLEND_RGB888);
        break;
    case SDL_BLENDMODE_ADD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_ADD_RGB888);
        break;
    case SDL_BLENDMODE_MOD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_MOD_RGB888);
        break;
    default:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_RGB888);
        break;
    }
    return 0;
}

static int
SDL_BlendLine_ARGB8888(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                       int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_BLEND_ARGB8888);
        break;
    case SDL_BLENDMODE_ADD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_ADD_ARGB8888);
        break;
    case SDL_BLENDMODE_MOD:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_MOD_ARGB8888);
        break;
    default:
        DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY_ARGB8888);
        break;
    }
    return 0;
}

static int
SDL_BlendLine_RGB(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                  int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 2:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY2_BLEND_RGB);
            break;
        case SDL_BLENDMODE_ADD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY2_ADD_RGB);
            break;
        case SDL_BLENDMODE_MOD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY2_MOD_RGB);
            break;
        default:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY2_RGB);
            break;
        }
        return 0;
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_BLEND_RGB);
            break;
        case SDL_BLENDMODE_ADD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_ADD_RGB);
            break;
        case SDL_BLENDMODE_MOD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_MOD_RGB);
            break;
        default:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_RGB);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

static int
SDL_BlendLine_RGBA(SDL_Surface * dst, int x1, int y1, int x2, int y2,
                   int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_BLEND_RGBA);
            break;
        case SDL_BLENDMODE_ADD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_ADD_RGBA);
            break;
        case SDL_BLENDMODE_MOD:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_MOD_RGBA);
            break;
        default:
            DRAWLINE(x1, y1, x2, y2, DRAW_SETPIXELXY4_RGBA);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

int
SDL_BlendLine(SDL_Surface * dst, int x1, int y1, int x2, int y2,
              int blendMode, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;

    /* This function doesn't work on surfaces < 8 bpp */
    if (dst->format->BitsPerPixel < 8) {
        SDL_SetError("SDL_BlendLine(): Unsupported surface format");
        return (-1);
    }

    /* Perform clipping */
    if (!SDL_IntersectRectAndLine(&dst->clip_rect, &x1, &y1, &x2, &y2)) {
        return (0);
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
            return SDL_BlendLine_RGB555(dst, x1, y1, x2, y2, blendMode, r, g,
                                        b, a);
        }
        break;
    case 16:
        switch (fmt->Rmask) {
        case 0xF800:
            return SDL_BlendLine_RGB565(dst, x1, y1, x2, y2, blendMode, r, g,
                                        b, a);
        }
        break;
    case 32:
        switch (fmt->Rmask) {
        case 0x00FF0000:
            if (!fmt->Amask) {
                return SDL_BlendLine_RGB888(dst, x1, y1, x2, y2, blendMode, r,
                                            g, b, a);
            } else {
                return SDL_BlendLine_ARGB8888(dst, x1, y1, x2, y2, blendMode,
                                              r, g, b, a);
            }
            break;
        }
    default:
        break;
    }

    if (!fmt->Amask) {
        return SDL_BlendLine_RGB(dst, x1, y1, x2, y2, blendMode, r, g, b, a);
    } else {
        return SDL_BlendLine_RGBA(dst, x1, y1, x2, y2, blendMode, r, g, b, a);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
