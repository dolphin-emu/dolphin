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

#include "SDL_video.h"
#include "SDL_draw.h"

static int
SDL_BlendRect_RGB555(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB555);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB555);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB555);
        break;
    default:
        FILLRECT(Uint16, DRAW_SETPIXEL_RGB555);
        break;
    }
    return 0;
}

static int
SDL_BlendRect_RGB565(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB565);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB565);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB565);
        break;
    default:
        FILLRECT(Uint16, DRAW_SETPIXEL_RGB565);
        break;
    }
    return 0;
}

static int
SDL_BlendRect_RGB888(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGB888);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGB888);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGB888);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_RGB888);
        break;
    }
    return 0;
}

static int
SDL_BlendRect_ARGB8888(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                       Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_ARGB8888);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_ARGB8888);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_ARGB8888);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_ARGB8888);
        break;
    }
    return 0;
}

static int
SDL_BlendRect_RGB(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 2:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB);
            break;
        case SDL_BLENDMODE_ADD:
            FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB);
            break;
        case SDL_BLENDMODE_MOD:
            FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB);
            break;
        default:
            FILLRECT(Uint16, DRAW_SETPIXEL_RGB);
            break;
        }
        return 0;
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGB);
            break;
        case SDL_BLENDMODE_ADD:
            FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGB);
            break;
        case SDL_BLENDMODE_MOD:
            FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGB);
            break;
        default:
            FILLRECT(Uint32, DRAW_SETPIXEL_RGB);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

static int
SDL_BlendRect_RGBA(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode,
                   Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (fmt->BytesPerPixel) {
    case 4:
        switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGBA);
            break;
        case SDL_BLENDMODE_ADD:
            FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGBA);
            break;
        case SDL_BLENDMODE_MOD:
            FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGBA);
            break;
        default:
            FILLRECT(Uint32, DRAW_SETPIXEL_RGBA);
            break;
        }
        return 0;
    default:
        SDL_Unsupported();
        return -1;
    }
}

int
SDL_BlendRect(SDL_Surface * dst, SDL_Rect * dstrect, int blendMode, Uint8 r,
              Uint8 g, Uint8 b, Uint8 a)
{
    SDL_PixelFormat *fmt = dst->format;

    /* This function doesn't work on surfaces < 8 bpp */
    if (fmt->BitsPerPixel < 8) {
        SDL_SetError("SDL_BlendRect(): Unsupported surface format");
        return (-1);
    }

    /* If 'dstrect' == NULL, then fill the whole surface */
    if (dstrect) {
        /* Perform clipping */
        if (!SDL_IntersectRect(dstrect, &dst->clip_rect, dstrect)) {
            return (0);
        }
    } else {
        dstrect = &dst->clip_rect;
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
            return SDL_BlendRect_RGB555(dst, dstrect, blendMode, r, g, b, a);
        }
        break;
    case 16:
        switch (fmt->Rmask) {
        case 0xF800:
            return SDL_BlendRect_RGB565(dst, dstrect, blendMode, r, g, b, a);
        }
        break;
    case 32:
        switch (fmt->Rmask) {
        case 0x00FF0000:
            if (!fmt->Amask) {
                return SDL_BlendRect_RGB888(dst, dstrect, blendMode, r, g, b,
                                            a);
            } else {
                return SDL_BlendRect_ARGB8888(dst, dstrect, blendMode, r, g,
                                              b, a);
            }
            break;
        }
    default:
        break;
    }

    if (!fmt->Amask) {
        return SDL_BlendRect_RGB(dst, dstrect, blendMode, r, g, b, a);
    } else {
        return SDL_BlendRect_RGBA(dst, dstrect, blendMode, r, g, b, a);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
